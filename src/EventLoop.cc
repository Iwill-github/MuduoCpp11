#include "EventLoop.h"

#include "Logger.h"
#include "Poller.h"      // Poller的getDefaultPoller方法是在DefaultPoller中实现的

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>       // 智能指针


__thread EventLoop* t_loopInThisThread = nullptr;   // __thread 是c++中的一个线程局部存储关键字。防止一个线程创建多个 EventLoop

const int kPollTimeMs = 10000;

// 创建wakeupfd，用来notify唤醒subReactor，来处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);   // 创建一个eventfd
    if(evtfd < 0){
        LOG_FATAL("eventfd error:%d\n", errno);
    }

    return evtfd;
}

/*
函数功能：
    这里我们将由wakeupFd_创建出来的Channel对象，并将其地址存入wakeupChannel_中，
    调用了Channel中的setReadCallback函数设置了它的读操作回调函数handleRead，
    并调用了Channel中的enableReading函数设置了事件类型为EPOLLIN。
*/
EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::getTid())
    , poller_(Poller::getDefaultPoller(this))       // 传入了EventLoop类的对象 loop
    , wakeupFd_(createEventfd())                   
    , wakeupChannel_(new Channel(this, wakeupFd_))  // 将 当前loop 和 wakeupFd_ 打包成 wakeupChannel_
{
    LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
    if(t_loopInThisThread){                         // 该线程已存在一个 EventLoop
        LOG_FATAL("Another EventLoop %p exists in thread %d\n", t_loopInThisThread, threadId_);
    }else{                                          // 当前线程第一次创建你 EventLoop对象
        t_loopInThisThread = this;
    }

    // 设置wakeupfd_的事件类型，以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    // 并启用它的读事件（使其关注自身 sokedfd 的读事件）
    wakeupChannel_->enableReading();
}


EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();   // 取消对所有事件的监听
    wakeupChannel_->remove();       // 将channel从poller中移除
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}


/*
函数功能：
    调用底层Poller，开启事件分发器。

函数内容：
    调用poller的poll方法底层调用epoll_wait把活跃Channel都放到activeChannels_容器中，
    然后进行遍历执行活跃channel的handleEvent判断具体事件类型，执行相应回调函数。
*/
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while(!quit_){
        activateChannles_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activateChannles_);   // 监听两类fd：client的fd，wakeup的fd（问题：这两个fd是何时，如何注册到poller中的？）
        for(Channel* channel: activateChannles_){
            channel->handleEvent(pollReturnTime_);  // 触发回调（该回调函数具体执行的功能，该功能需要再创建channel时候注册）
        }

        // 执行待处理的函数对象（functors），这些functors可能是事件循环外部提交给事件循环线程的任务，
        // 通过这种方式实现线程安全的任务队列处理。
        // 这有助于扩展事件循环的功能，使其不仅能处理I/O事件，还能处理定时任务、延后执行的任务等
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}   


/* 退出事件循环：1. loop在自己线程中调用quit() 2. 在非loop的线程中调用quit()
                        mainLoop
    subLoop1    subLoop2    subLoop3    subLoop3 ...
*/
void EventLoop::quit(){
    quit_ = true;
    if(!isInLoopThread()){  // 在其他线程中调用quit，比如在subloop中，调用mainloop的quit
        wakeup();   // 唤醒要退出的loop
    }
}                  


// 在当前loop执行cb，这里的回调函数cb，其实都是和channel相关的
void EventLoop::runInLoop(Functor cb){
    if (isInLoopThread()){  // 在当前的loop线程中，执行cb
        cb();
    }else{                  // 在非当前loop线程中，执行cb，就需要唤醒其他loop线程，执行cb
        queueInLoop(cb);
    }
}


// 把cb放入队列中，唤醒loop所在的线程后，再执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }

    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_ 表示 doPendingFunctors中回调还没执行完，loop中又阻塞在poll上，
    // 此时，也要唤醒loop

    if(!isInLoopThread() || callingPendingFunctors_){   
        wakeup();   // 唤醒loop所在线程
    }
} 


// 读事件的回调函数
void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);    // 读出wakeupFd_中的数据，即wakeupChannel_触发后的事件
    if(n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8\n", n);
    }
}


// mainLoop用的。用来唤醒loop所在的线程, 向 wakeupfd_ 写一个数据, wakeupChannel_ 就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n", n);
    }
}                  


// channel 调用的 loop.updateChannel 间接的调用 poll.updateChannel 来更改在 poll 上与 channel 相关的fd的事件
void EventLoop::updateChannel(Channel* Channel){
    poller_->updateChannel(Channel);
} 


// channel 调用的 loop.removeChannel 间接的调用 poll.removeChannel 来删除在 poll 上与 channel 相关的fd的事件
void EventLoop::removeChannel(Channel* Channel){
    poller_->removeChannel(Channel);
}           


bool EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}


// 执行回调函数。注意，回调是在vector容器中存放的，谁可以在这里写回调？TcpServer
void EventLoop::doPendingFunctors(){
    std::vector<Functor> fucntors;  
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        fucntors.swap(pendingFunctors_);
    }

    // 使用局部变量，即使没有执行完回调函数，也不妨碍mainLoop继续向pendingFunctors_写回调
    for(const Functor& functor : fucntors){
        functor();  // 执行当前 loop 需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}


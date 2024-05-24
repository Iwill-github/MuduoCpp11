#include "Channel.h"

#include <sys/epoll.h>

#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;  // 该变量表示文件描述符可读事件的掩码，即当文件描述符上发生可读事件时，会触发该掩码对应的事件。其中，EPOLLIN表示可读事件，EPOLLPRI表示紧急可读事件。 
const int Channel::kWriteEvent = EPOLLOUT;           // 可写事件


// EventLoop: ChannelList Poller
Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop)
    , fd_(fd)
    , events_(0)
    , revents_(0)
    , index_(-1)
    , tied_(false)
{ 
}


Channel::~Channel(){}


/*
    强、弱智能指针
        1. 强智能指针持有对象所有权并负责对象的生命周期管理，
        2. 而弱智能指针不持有所有权，仅用于观察和安全地访问可能已不存在的对
    循环引用：
        1. 如果Channel类直接持有另一个对象的shared_ptr，而那个对象反过来又以某种方式持有Channel的shared_ptr，
            那么即使所有外部对这两个对象的引用都消失了，它们仍然会因为互相持有对方的强引用而无法被释放。
        2. 使用weak_ptr断开了这个循环，因为weak_ptr不增加被观察对象的引用计数。
    什么时候调用该函数？
        1. 一个TcpConnection新连接创建的时候被调用。
           TcpConnection => Channel => poller
        2. TcpConnection对象被remove时，Channel对象调用相应的回调产生的结果就会是未知的。（因为Channel对象中的回调函数是在TcpConnection中绑定注册的）
*/
void Channel::setTie(const std::shared_ptr<void>& obj){
    tie_ = obj;
    tied_ = true;
}


/*
    当改变channel代表的文件描述符fd的events事件后，update负责在poller里面更改fd相应的事件（通过系统调用 epoll_ctl）
    因为 EventLoop 包含 ChannelList 和 Poller，ChannelList 和 Poller 是两个独立的组件。
*/
void Channel::update(){
    // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}


void Channel::remove(){
    // 在channel所属的EventLoop中，把当前的channel删除掉
    loop_->removeChannel(this);
}


/*
    事件处理函数:fd得到poller通知后，处理事件的
*/ 
void Channel::handleEvent(Timestamp receiveTime){
    if(tied_){
        // std::weak_ptr的lock()方法用于检查所观察的对象是否仍然存在，如果存在，则返回一个有效的std::shared_ptr，
        // 否则返回一个空的std::shared_ptr。这是std::weak_ptr的一个重要功能，
        // 因为它允许你在对象可能已经被删除的情况下安全地访问对象。
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(receiveTime);
        }
    }else{
        handleEventWithGuard(receiveTime);
    }
}



/*
    根据poller 通知的channel发生的具体事件，由channel负责调用具体的回调操作
*/ 
void Channel::handleEventWithGuard(Timestamp receiveTime){
    LOG_INFO("channel handleEvent revents %d\n", revents_);

    // 通常一个套接字不会同时既是挂起的（HUP）又可读（IN）。
    // 如果发生了这种情况，可能表示一种特殊的情况，比如套接字在关闭过程中，但仍有数据待读取。
    if( (revents_ & EPOLLHUP) && (revents_ & EPOLLIN) ){    
        if( closeCallback_ ){
            closeCallback_();
        }
    }

    if( revents_ & EPOLLERR ){
        if( errorCallback_ ){
            errorCallback_();
        }
    }

    if( revents_ & EPOLLIN ){
        if( readCallback_ ){
            readCallback_( receiveTime );
        }
    }

    if( revents_ & EPOLLOUT ){
        if( writeCallback_ ){
            writeCallback_();
        }
    }
}


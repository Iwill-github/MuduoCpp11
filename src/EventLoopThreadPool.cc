#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"



EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0)
{ }


// 注意：新线程中创建的loop，是在栈区创建的，不需要手动释放，所以析构函数不需要做事情
EventLoopThreadPool::~EventLoopThreadPool(){ }



void EventLoopThreadPool::start(const ThreadInitCallback& cb){
    started_ = true;

    for(int i = 0; i < numThreads_; ++i){
        char buf[name_.size() + 32];    // 线程名的命名规则
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);
        threads_.emplace_back(std::unique_ptr<EventLoopThread>(t));
        loops_.emplace_back(t->startLoop());    // 底层创建线程，绑定一个新的EventLoop，并返回该loop的地址。参看 EventLoopThread.cc
    }

    if(numThreads_ == 0 && cb){   // 整个服务端只有一个线程，运行着baseloop，且用户设置的为baseloop设置了回调函数（该回调函数为baseloop的线程初始化函数）
        cb(baseLoop_);
    }
}


/*
如果工作在多线程中，baseloop_ 默认以轮询的方式分配channel给subloop
    I/O线程，运行的 baseloop_ 处理用户连接事件
    工作线程，运行的 subloop 处理已连接用户的I/O事件
*/ 
EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop *loop = baseLoop_;

    if(!loops_.empty()){        // 如果创建了独立的事件循环线程，通过轮询获取下一个处理事件的loop
        loop = loops_[next_];
        next_ = (next_ + 1) % loops_.size();
    }

    return loop;                // 如果没有创建过独立的事件循环线程，则返回baseLoop_
}



std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        return std::vector<EventLoop*>(1, baseLoop_);
    }else{
       return loops_;
    }
}


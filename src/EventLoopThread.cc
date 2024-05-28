#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb)
{ }


EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if(loop_ != nullptr){
        loop_->quit();      // 线程退出，线程执行的事件循环也要退出
        thread_.join();     // 线程退出
    }
}


/*
    函数功能：
        开启一个新线程，单独执行一个loop对象，并返回该loop对象的地址
*/
EventLoop* EventLoopThread::startLoop(){
    thread_.start();                // 开启一个新线程，执行 threadFunc

    EventLoop* loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr){
            cond_.wait(lock);       // 等待 loop_ 变量被赋值，此时会短暂的释放锁，被唤醒后重新获取锁
        }
        loop = loop_;
    }

    return loop;
}


// 该方法，是在单独的新线程中执行的
void EventLoopThread::threadFunc(){
    EventLoop loop;             // 创建一个 EventLoop，和上面的线程是一一对应的，one loop per thread
    
    if(callback_){
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();
    
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}

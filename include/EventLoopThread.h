#pragma once

#include "noncopyable.h"
#include "EventLoop.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>


/*
    EventLoopThread 类的功能梳理：
        通过 startLoop() 开启一个新线程，在新线程中执行 线程初始化回调 和 事件循环
*/
class EventLoopThread: public noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), 
                        const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;                   // 事件循环
    bool exiting_;                      // 退出标志
    Thread thread_;                     // 线程
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;       // 线程初始化回调
};

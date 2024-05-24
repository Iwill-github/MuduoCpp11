#pragma once

#include <functional>
#include <vector>
#include <atomic>   // atomic_bool
#include <memory>   // unique_ptr
#include <mutex>    // mutex

#include "noncopyable.h"
#include "Channel.h"
#include "Poller.h"
#include "CurrentThread.h"

class Channel;
class Poller;

/* 
    EventLoop 主要成员变量：
        poller_             # 每个EventLoop事件循环包含一个io复用Poller,每个 Poller监听多个Channel
        wakeupFd_
        wakeupChannel_

    EventLoop 类的功能梳理：
        每一个事件循环均需要做一下事情：
            1. 通过io复用的poller监听 多个Channel的文件描述符，将发生事件文件描述符对应的channel写回 activateChannles_
            2. 执行activateChannles_中channel的回调操作（注意，回调操作是在Channel中指定的，在 loop中执行的）

        每一个事件循环包含一个特殊 Channel，涉及 wakeupFd_、wakeupChannel_
            1. wakeupFd_ 是一个用于唤醒当前事件循环的 eventfd
                为什么需要唤醒？因为当前时间循环正在poll阻塞，而其他线程需要唤醒当前事件循环，从而执行更新监听的回调操作。
            2. wakeupChannel_ 是封装了 wakeupFd_ 和 EventLoop 对象 Channel。
                该channel的回调函数是 wakeup()，唤醒当前事件循环。

    问题：
        1. poller 需要监听的一些文件描述符，是何时注册到 epollfd 的？
            xxxxxx
        2. channel 的回调函数，是什么时候被注册的？
            一般是在创建channel的时候，就需要注册channel的事件回调函数。
*/ 
class EventLoop: public noncopyable{
public:
    // Functor 就是一个类型别名，它代表了能够接受零个参数并且不返回任何值的可调用对象，
    // 比如无参函数、Lambda 表达式、成员函数指针等
    using Functor = std::function<void()>;      

    EventLoop();
    ~EventLoop();

    void loop();                    // 开启事件循环
    void quit();                    // 退出事件循环
    
    Timestamp pollReturnTime() const { return pollReturnTime_; }
    
    void runInLoop(Functor cb);     // 在当前loop执行cb
    void queueInLoop(Functor cb);   // 把cb放入队列中，唤醒loop所在的线程后，再执行cb

    void wakeup();                  // 用来唤醒loop所在的线程
    
    void updateChannel(Channel* Channel);           // channel 调用的 loop.updateChannel 间接的调用 poll.updateChannel 来更改在 poll 上与 channel 相关的fd的事件
    void removeChannel(Channel* Channel);           // channel 调用的 loop.removeChannel 间接的调用 poll.removeChannel 来删除在 poll 上与 channel 相关的fd的事件
    
    bool hasChannel(Channel* channel);

    bool isInLoopThread() const{ return threadId_ == CurrentThread::getTid(); };    // loop 对象在创建它的线程中

private:
    void handleRead();              // wakeup()中调用
    void doPendingFunctors();       // 执行回调函数。注意，回调是在vector容器中存放的

private:
    using ChannelList = std::vector<Channel*>;
    
    std::atomic_bool looping_;                  // 是否在事件循环中
    std::atomic_bool quit_;                     // 是否退出了事件循环
    std::atomic_bool callingPendingFunctors_;   // 是否有需要执行的回调操作

    const pid_t threadId_;                      // 所在线程的id
    Timestamp pollReturnTime_;                  // poller 返回 发生事件的channels 的时间点
    std::unique_ptr<Poller> poller_;            // poller对象的智能指针
    
    int wakeupFd_;                              // 该loop的唤醒事件的文件描述符 eventfd
    std::unique_ptr<Channel> wakeupChannel_;    // 封装了 wakeupFd_ 的channel对象指针（监听wakeupFd_的读事件，执行对应的回调）

    ChannelList activateChannles_;              // 发生事件的 channel对象指针 列表

    std::vector<Functor> pendingFunctors_;      // 需要执行的回调操作
    std::mutex mutex_;                          // 互斥锁，用于保证上述vector容器的线程安全
};

 
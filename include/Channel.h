#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>           // 智能指针相关的头文件

class EventLoop; 
class Timestamp;


/*
    1. Channel 类的功能梳理（Demultiplex）：
        * 在Linux网络编程中，Channel类通常用于表示一个文件描述符（如套接字），封装了 
            socketfd    需要向poller上注册的文件描述符、
            event       socketfd 上的感兴趣事件，如 EPOLLIN，EPOLLOUT事件、
            revents     poller 返回的触发事件、
            callbacks   一些列事件对应的回调操作、
            loop        该channel所在的事件循环、
        * 描述叙述：
            muduo库中，无论是对服务端监听的fd(listenfd)，还是客户端连接成功后返回的通信fd(connfd)，
            都会封装上述信息作为一个channel(acceptorChannel、connectionChannel)，然后设置相应的感兴趣事件，同时注册到poller上。
        * 如何更改 channel 对应 fd 的感兴趣事件？
            Channel => EventLoop => Poller

    2. EventLoop、ChannnelList、Poller之间的关系   (它们在Reactor模型上对应 Demultiplex)
        1. 1个EventLoop是1个事件循环，1个事件循环是跑在一个线程里
        2. 1个EventLoop对应1个Poller，1个Poller对应多个Channel即ChannelList

*/
class Channel: public noncopyable{
public:
    // typedef std::function<void()> EventCallback;     // 定义了一个类型别名EventCallback，它代表了一个无参数并且返回值为void的函数类型。<void()>表示被封装的函数没有输入参数，并且没有返回值。
    using EventCallback = std::function<void()>;                
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void setReadCallback(ReadEventCallback cb){ readCallback_ = std::move(cb); }     // 设置回调函数对象。readCallback_是一个对象，所以此处调用的拷贝构造函数
    void setWriteCallback(EventCallback cb){ writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb){ closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb){ errorCallback_ = std::move(cb); }

    void setTie(const std::shared_ptr<void>&);     // 当channel被手动remove时，防止channel还在执行回调操作

    int getFd() const { return fd_; }
    int getEvents() const { return events_; }
    int getIndex() { return index_; }
    EventLoop* get_ownerLoop() { return loop_; }    // one loop per thread
    void update();          // 通过channel所属的EventLoop，调用poller的相应方法，注册fd的events事件
    void remove();          // 在channel所属的EventLoop中，把当前的channel删除掉

    void setRevents(int revt){ revents_ = revt; }
    void setIndex(int idx){ index_ = idx; }


    // 设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }   // 设置fd相应的事件状态
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }  
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }  // 当前感兴趣事件是否为空，即不对任何事件感兴趣
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    void handleEvent(Timestamp receiveTime);            // 事件处理函数。fd得到poller通知后，处理事件的
    void handleEventWithGuard(Timestamp receiveTime);   // 根据你具体接收到的事件，执行相应的事件处理函数

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

private:
    EventLoop *loop_;       // 事件循环（channel为什么要依赖 loop 呢？因为channel需要通过loop和poller通信）
    const int fd_;          // fd, channel对应的文件描述符，即poller需要监听的对象
    int events_;            // fd 感兴趣的事件，poller的ChannelList对象的元素
    int revents_;           // fd 发生的事件，参看poller.poll
    int index_;             // 表示当前 channel 的状态，具体定义参看EpoolPoller.cc
    
    /*
    强、弱智能指针
        1. 强智能指针持有对象所有权并负责对象的生命周期管理，
        2. 而弱智能指针不持有所有权，仅用于观察和安全地访问可能已不存在的对
    循环引用：
        1. 如果Channel类直接持有另一个对象的shared_ptr，而那个对象反过来又以某种方式持有Channel的shared_ptr，
            那么即使所有外部对这两个对象的引用都消失了，它们仍然会因为互相持有对方的强引用而无法被释放。
        2. 使用weak_ptr断开了这个循环，因为weak_ptr不增加被观察对象的引用计数。
    */
    std::weak_ptr<void> tie_;
    bool tied_;

    // std::function<void(Timestamp)> readCallback_;
    ReadEventCallback readCallback_;        // 因为Channel通道里，可以获知fd最终发生的具体事件revents，所以它负责调用具体事件的回调操作
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
    

};



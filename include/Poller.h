#pragma once

#include <vector>
#include <unordered_map>

#include "Channel.h"
#include "noncopyable.h"
#include "Timestamp.h"


/*
    muduo 库中多路事件分发器的核心IO复用模块
*/ 
class Poller{
public:
    using ChannelList = std::vector<Channel*>;              // c++11 新特性，using 声明，相当于 typedef
    
    Poller(){owerLoop_ = nullptr;};
    Poller(EventLoop *loop);
    virtual ~Poller() = default;                            // 让编译器生成该虚析构函数的默认实现。

    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0; // 给所有IO复用保留统一的接口
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    bool hasChannel(Channel* channel) const;                // 判断参数channel是否在当前Poller的ChannelList中 
    static Poller* getDefaultPoller(EventLoop *loop);       // 该方法实现不写在Poller.cc文件中！！！ 因为基类中不建议使用派生类对象

protected:
    // [sockfd, sockfd所属的channel通道]
    using ChannelMap = std::unordered_map<int, Channel*>;   
    ChannelMap channels_;   // 定义Poller所属的事件循环的 ChannelList

private:
    EventLoop *owerLoop_;   // 定义Poller所属的事件循环

};


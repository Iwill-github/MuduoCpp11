#pragma once

#include <vector>
#include <sys/epoll.h>

#include "Poller.h"


class Channel;

/*
    1. 该类的功能，主要围绕epoll的使用编写： 
        epoll_create     对应于 EPollPoller、~EPollPoller
        epoll_ctl        对应于 updateChannel、removeChannel
        epoll_wait       对应于 poll

    2. 该类的功能：
        ----------------------------------------------------------------         
                        EventLoop   =>    poller.poll
              ChannelList               Poller（监听各个channel上的事件）
        （用于保存发生事件的连接）     ChannelMap <fd, channel*> 继承于Poller（需要监听的channel需要注册到poller上的含义，即写入到该map中）  
        ----------------------------------------------------------------
        EventLoop中，调用了poller.poll,通过 epoll_wait, 将监听到发生事件的channel，告知到EventLoop

    3. 该类的主要成员解释：  
        epollfd_ 对应着一个 events_。
        events_是一个epoll_event数组，每一个epoll_event对应着一个channel，表示该channel对事件epoll_evnet 感兴趣。  
*/
class EPollPoller : public Poller{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;

    // 重写基类Poller的抽象方法
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;    // 将发生事件的channel告知EventLoop

private:
    static const int kInitEventListSize = 16;

    // 其他私有的方法
    void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;  // 填写活跃的连接。const表示承诺不会修改EpollPoller对象的任何成员
    void update(int operation, Channel *channel);       // 更新 epollfd_ 

    using EventList = std::vector<epoll_event>;

    int epollfd_;           // epoll_wait 的第一个参数。epoll文件描述符，代表着一颗红黑树数据结构
    EventList events_;      // epoll_wait 的第二个参数。将 epollfd_ 监听到的所有事件封装到events_中，每一个事件均绑定着对应的channel
};


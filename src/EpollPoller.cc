#include "EpollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <sys/errno.h>
#include <unistd.h>     // close
#include <string.h>     // memset


// channel的成员 index_ = 1
const int kNew = -1;        // 表示channel 还未添加到poller中（channel表示文件描述符，添加到poller中表示注册感兴趣的事件）
const int kAdded = 1;       // 表示channel 已经添加到poller中
const int kDeleted = 2;     // 表示channel 已经从poller中删除


/*
    笔记：
        1. epoll是Linux I/O多路复用的一种机制，它允许程序等待多个I/O事件，而无需为每个事件创建单独的线程。
        2. epoll_create1(): 这是Linux内核提供的系统调用，用于创建一个epoll文件描述符（epollfd）。
        3. EPOLL_CLOEXEC表示在创建的epoll文件描述符上设置FD_CLOEXEC标志。这意味着当进程执行exec家族的系统调用时，这个epoll文件描述符会被自动关闭
*/
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop)
    , epollfd_(::epoll_create1(EPOLL_CLOEXEC))  
    , events_(kInitEventListSize)       // EPollPoller会将发生事件的fd的event放进vector序列中
{
    if(epollfd_ < 0){
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}


EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}


/*
    ---------------------------------------          
            EventLoop
    ChannelList          Poller
                  ChannelMap <fd, channel*>
    ---------------------------------------
    函数功能：
        将channel添加（注册）到poller中（让poller监听该channel所关注的事件）
    笔记：
        1. EventLoop包含 Channel、Poller两个独立的组件。当Channel类对象想要更新对应文件描述符所关心的事件时，
           不可以直接调用Poller的方法，需要通过EventLoop对象方法调用Poller对象方法
        2. 即，Channell类对象的update、remove方法中，调用了EventLoop对象方法中的 updateChannel、removeChannel方法
           EventLoop对象方法中的 updateChannel、removeChannel方法，调用了EPollPoller对象的 updateChannel、removeChannel方法
*/ 
void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->getIndex();
    LOG_INFO("fd=%d events=%d index=%d\n", channel->getFd(), channel->getEvents(), index);

    // 如果channel 没有在poller中，则添加进去
    if(index == kNew || index == kDeleted){
        if( index == kNew ){
            int fd = channel->getFd();
            channels_[fd] = channel;
        }

        channel->setIndex(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }else{  // channel 已经在poller中注册过了
        int fd = channel->getFd();
        if(channel->isNoneEvent()){     // 如果channel对任何事件都不感兴趣，则从poller中删除
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(kDeleted);
        }else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}


/*
    ---------------------------------------          
              EventLoop
    ChannelList          Poller
                   ChannelMap <fd, channel*>
    ---------------------------------------
    函数功能：
        从Poller中删除channel（表示不在关心这个channel上的事件），即，将channel从 ChannelMap中remove掉。
        但是依然存在在ChannelList中
*/
void EPollPoller::removeChannel(Channel *channel){
    int fd = channel->getFd();          // 获取channel的fd
    int index = channel->getIndex();    // 获取channel在Poller的状态，只是奇怪这个命名陈硕大神为什么不用state
    LOG_INFO("fd=%d index=%d\n", fd, index);

    channels_.erase(fd);
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(kNew);
}


/*
    ---------------------------------------          
              EventLoop   =>   poller.poll
    ChannelList          Poller
                   ChannelMap <fd, channel*>
    ---------------------------------------
    函数功能：
        通过EventLoop，调用了poller.poll方法，通过 epoll_wait, 将监听到发生事件的channel，告知到EventLoop
*/
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels){
    // 实际上应该用 LOG_DEBUG 输出日志更为合理，因为在高并发的情况下，LOG_INFO可能会影响poll的性能
    LOG_INFO("fd total count:%lu\n", channels_.size());

    // &*events_.begin() 表示vector容器的起始地址
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs); // 最多只会返回events_.size()个事件
    int saveErrno = errno;  // 高并发时，可能多个epoll_wait都会出错，均会写errno，所以先用局部变量存储errno。（注意，依然可能会出问题）
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);  // 将监听到发生事件的活跃连接，写入到ChannelList（EventLoop的另一个组件）中
        if(numEvents == events_.size()){
            events_.resize( events_.size()*2 );
        }
    }else if(numEvents == 0){
        LOG_DEBUG("timeout! \n");
    }else{
        if(saveErrno != EINTR){
            errno = saveErrno;      // 防止在处理上述逻辑过程中，其他loop有可能也发生错误修改了全局的errno的值。所以使用了局部变量暂存了errno
            LOG_ERROR("errno = %d\n", saveErrno);
        }
    }

    return Timestamp::now();
}


/*
    函数功能：
        将监听到发生事件的活跃连接，写入到 activeChannels 中
    笔记：
        events_[i] 表示的事件，的.data.ptr属性绑定了Channel对象（参看 EPollPoller::update 方法）。
        故，可以通过events_[i]获取相应的Channel对象，将该对象添加到活跃的ChannelList中。
*/
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for(int i = 0; i < numEvents; ++i){
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);     // EventLoop就拿到了它的Poller给它返回的所有发生事件的channel列表
    }
} 


/*
    函数功能：更新channel通道所感兴趣的事件。  epoll_ctl add/mod/del
*/ 
void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    memset(&event, 0, sizeof event);

    int fd = channel->getFd();

    event.events = channel->getEvents();
    event.data.fd = fd;
    event.data.ptr = channel;   // 将channel保存到epoll_event中

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n", errno);       // 如果没删除成功，影响也不是致命的
        }else{
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}      


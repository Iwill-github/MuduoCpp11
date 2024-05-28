#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

/*
    Acceptor 类功能梳理：
        Acceptor 是对 listenfd 的封装，主要是为了返回 connfd，同时 TcpServer => Acceptor => channel
        1. 之所以依赖 loop_ ，是因为其主要运行在 baseloop 中，即 mainReactor 中。
        2. 封装bind、listen操作，设置 listenfd 为监听状态；
        3. 设置 listenfd 对应的 acceptChannel_ 的事件回调（ReadCallback）和 handleRead 函数绑定
           在handleRead函数中调用的 newConnectionCallback_ 回调，该回调实际上是在 TcpServer 中进行绑定注册的。
           setReadCallback => handleRead => newConectionCallback
           acceptor.setNewConnectionCallback => TcpServer::newConnection
*/
class Acceptor: public noncopyable{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
    
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb){ newConnectionCallback_ = cb; }
    
    bool getListenning() const { return listenning_; }
    
    void listen();

private:
    void handleRead();

    EventLoop *loop_;                                   // Acceptor 用的就是用户定义的 baseloop，也称作 mainloop
    Socket acceptSocket_;                               // listenfd
    Channel acceptChannel_;                             // listenfd 对应的 channel
    NewConnectionCallback newConnectionCallback_;       // 新连接回调，该回调是用 TcpServer 设置的
    bool listenning_;                                   // 是否正在监听
};


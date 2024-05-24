#pragma once

#include "noncopyable.h"

class InetAddress;


/*
    Socket 类功能梳理：
        负责套接字的创建、绑定、监听、接收（返回客户端地址）、关闭等操作
*/
class Socket: public noncopyable {
public:
    explicit Socket(int sockfd)             // 防止隐式类型转换
        : sockfd_(sockfd)
    {}
    ~Socket();

    void bindAddress(const InetAddress &localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();
    
    int getFd() const { return sockfd_; }   // 只读的方法，所以定义为 常函数
    
    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReuserPort(bool on);
    void setKeepAlive(bool on);
    
private:
    const int sockfd_;

};

#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>         // close
#include <sys/types.h>      // bind等
#include <sys/socket.h>     // bind等
#include <string.h>         // memset
#include <netinet/tcp.h>    // TCP_NODELAY


Socket::~Socket(){
    close(sockfd_);
}


// 将一个套接字 sockfd_ 绑定到指定的网络地址
void Socket::bindAddress(const InetAddress &localaddr){
    if(0 != ::bind(sockfd_, (sockaddr*)localaddr.getsockAddr(), sizeof(sockaddr_in))){    // bind函数通常用于将一个套接字 sockfd_ 绑定到指定的网络地址
        LOG_FATAL("bind sockfd_:%d fail\n", sockfd_);
    }
}


/*
函数功能：
    使套接字进入监听状态，准备接受连接请求。
其他解释：
    函数内部调用了全局作用域的::listen()函数，参数为套接字文件描述符sockfd_和最大连接队列长度1024。
    如果监听失败，则输出日志信息。
*/ 
void Socket::listen(){
    if(0 != ::listen(sockfd_, 1024)){   // :: 全局作用域，以免和自定义的方法冲突
        LOG_FATAL("listen sockfd_:%d fail\n", sockfd_);
    }
}


/*
函数功能：
    该行代码的功能是从sockfd_表示的套接字上接受一个连接请求，
    如果连接成功，通过指针设置了连接的客户端地址，并返回处理该连接的套接字 conn
    如果连接失败，返回-1
其他解释：
    这里使用了全局作用域的::accept()函数以避免与可能存在的类内同名方法冲突。
    它会阻塞等待直到有一个客户端连接到服务器。
    当成功接受一个连接时，会返回一个新的文件描述符connfd用于与这个客户端通信。
*/
int Socket::accept(InetAddress* peeraddr){

    sockaddr_in addr;
    socklen_t len = sizeof addr;    // socklen_t是sockaddr_in结构体的大小，用于保存地址长度
    memset(&addr, 0, sizeof addr);

    // int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len); 
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK); 
    if(connfd >= 0){
        peeraddr->setSockAddr(addr);
    }

    return connfd;
}


/*
函数功能：
    尝试关闭写通道，如果操作失败，则记录错误日志
*/
void Socket::shutdownWrite(){
    if(::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERROR("shutdownWrite error:%d\n", errno);
    }
}


/*
函数功能：
    setsockopt函数来设置套接字选项
其他解释：
    IPPROTO_TCP     表示我们正在处理TCP协议
    TCP_NODELAY     设置TCP套接字选项以禁用Nagle算法，从而减少小数据包的延迟
*/
void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    // setsockopt函数来设置套接字选项。
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}



void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    // setsockopt函数来设置套接字选项。
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}



void Socket::setReuserPort(bool on){
    int optval = on ? 1 : 0;
    // setsockopt函数来设置套接字选项。
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
}



void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    // setsockopt函数来设置套接字选项。
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}


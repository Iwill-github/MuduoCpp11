#include "Acceptor.h"

#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <unistd.h>


// 创建非阻塞的 socket
static int createNonblocking(){
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0){
        LOG_FATAL("listen socket create err:%d\n", errno);
    }

    return sockfd;
}


Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())            // 创建 socket
    , acceptChannel_(loop, acceptSocket_.getFd())   // channel为什么要依赖loop？因为channel需要向poller注册事件，这一操作是通过loop（包括channel、poller）来完成的
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReuserPort(true);
    acceptSocket_.bindAddress(listenAddr);          // 绑定 socket
    /*
        TcpServer::start() 方法调用 Acceptor.listen() 方法来监听 socket  
        当有新用户连接时，acceptChannel_ 会执行现在注册的回调（注意，该回调会在subloop中执行）
    */      
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}


Acceptor::~Acceptor(){
    acceptChannel_.disableAll();    // 不在需要 poller 关注其读写事件了
    acceptChannel_.remove();        // 从poller 中删除该channel
}


// 设置套接字进入监听状态
void Acceptor::listen(){
    listenning_ = true;
    acceptSocket_.listen();             // 设置套接字进入监听状态, 封装了 listen 函数相关
    acceptChannel_.enableReading();     // 设置 acceptChannel_ 感兴趣的事件为读事件，同时将listenfd 添加到 poller 中（channel => loop => poll）
                                        // 一旦有读事件，发生就会执行 acceptChannel_ 的读事件回调（具体操作需要 TcpServer 指定）。
}


/*
函数功能：
    1. 当有客户端连接时，acceptSocket_.accept(&peerAddr) 返回客户端的 sockfd，调用 newConnectionCallback_ 函数，进行channel封装，subloop分发
    2. newConnectionCallback_ 函数是 TcpServer 进行注册的绑定回调函数
*/ 
void Acceptor::handleRead(){
    InetAddress peerAddr;   // 客户端地址
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);   // 调用回调函数（TcpServer中注册），轮询找到subloop，唤醒subloop，分发当前新客户端的channel
        }else{
            ::close(connfd);
        }
    }else{
        LOG_ERROR("Acceptor::handleRead:%d \n", errno);
        if(errno == EMFILE){
            LOG_ERROR("sockfd reached limit:%d \n", errno);
        }
    }
}


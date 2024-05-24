#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <functional>   // placeholders 命名空间
#include <string.h>     // memset、bzero


EventLoop* CheckLoopNotNull(EventLoop* loop){
    if (loop == nullptr){
        LOG_FATAL("mainLoop is null! errno:%d \n", errno);
    }
    
    return loop;
}


/*
TcpServer对象构造：
    acceptor_
        1. acceptor_ 创建的时候，会针对 listenfd 创建socket、绑定socket、创建 socket对应的 acceptChannel，同时将 读事件回调handleRead 注册到 acceptChannel 上;
        2. acceptChannel 上有读事件发生时，会调用 handleRead 函数，返回一个 connfd;
        3. acceptChannel 上有读事件发生时，同时调用 newConnectionCallback 回调函数（该回调函数通过 acceptor_.setNewConnectionCallback() 来设置，该回调运行在mainloop中，用于 xxx xxx）
        4. 当调用 acceptor_.listen() 时，会设置套接字为监听状态、设置 acceptChanenl感兴趣事件为读事件、！同时将 listenfd 添加到 poller 中（channel => loop => poll）

    threadPool_
        1. 

*/
TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
    : loop_(CheckLoopNotNull(loop))     // baseloop
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_( new Acceptor(loop, listenAddr, option == kReusePort) )   
    , threadPool_(new EventLoopThreadPool(loop, name_)) 
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    // 绑定回调 acceptor_ 的新用户连接回调。当有新用户连接时，会执行 TcpServer::newConnection（轮询，分发操作）
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}


TcpServer::~TcpServer(){
    for(auto& item : connections_){
        // 这个局部的强 shared_ptr 智能指针对象，出右括号，可以自动释放new出来的TcpConnection对象资源
        // 如果直接item.second.reset()，释放了new出来的TcpConnection对象资源，则无法再调用TcpConnection::connectDestroyed
        TcpConnectionPtr conn(item.second);     
        item.second.reset();

        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}


// 设置线程数量，即设置subloop的个数
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}


// 开启服务器监听
void TcpServer::start(){
    if(started_++ == 0){                                                    // 防止一个 TcpServer 对象被 start 多次
        threadPool_->start(threadInitCallback_);                            // 启动底层 looop 线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));    // 执行 Acceptor::listen 回调
    }
}


/*
    1. 主线程（mainloop）根据轮询算法使 ioloop 指向一个subloop，把当前connfd封装成 channel分发给subloop
            如果ioloop指向的subloop就是 baseloop，则调用 runInLoop
            如果ioloop指向的subloop不是 baseloop，则调用 queueInLoop，wakeup 唤醒该subloop后再执行回调
    2. 有一个新的客户端连接时，acceptor会执行这个回调操作。
            TcpServer => Acceptor => Channel => Poller
*/
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
    // 轮询算法，选择一个subloop，来管理对应的channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;  // newConnection 只有在mainloop中处理，不涉及多线程，所以不需要定义为原子类型
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from [%s] \n",
        name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过sockfd获取其绑定的本机的ip地址和端口信息 localAddr
    sockaddr_in local;
    ::bzero(&local, sizeof(local));     // ::memset(&local, 0, sizeof(local));
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0){
        LOG_ERROR("sockets::getLocalAddr errno:%d \n", errno);
    }
    InetAddress localAddr(local);

    // 根据连接成功的 sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(
        new TcpConnection(ioLoop,
                          connName,
                          sockfd,       // Socket Channel 
                          localAddr,
                          peerAddr)
    );
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));  // 设置如何关闭连接的回调

    // 直接调用TcpConnection::connectEstablished，执行了用户设置的连接建立回调
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}


void TcpServer::removeConnection(const TcpConnectionPtr& conn){
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}


void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection [%s] \n", 
                name_.c_str(), conn->getName().c_str());
    
    size_t n = connections_.erase(conn->getName());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}



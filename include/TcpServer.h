#pragma once

#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"      // 提供给用户
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <unordered_map>

/*
    TcpServer 成员函数：
        loop_           baseloop        -- 对应 manReactor
        acceptor_

        threadPool_     subloop         -- 对应 subReactor
        connections_    保存所有的TcpConnection

        callbacks       用户设置的各种回调操作

    TcpServer 类功能梳理：
        （mainloop）acceptor相关的用户新连接回调的逻辑：
            TcpServer => Acceptor => channel => Poller
            Poller监听到读事件，执行TcpServer::newConnection回调函数

        （subloop）用户连接 事件回调的逻辑：
            TcpServer => TcpConnection => channel => Poller
*/

// 对外服务器编程使用的类
class TcpServer: public noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;     // 线程初始化回调

    enum Option{            // 是否对端口重用
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();

    void setThreadInitCallback(const ThreadInitCallback& cb){ threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb){ connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb){ messageCallback_ = cb; }
    void setWriteCompleteCallback( const WriteCompleteCallback& cb){ writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);        // 设置线程数量，即设置subloop的个数
    void start();                             // 开启服务器监听

private: 
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* loop_;                                               // 用户定义的loop，即 baseloop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;                            // listenfd 相关操作。运行在manloop，主要为了监听新连接事件。
    std::shared_ptr<EventLoopThreadPool> threadPool_;               // one loop per thread（注意，threadPool_中不包含baseloop）

    ThreadInitCallback threadInitCallback_;                         // loop线程初始化回调函数（用户创建线程时，执行的函数）    
    ConnectionCallback connectionCallback_;                         // 连接回调函数（用户连接时，执行的函数）
    MessageCallback messageCallback_;                               // 消息回调函数（用户接收发送消息时，执行的函数）
    WriteCompleteCallback writeCompleteCallback_;                   // 消息发送完成回调函数（用户发送消息后，执行的函数）

    std::atomic_int started_ ; 

    int nextConnId_;
    ConnectionMap connections_;                                     // 保存所有的连接
};


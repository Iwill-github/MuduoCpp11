#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>   // enable_shared_from_this
#include <string>
#include <atomic>


class Channel;
class EventLoop;
class Socket;


/*  TcpConnection类的主要成员：
        socket_             # 客户端通信信息
        channel_
        localAddr_
        peerAddr_
        
        loop_               # channel所在的事件循环（subloop）

        callbacks          # TcpServer中注册的各种回调操作

        inputBuffer_        # 发送缓冲区相关
        outputBuffer_
        highWaterMark_

    TcpConnection类功能梳理：
        1. TcpConnection 用来打包成功连接客户端的通信链路。socket_、channel_
        2. TcpServer => Acceptor => TcpConnection => Channel => Poller

*/
// 当TcpConnection对象被std::shared_ptr管理时，enable_shared_from_this的成员函数
// shared_from_this()和weak_from_this()可以用来获取指向当前对象的std::shared_ptr或std::weak_ptr。
class TcpConnection: public noncopyable, public std::enable_shared_from_this<TcpConnection> {
private:
    enum StateE{
        kDisconnected, kConnecting, kConnected, kDisconnecting
    };

public:
    TcpConnection(EventLoop *loop, 
                const std::string& nameArg, 
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& getName() const { return name_; }
    const InetAddress& getLocalAddrress() const { return localAddr_; }
    const InetAddress& getPeerAddrress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string& buf);              // 发送数据
    void shutdown();                                // 关闭连接

    void setState(StateE state) { state_ = state; }

    void connectEstablished();      // 连接建立
    void connectDestroyed();        // 连接销毁

    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) { highWaterMarkCallback_ = cb; }
    void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }

private:
    // enum StateE{
    //     kDisconnected, kConnecting, kConnected, kDisconnecting
    // };

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* data, size_t len);

    void shutdownInLoop();

    EventLoop* loop_;                               // 这里绝对不是 baseLoop，因为 TcpConnection 都是在 subLoop 中管理的
    const std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和Acceptor类似 Acceptor => mainLoop       TcpConnection => subLoop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    ConnectionCallback connectionCallback_;     // TcpServer => TcpConnection => Channel
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;

    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
    
};



#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <unistd.h>         // close
#include <sys/types.h>      // bind等
#include <sys/socket.h>     // bind等
#include <string.h>         // memset
#include <netinet/tcp.h>    // TCP_NODELAY
#include <string>


static EventLoop* CheckLoopNotNull(EventLoop* loop){
    if (loop == nullptr){
        LOG_FATAL("mainLoop is null! errno:%d \n", errno);
    }
    
    return loop;
}


TcpConnection::TcpConnection(EventLoop *loop, 
                const std::string& nameArg, 
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr)
    : loop_( CheckLoopNotNull(loop) )
    , name_( nameArg )
    , state_(kConnecting)
    , reading_(true)
    , socket_( new Socket(sockfd) )
    , channel_( new Channel(loop, sockfd) )
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) // 64M，防止发送太快，而接受太慢
{
    // 给channel设置回调函数，poller监听到感兴趣事件发生时候所执行的函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd = %d", name_.c_str(), channel_->getFd());
    socket_->setKeepAlive(true);    // 启动保活机制，保持长连接
}



TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConnection::dtor[%s] at fd = %d state=%d\n", 
                name_.c_str(), channel_->getFd(), (int)state_);
}



// 向客户端发送数据
void TcpConnection::send(const std::string& buf){
    if(state_ == StateE::kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }else{
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));     // 绑定的是 this->sendInLoop
        }
    }
}



/*
    上层调用 shutdown时，关闭socket_的写端，poller会给channel通知关闭事件，
    loop会回调TcpConnection的handleClose方法（channel的关闭回调就是，TcpConnection中绑定的handleClose方法）。
*/
void TcpConnection::shutdown(){
    if(state_ == kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
    }
}



// 发送数据  应用写的快  而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置水位回调
void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwrote = 0;
    ssize_t remaining = len;
    bool faultError = false;

    // 如果连接已经被关闭了
    if(state_ == kDisconnected){
        LOG_ERROR("disconnecting, give up writing, errno:%d\n", errno);
        return;
    }

    // channel_ 第一次开始写数据，而且缓冲区没有待发送数据
    if( !channel_->isWriting() && outputBuffer_.readableBytes() == 0 ){
        nwrote = ::write(channel_->getFd(), data, len);
        if(nwrote >= 0){
            remaining = len - nwrote;

            if(remaining == 0 && writeCompleteCallback_){
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(&TcpConnection::writeCompleteCallback_, shared_from_this()));
            }
        }else{  // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK){
                LOG_ERROR("errno:%d\n", errno);
                if(errno == EPIPE || errno == ECONNRESET){  // SIGPIPE  RESET
                    faultError = true;
                }
            }
        }
    }

    // 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区中
    // 然后给channel注册epollout事件，poller发现tcp发送缓冲区有空间，会通知相应的sock-channel，调用writeCallback_回调方法
    // 也就是调用TcpConnection::handleWrite方法，把发送缓冲区中的数据全部发送完成
    if(!faultError && remaining > 0){
        size_t oldLen = outputBuffer_.readableBytes();  // 目前发送缓冲区剩余的待发送数据的长度
        if(oldLen < highWaterMark_ 
            && oldLen + remaining >= highWaterMark_
            && highWaterMarkCallback_)
        {
            // 调用水位线回调
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));    
        }
        outputBuffer_.append((char*) data + nwrote, remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting();      // 这里一定要注册channel的写事件，否则poller不会给channel通知epollout事件
        }
    }
}


// 连接建立
void TcpConnection::connectEstablished(){
    setState(kConnected);
    // shared_from_this() 表示从一个对象内部获取指向该对象的 shared_ptr 实例
    channel_->setTie(shared_from_this());       // 使用弱智能指针，TcpConnection对象被remove后，依然执行channel_对应的回调
    channel_->enableReading();                  // 向poller注册channel的eventin事件

    // 新连接建立，执行新连接建立回调
    connectionCallback_(shared_from_this());
}   


// 连接销毁 
void TcpConnection::connectDestroyed(){
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();     // 把channel所有感兴趣的事件，从poller中del掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove();             // 把channel从poller中删除掉
}


void TcpConnection::shutdownInLoop(){
    if(!channel_->isWriting()){     // 当前outputBuffer_中的数据已经发送完成
        socket_->shutdownWrite();   // 关闭写端
    }
}


// 调用读事件回调。
void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->getFd(), &savedErrno);
    if(n > 0){
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作 onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);   // shared_from_this() 表示获取当前 TcpConnection 对象的智能指针。将connection给他的原因是，他还需要用connection发送数据
    }else if(n == 0){
        // 对方关闭连接
        handleClose();
    }else{
        // 错误
        errno = savedErrno;
        LOG_ERROR("errno:%d\n", errno);
        handleError();
    }
}


// 调用写事件回调
void TcpConnection::handleWrite(){
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->getFd(), &savedErrno);
        if(n > 0){
            outputBuffer_.retrive(n);
            if(outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();
                if(writeCompleteCallback_){
                    // 唤醒 loop_ 对应的线程，执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if(state_ == kDisconnecting){
                    shutdownInLoop();
                }
            }
        }else{
            LOG_ERROR("errno:%d\n", errno);
        }
    }else{
        LOG_ERROR("TcpConnection fd=%d is down, no more writing\n", channel_->getFd());
    }
}


// poller => channel::closeCallback => TcpConnection::handClose
void TcpConnection::handleClose(){
    LOG_INFO("fd=%d state=%d\n", channel_->getFd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行连接关闭时的回调
    closeCallback_(connPtr);        // 执行关闭连接回调 执行的是TcpServer::removeConnection回调方法
}


// 调用错误事件回调
void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->getFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }

    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d", name_.c_str(), err);
}


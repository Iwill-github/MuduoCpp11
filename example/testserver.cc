#include <mymuduo/TcpServer.h>
#include <mymuduo/Logger.h>

#include <string>
#include <functional>


class EchoServer{
public:
    EchoServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &nameArg)
            : server_(loop, listenAddr, nameArg)
            , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的loop线程数量
        server_.setThreadNum(3);        // 3个subloop + 1个mainloop
    }

    void start(){
        server_.start();
    }


private:
    // 设置连接建立或断开的回调
    void onConnection(const TcpConnectionPtr& conn){
        if(conn->connected()){
            LOG_INFO("conn UP : %s \n", conn->getPeerAddrress().toIpPort().c_str());
        }else{
            LOG_INFO("conn DOWN : %s \n", conn->getPeerAddrress().toIpPort().c_str());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time){
        std::string msg = buf->retriveAllAsString();
        conn->send(msg);
        conn->shutdown();   // 关闭写端  EPOLLHUP => closeCallback_
    }

    EventLoop *loop_;
    TcpServer server_;
};


/*
    编译命令:
        g++ testserver.cc -o testserver -lpthread -lmymuduo -g
*/
int main(){
    EventLoop loop;
    InetAddress addr(8002);
    EchoServer server(&loop, addr, "EchoServer");   // Acceptor、non-blocking listenfd、create、bind

    server.start();     // listen、loopthread、listenfd => acceptChannel => mainloop
    loop.loop();        // 启动 mainloop 的底层 poller

    return 0;
}



#pragma once

#include <netinet/in.h>
#include <iostream>
#include <string>

#include "noncopyable.h"


/*
    InetAddress 类功能梳理：
        InetAddress 类是一个表示 IP 地址和端口号的类
*/
// 这里不继承 public noncopyable 
class InetAddress {
public:
    explicit InetAddress(uint16_t port, std::string ip = "127.0.0.1");        // 禁止隐式的拷贝构造函数
    explicit InetAddress(sockaddr_in addr)
        : addr_(addr)
    {}
    InetAddress(){}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;

    const sockaddr_in* getsockAddr() const{
        return &addr_;
    }

    void  setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;      // 存储 IPv4 地址和端口号信息。
};


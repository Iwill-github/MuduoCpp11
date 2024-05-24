#include "InetAddress.h"
#include <strings.h>        // bzero方法的头文件
#include <arpa/inet.h>

#include <string.h>

/*
    1. explicit 应该出现在声明的地方，不能出现在定义的地方
    2. 默认参数应该在声明处给出，不能在定义处给出
*/ 
InetAddress::InetAddress(uint16_t port, std::string ip){
    bzero(&addr_, sizeof addr_);    
    addr_.sin_family = AF_INET;         // ipv4
    addr_.sin_port = htons(port);       // host to net 
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  // 将ip.c_str()转化为点分十进制表示，同时转为网络字节序
}


std::string InetAddress::toIp() const{
    // addr_
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);     // ::表示全局命名空间。将二进制的网络地址（如struct in_addr）转换为主机字节序的点分十进制字符串
    return buf;
}


std::string InetAddress::toIpPort() const{
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    snprintf(buf + end, sizeof buf - end, ":%u", port);

    return buf;
}


uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}



// #include <iostream>
// int main(int argc, char const *argv[]){
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;

//     return 0;
// }

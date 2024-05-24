#include "Poller.h"

#include "EpollPoller.h"

#include <stdlib.h>

/*
    该方法实现之所以不卸载Poller.cc中，是因为该方法已使用了派生类的实现方法，一般情况下，不建议在基类中引用派生类的实现方法；
    而DefaultPoller.cc作为一个公共文件，可以引用其他所有类的实现方法
*/
Poller* Poller::getDefaultPoller(EventLoop *loop){
    if( ::getenv("MUDUO_USE_POLL") ){
        return nullptr;     // 生成 poll的实例
    }else{
        return new EPollPoller(loop);     // 生成 epoll的实例
    }
}

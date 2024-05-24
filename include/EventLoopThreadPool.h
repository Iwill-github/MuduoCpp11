#pragma once 

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;


/*
    EventLoopThreadPool 类的功能梳理：该类的功能主要是管理 EventLoopThread。
        1. baseloop_ 不在 loops_ 中。
        2. 如果没有用 setThreadNum 设置工作线程个数，那么 IO线程 和 工作线程 均为 baseloop_ 对应线程。
        3. 如果用 setThreadNum 设置了工作线程个数，那么 IO线程 为 baseloop_ 对应线程，工作线程为轮询出的subloop对应线程。

    问题：
        baseloop_ 好像是创建 EventLoopThreadPool 时，传入的 EventLoop*
        在 EventLoopThreadPool 中，貌似没有其对应的线程？？
*/
class EventLoopThreadPool: public noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;     // EventLoopThread 类对象开启新线程执行事件循环前，需要执行的初始化线程回调函数

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    EventLoop* getNextLoop();                               // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    std::vector<EventLoop*> getAllLoops();
    bool getStarted() const { return started_; }
    const std::string& getName() const { return name_; }    // 函数后面的const关键字表明这个成员函数不会修改对象的状态。这意味着你可以在一个const对象上调用这个函数。

private:
    EventLoop* baseLoop_;           // 用户创建的第一个 loop
    std::string name_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>> threads_;     // 包含了所有创建的线程指针
    std::vector<EventLoop*> loops_;                             // 包含了所有创建的事件循环
};

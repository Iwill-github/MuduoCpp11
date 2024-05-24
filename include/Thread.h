#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>       // 智能指针
#include <unistd.h>     // pid_t
#include <string>
#include <atomic>

/*
    EventLoop 类的功能梳理：
        记录了一个新线程的详细信息。
*/

class Thread: public noncopyable{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string& name = std::string());   // 构造函数。name默认值为空
    explicit Thread();  // 构造函数，未初始化任何参数，仅解决报错
    ~Thread();

    void start();
    void join();
    void end();

    bool getStarted() const { return started_; };
    bool getTid() const { return tid_; };
    const std::string& getName() const { return name_; };
    
private:
    void setDefaultName();

    bool started_;
    bool joined_;                               // 是否需要等待其他线程结束
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;                           // 线程函数
    std::string name_;                          // 线程名
    static std::atomic_int numCreated_;         // 对所有线程数量进行计数
};

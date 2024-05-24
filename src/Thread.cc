#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>        // sem_t


/*
    在C++中，std::atomic 类型有特殊的行为和限制，主要是因为它们的无锁特性和线程安全性。
    具体来说，std::atomic 对象的拷贝构造函数和拷贝赋值运算符是被删除的，
    这意味着不能通过赋值或拷贝来初始化 std::atomic 对象。
*/ 
// std::atomic_int Thread::numCreated_ = 0;     // 不可以进行赋值初始化，因为拷贝构造函数被删除了 
std::atomic_int Thread::numCreated_(0);         // 静态成员变量需要类外单独进行初始化。调用指定的构造函数初始化


Thread::Thread(ThreadFunc func, const std::string& name)    // 默认参数只能在声明中指定
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(func)
    , name_(name)
{
    setDefaultName();
}    


Thread::~Thread(){
    // 如果线程已启动且未被加入，则尝试使线程脱离
    if(started_ && !joined_){
        thread_->detach();  // 将thread_所指向的线程脱离，使其在执行完成后自动释放资源，无需被其他线程 显式join该线程
    }
}


// 一个Thread对象，记录的就是一个新线程的详细信息
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);   // false 表示信号量不是进程间共享的

    thread_ = std::shared_ptr<std::thread>(
        new std::thread([&](){
            tid_ = CurrentThread::getTid();     // 获取线程的tid_值
            sem_post(&sem);                     // 尝试将信号量sem +1
            func_();                            // 开启新线程，专门执行该线程函数
        })
    );

    // 这里必须等待获取上面新创建线程的tid_值
    sem_wait(&sem);                             // 尝试将信号量sem -1
}


void Thread::join(){
    joined_ = true;
    thread_->join();        // thread.join()
}


void Thread::end(){
    
}


void Thread::setDefaultName(){
    int num = numCreated_;
    if(name_.empty()){
        // name_ = "Thread";
        // if(num > 0){
        //     name_ += std::to_string(num);
        // }

        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;   // 隐式转换
    }
}

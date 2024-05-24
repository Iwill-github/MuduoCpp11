#include "CurrentThread.h"

namespace CurrentThread{
    __thread int t_cachedTid = 0;

    void cachedTid(){
        if(t_cachedTid == 0){
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));    // 通过系统调用 syscall获取当前线程的pid
        }

    }
}






#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread{
    extern __thread int t_cachedTid;

    void cachedTid();

    // 该线程id是否缓存，若没缓存则缓存后返回，若缓存了则直接返回缓存的值
    inline int getTid(){   // inline 关键字用于请求编译器进行函数内联，将函数体插入到每个调用函数的地方，以减少函数调用的开销
        if(__builtin_expect(t_cachedTid == 0, 0)){
            cachedTid();
        }
        return t_cachedTid;
    }
}


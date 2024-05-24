#include <Buffer.h>
#include <errno.h>
#include <sys/uio.h>        // iovec
#include <unistd.h>         // write


/*
    从 fd 上读取数据     Poller 工作在 LT 模式
    Buffer缓冲区是有大小的！但是从fd上读数据的时候，却不知道tcp数据最终的大小
*/ 
ssize_t Buffer::readFd(int fd, int* saveErrno){
    char extrabuf[65536] = {0};     // 栈上的内存空间   64K的空间
    struct iovec vec[2];

    const size_t writable = writableBytes();    // Buffer底层缓冲区，剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;   // 缓冲区中可写空间的起始位置
    vec[0].iov_len = writable;                  // 可写空间的大小

    // 缓冲区中可写空间不足，则将栈上的内存空间作为可写空间
    vec[1].iov_base = extrabuf;                 // 栈上的内存空间
    vec[1].iov_len = sizeof extrabuf;           // 栈上的内存空间的大小

    // 设置可写空间。如果可写空间不足，则将栈上的内存空间作为可写空间（iovecnt = 2，表示选择使用第2块空间）
    const int iovecnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = readv(fd, vec, iovecnt);  // 读取数据

    if(n < 0){                  // 读取失败
        *saveErrno = errno;
    }else if(n <= writable){    // 读取成功，且数据量小于可写空间
        writerIndex_ += n;
    }else{                      // 读取成功，且数据量大于可写空间，extrabuf 也写入了数据
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable); // writerIndex_ 开始写 n - writable 大小的数据
    }

    return n;
}



ssize_t Buffer::writeFd(int fd, int *saveErrno){
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }

    return n;
}


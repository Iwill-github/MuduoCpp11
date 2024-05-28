#pragma once

#include "noncopyable.h"

#include <vector>
#include <iostream>     // size_t
#include <string>
#include <algorithm>    // copy

/*
    @code
    +--------------------+--------------------+--------------------+
    | prependable bytes  |   readable bytes   |    writable bytes  |
    |                    |     (CONTENT)      |                    |
    +--------------------+--------------------+--------------------+
    |                    |                    |                    |
    0         <=     readerIndex          writerIndex   <=       size

    应用写数据 -> 缓冲区 -> Tcp发送缓冲区 -> 网络发送缓冲区 -> TCP发送
*/
class Buffer: public noncopyable {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;
    
    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const{ return writerIndex_ - readerIndex_; }
    size_t writableBytes() const{ return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const{ return readerIndex_; }


    // 返回缓冲区中，可读数据的起始地址
    const char* peek() const{
        return begin() + readerIndex_;
    }


    void retrive(size_t len){
        if(len < readableBytes()){  // 应用只读取了可读缓冲区的一部分，长度为len，还剩下 readerIndex_ + len -> writerIndex_
            readerIndex_ += len;
        }else{                      // len = readableBytes()
            retriveAll();
        }
    }


    void retriveAll(){
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }


    std::string retriveAsString(size_t len){
        std::string result(peek(), len);     
        retrive(len);                       // 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
        return result;
    }


    // 把 onMessage 函数上报的Buffer数据，转成string类型的数据返回
    std::string retriveAllAsString(){
        return retriveAsString(readableBytes());    // 应用可读取数据的长度
    }


    // buffer_.size - writerIndex_ >= len
    void ensureWritable(size_t len){
        if(writableBytes() < len){
            makeSpace(len);     // 扩容函数
        }
    }


    char* beginWrite(){ return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }


    // 把要 [data, data + len] 的数据，添加到 writable 中
    void append(const char* data, size_t len){
        ensureWritable(len);    // 空闲空间不够时，进行扩容
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }


    /*
        从 fd 上读取数据     Poller 工作在 LT 模式
        Buffer缓冲区是有大小的！但是从fd上读数据的时候，却不知道tcp数据最终的大小
    */ 
    ssize_t readFd(int fd, int* saveErrno);
    ssize_t writeFd(int fd, int* saveErrno);                    // 通过fd发送数据

private:
    // 对迭代器进行取引用、取地址，得到首元素地址
    char* begin(){ return &*buffer_.begin(); }
    const char* begin() const{ return &*buffer_.begin(); }

    // 扩容
    void makeSpace(size_t len){
        /*
            kCheapPrepend   |   readerIndex_   |   writerIndex_   |
            kCheapPrepend   |                       len                         |
        */  
        if(writableBytes() + prependableBytes() < len + kCheapPrepend){
            // 如果 可写空间(已经被读过可以覆盖的空间) + 可读空间 < len + kCheapPrepend，则扩容
            buffer_.resize(writerIndex_ + len);
        }else{
            // 将可读数据向前移动
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,       // 输入的迭代器 first
                      begin() + writerIndex_,       // 输入的迭代器 end
                      begin() + kCheapPrepend);     // 输出的迭代器 first
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;    
};


#pragma once

/**
 * noncopyable 被继承之后，派生类对象可以被正常的构造和析构，但是派生类对象无法进行拷贝构造和赋值操作
 */
class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;                   // 删除类的 拷贝构造 函数
    noncopyable& operator=(const noncopyable&) = delete;        // 删除类的 拷贝复制 操作符

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};


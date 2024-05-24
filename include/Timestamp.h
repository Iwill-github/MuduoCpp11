#pragma once

#include <iostream>
#include <string>

/*
    时间类
*/
class Timestamp{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);     // 使用 explicit 限制编译器执行隐式对象转换
    static Timestamp now();
    std::string toString() const;       // 常量成员函数

private:
    int64_t microSecondsSinceEpoch_;
};


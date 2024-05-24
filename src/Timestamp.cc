#include "Timestamp.h"
#include <sys/time.h>

Timestamp::Timestamp() 
    : microSecondsSinceEpoch_(0){}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch) 
    : microSecondsSinceEpoch_(microSecondsSinceEpoch){}

Timestamp Timestamp::now(){
    return Timestamp(time(NULL));
}

std::string Timestamp::toString() const{      // 常量成员函数
    char buf[128] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);  // 返回值是tm的结构体指针
    snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d",
               tm_time->tm_year + 1900, 
               tm_time->tm_mon + 1, 
               tm_time->tm_mday,
               tm_time->tm_hour, 
               tm_time->tm_min, 
               tm_time->tm_sec);
    return buf;
}


/*
    右键选择，Run Code
*/ 
// #include <iostream>
// int main(){
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }

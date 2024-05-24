#!/bin/bash
#指定该脚本应该使用那个程序来执行。

set -e                       # 启用“退出立即”（errexit）选项.如果脚本中的任何命令返回非零退出状态（表示错误），脚本会立即停止执行。

if [! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build && cmake .. && make

cd ..       # 回到项目根目录


#创建头文件目录
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi


#把头文件拷贝到 /usr/include/mymuduo
cd include
for header in `ls *.h`
do
    cp $header /usr/include/mymuduo
done
cd ..

#把so库拷贝到 /usr/lib
cp `pwd`/lib/libmymuduo.so /usr/lib


# 更新动态链接库缓存
ldconfig



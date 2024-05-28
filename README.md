
# MuduoCpp11
仿muduo库（c++11，不依赖boost库）

# 编译该项目
sudo sh automake.sh

# 测试该项目
cd example
make
./testserver                # 启动服务器
telnet 127.0.0.1 8002       # 模拟客户端连接

<img src="../imgs/test.png" alt="image-test" />



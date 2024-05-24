cmake_minimum_required(VERSION 3.5)    # 配置最低版本号
project(muduocpp11)                       # 配置项目名称

# muduocpp11 最终编译成so动态库，设置动态库的路径，放在根目录的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 配置编译选项：嵌入调试信息 以及 启动c++11语言标准
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC)
# 注意使用双引号包围整个变量扩展和追加的字符串，这是推荐的做法，以避免某些shell解析问题
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")   


# 配置头文件的搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
)


# 定义参与编译的源代码文件
aux_source_directory(./src SRC_LIST)
# 编译生成动态库
add_library(muduocpp11 SHARED ${SRC_LIST})     # SHARED 表示生成动态库


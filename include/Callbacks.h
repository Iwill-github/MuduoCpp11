#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <memory>
#include <functional>


class Buffer;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;                        // 指向TcpConnection的智能指针
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;        // 连接回调函数（用户连接时，执行的函数）
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;             // 关闭回调函数（用户关闭连接时，执行的函数）
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;     // 消息回调函数（用户接收发送消息时，执行的函数）
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;   // 读回调函数（用户接收数据时，执行的函数）
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;         // 高水位回调函数（用户接收数据时，执行的函数）



telnet 127.0.0.1 8002

ps -ef | grep testserver    # -e参数表示显示所有进程，-f参数提供了完整的格式

sudo netstat -tanp | grep 8002
   * -t：此选项指示netstat仅显示TCP协议的连接。
   * -a：显示所有状态的连接，包括监听（LISTEN）、建立（ESTABLISHED）以及等待关（TIME_WAIT）等状态的套接字。
   * -n：以数字形式显示IP地址和端口号，而不是尝试解析主机名和服务名称，这样可加快显示速度并避免DNS查询失败问题。
   * -p：显示每个连接或监听对应的进程ID（PID）和进程名称，这对于追踪哪个程序建立了特定的网络连接非常有用。


gdb相关
   * start
   * n       next  单步执行程序（跳过函数调用）
   * s       step  单步执行程序
   * p       print 打印变量值


source insight使用简单记录
1. 项目文件下，创建 xx_insight 目录
2. 设置项目名称、项目路径
3. Option -> File Type Options 添加 *.cc
4. 选择带查看的文件夹目录 -> Add tree -> close
5. Project -> Synchronize Files
6. 左下角  -> 浏览项目符号
7. ctrl + 鼠标左键  ->  跳转到对应文件
8. Options -> Key Assignments -> 搜索high -> 添加/设置快捷键


扩展
1. TcpClient 类
2. 支持定时事件 TimeQueue     链表/队列  事件轮(libevent)    nginx定时器（红黑树）
3. DNS、HTTP、RPC
4. 丰富的使用示例 examples
5. 服务器性能测试 - QPS, Queries Per Second     需要重设置linux进程的socketfd上限
   * wrk         linux上，需要单独编译安装，只能测试http服务的性能
   * jmeter      需要安装 JDK 运行环境，可以测试http服务 tcp服务 生成聚合报告





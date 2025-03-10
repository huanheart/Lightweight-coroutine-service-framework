# 基于ucontext和C++11实现的协程服务框架

## 协程服务框架概述
参考开源项目sylar：实现协程库以及高性能TCPServer，并且提供**长连接服务**,支持简单的http服务

关于长连接的说明
* 查看用户的活跃情况，若在指定时间内没有活跃，则再删除。
* 而不是请求响应一次就直接删除对应socket
* 本身协程库内部已实现，默认定时器Timeout为120s,例如recv异步120s没有接收成功则删除对应sockfd

测量得出:QPS 是仓库中轻量级webserver的两倍

* 协程实现：基于POSIX标准下的**ucontext实现非对称协程**
* 实现**N-M 协程调度器**，支持主线程参与调度
* hook重新实现部分重要函数接口，利用epoll和定时机制做到阻塞调用异步化
* 使用池化技术将**线程和协程结合**，实现高性能并发
* 实现基于**内存池的缓存数据结构**
* 使用**nginx作为反向代理**（可自行选择）.在nginx.conf中，**可限制最大流量以及最大并发处理数量，并且使用工厂模式进行管理**
* 利用**单例模式**管理实现**数据库连接池**

## 待扩展部分
- [X] 二次更改长短连接，不依赖客户端是否长连接还是短连接，而是服务端自行选择
- [X] 比较短链接版本和长链接版本的qps差异(长链接要比短链接的qps明显大长连接的qps的20%)
- [X] 将http_conn直接继承于tcpserver，并且加锁。查看与当前http_conn数组映射模式的性能差异
- [X] 查看muduo基于**多缓冲区**与**批量写**的高性能异步日志源码并导入到该项目中，并可通过命令行选项选择日志是否开启，默认开启
- [X] 使用gdb解决管道破裂的bug，保证http服务的正常运行
- [X] 实现简单的http服务
- [X] 更改http_conn.cpp的Socket更改为封装的Socket，io函数为Socket类封装的io函数
- [X] 实现提供简单的RPC服务,[详情请看](https://github.com/huanheart/RPCCoroutineServiceFramework)
- [ ] gdb下进入视频页面后回退会出现管道破裂，但是普通运行并不会，找了好久找不到
- [ ] 解决新new调度器赋值给accept_worker调度器将会整体段错误的情况,m_ioworker并不会出现这个情况


## 运行环境要求

* 乌班图 22.04
* boost库（muduo库是基于boost库，固然需要有所依赖）
* muduo库（因为有用到异步日志）
* mysql 8.0.37 (注意：使用mysql的时候需要让终端处于root模式，否则可能出现mysql没有权限的情况)
* nginx，具体参考:https://help.fanruan.com/finereport10.0/doc-view-2644.html
* 有makefile , g++相关工具

* mysql所需更改的地方
    ```cpp
  create database yourdb;     //yourdb根据自身喜好命名数据库名称

  // 创建user表
  USE yourdb;      //使用该数据库
  CREATE TABLE user( 
      username char(50) NULL,
      passwd char(50) NULL
  )ENGINE=InnoDB;

  // 添加数据
  INSERT INTO user(username, passwd) VALUES('name', 'passwd');
    ```
* 修改main.cpp中的数据库初始化信息
    ```cpp
    //数据库登录名，密码，以及你所创建的数据库名称
    string user="root"; //用户
    string passwd="root" //你每次使用mysql的密码
    string databasename="yourdb"; //你创建的数据库的名称
    ```
* mysql部分问题以及解决方案(乌班图22.04版本缺少枚举的解决)
```cpp
  494 | enum net_async_status STDCALL
      |      ^~~~~~~~~~~~~~~~
/usr/include/mysql/mysql.h:496:6: error: use of enum ‘net_async_status’ without previous declaration
```
```cpp
//找到乌班图下mysql.h的位置，添加这么一句话
#ifndef DECLARATION_NET_ASYNC_STATUS
#define DECLARATION_NET_ASYNC_STATUS
enum net_async_status {
NET_ASYNC_COMPLETE = 0,
NET_ASYNC_NOT_READY,
NET_ASYNC_ERROR,
NET_ASYNC_COMPLETE_NO_MORE_RESULTS
};
#endif
```
* 修改main.cpp中run函数的ip地址
    ```cpp
  //将对应192.168.15.128更改为服务器本身的ip地址
  sylar::Address::ptr m_adress=sylar::Address::LookupAnyIPAddress("192.168.15.128:"+to_string(port) );
    ```
* nginx :进入/usr/nginx/conf目录下，使用vim将nginx更改为如下
    * 将location下目录全部更改为如下
      ```cpp
              location  / {
              proxy_pass http://192.168.15.128:9006;          #改为CoroutineServer的ip地址以及端口号
              proxy_http_version 1.1;                      #设置http版本，如果不设置那么将会出错（因为nginx默认发送http1.0请求，但是该CoroutineServer响应不了1.0请求
              limit_conn addr 1;                           #用于设置最大并发数
              #limit_rate  50;   #限制对应响应的速率，可以用50（默认表示50bit来限制）追求速度快也可以直接弄成1m
              limit_conn_log_level warn;
              proxy_set_header X-Forwarded-By "Nginx";   #将http标识头多加一个nginx标识，不加也会出错(根据该CoroutineServer逻辑可以看出来)
  
          }
      ```
        *  在keepalive_timeout 65; 下多加一句话
        ```cpp
        limit_conn_zone $binary_remote_addr zone=addr:5m;  #将其设置为5m的空间，其共享内存的名字为addr，此时其实就可以处理10几万的数量了
        ```

## 个性化运行
```cpp
./CoroutineServer [-p port]  [-s sql_num] [-t thread_num] [-c close_log]  [-n Proxy]
```
* -p 自定义端口号
    * 默认9006
* -s 数据库连接池默认数量
    * 默认为8
* -t 线程数量
    * 默认为8
* -c 是否开启日志系统，默认打开
    * 0,打开日志
    * 1,关闭日志
* -n 选择是否启动反向代理,默认启动nginx,后续会加阿帕奇等中间服务器
    * 0 启动nginx
    * 1 不使用任何中间服务器

## 启动项目：
* cd在项目目录下
* make          #进行编译，将会生成CoroutineServer
* ./CoroutineServer           #启动项目
* 当选择nginx的时候，根据上述文档，应该去到/usr/nginx/sbin先将对应nginx进行启动(./nginx),也要查看当前nginx监听的端口是否被占用
* 在浏览器输入ip:端口.   **举例192.168.12.23:80**   即访问到对应服务器(若开启CoroutineServer的-n选项为nginx，那么ip地址输入为nginx的ip地址，否则输入CoroutineServer的ip地址+端口,**注意默认需要输入nginx的ip地址，否则访问不到**)

## 关于日志文件
注意，当前日志文件是隐藏的，默认会生成在logs目录下，通过ls -a选项即可看到当前目录下拥有的日志

## 如何删除日志文件？
* 由于日志文件是隐藏的，可能在命令行中不能直接看到，换成ide可能看得到，固然使用如下命令可以删除隐藏文件,
* 与常规的rm -f *.log这种形式不同，需要加入.在*的前面
```cpp
rm -f .*.log
```

## 压力测试
针对每个人的服务器性能不同，测压出来的qps将会有所不同，可以拿同一个服务器上nginx的和当前webserver的qps进行对比


```cpp
#Linux上使用wrk工具
#安装
sudo apt-get install wrk
#使用示例
#-t12表示使用12个线程,-c400表示建立400个并发连接,-d30表示测试时间为30s
wrk -t12 -c400 -d30s -H "Connection: keep-alive" http://192.168.15.128:80
```


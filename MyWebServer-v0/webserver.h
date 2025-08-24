#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string>

// #include "config.h"
//#include "sql_connection_pool.h"
#include "threadpool.h"
//#include "list_timer.h"
#include "epoll_util.h"
#include "http_connection.h"
//#include "log.h"

using std::string;

const int MAX_FD = 65536; // 最大文件描述符
const int MAX_EVENT_NUMBER = 10000; // 监听最大事件数
const int TIMESLOT = 5; // 定时器周期

class WebServer {
public:
    WebServer();
    ~WebServer();
    // 初始化
    void init(int port , string user, string passWord, string databaseName,
              int log_write , int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);
    // 线程池初始化
    void thread_pool();
    // 数据库连接池初始化
    void sql_pool();
    // 开启日志
    void log_write();
    // 设置触发模式
    void trig_mode();
    // 开启事件监听
    void event_listen();
    // 开启循环监听
    void event_loop();
    // 给fd创建定时器
    void timer(int connectfd, struct sockaddr_in client_address);
    // fd的定时器向后延迟
    void adjust_timer(timeNode *timer);
    // 定时器
    void deal_timer(timeNode *timer, int sockfd);
    // 收到客户端连接时的处理
    bool deal_client_data();
    // 收到信号，处理信号
    bool deal_with_signal(bool& timeout, bool& stop_server);
    // 处理读数据请求
    void deal_with_read(int sockfd);
    // 处理写数据请求
    void deal_with_write(int sockfd);


public:
    // 基础
    int port;
    char* root;
    int logWrite;
    int closeLog;
    int actorModel;
    int pipefd[2];
    int epollfd;
    // 客户端请求
    http_connection * users;

    // 数据库相关
    sql_connection_pool* sqlConnPool;
    string username;
    string password;
    string databaseName;
    int sqlConnNum;

    // 线程池相关
    threadpool<http_connection>* threadPool;
    int threadNum;

    // epoll
    struct epoll_event events[MAX_EVENT_NUMBER];
    int listenfd;
    int optLinger;
    int trigMode; // 默认0-LT 1-ET
    int listenTrigMode;
    int connectTrigMode;


    // 定时器
    client_data* users_timer;
    // epoll工具
    epoll_util epoll_utils;
};


#endif
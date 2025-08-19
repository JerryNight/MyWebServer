#ifndef EPOLL_UTIL_H
#define EPOLL_UTIL_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>
#include "list_timer.h"


class epoll_util {
public:
    epoll_util(){}
    ~epoll_util(){}
    void init(int timeslot);
    // 将文件描述符设置为非阻塞
    int set_nonBlocking(int fd);
    // 注册读事件，ET模式，开启oneshot
    void add_fd(int epollfd, int fd, bool oneShot, int trigMode);
    // 删除事件
    void del_fd(int epollfd, int fd);
    // 信号处理函数
    static void sig_handler(int sig);
    // 设置信号函数
    void add_sig(int sig, void(handler)(int), bool restart = true);
    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();
    void show_error(int connfd, const char *info);

public:
    static int* u_pipe_fd;
    sort_timer_lst timer_lst;
    static int u_epollfd;
    int timeslot;
};

int* epoll_util::u_pipe_fd = 0;
int epoll_util::u_epollfd = 0;


#endif
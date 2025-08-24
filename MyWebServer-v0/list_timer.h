#ifndef LIST_TIMER_H
#define LIST_TIMER_H
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

class timeNode;
struct client_data{
    struct sockaddr_in address;
    int sockfd;
    timeNode* timer;
};

class timeNode {
public:
    timeNode():prev(NULL),next(NULL){};
public:
    time_t expire;
    void (* cb_func)(client_data*); // 函数指针
    client_data* user_data;
    timeNode* prev;
    timeNode* next;
};

class sort_timer_lst {
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(timeNode *timer);
    void adjust_timer(timeNode *timer);
    void del_timer(timeNode *timer);
    void tick();

private:
    timeNode* head;
    timeNode* tail;
    void add_timer(timeNode *timer,timeNode *head);
};


void cb_func(client_data *user_data);

#endif
#include "epoll_util.h"



void epoll_util::init(int timeslot){
    timeslot = timeslot;
}

// 将文件描述符设置为非阻塞
int epoll_util::set_nonBlocking(int fd){
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

// 注册读事件，ET模式，开启oneshot
void epoll_util::add_fd(int epollfd, int fd, bool oneShot, int trigMode){
    struct epoll_event event;
    event.data.fd = fd;
    if (trigMode == 1) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if (oneShot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonBlocking(fd);
}

// 删除事件
void epoll_util::del_fd(int epollfd, int fd){
    struct epoll_event event;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
}

// 信号处理函数
static void sig_handler(int sig){
    int msg = sig;
    send(epoll_util::u_pipe_fd[1], (char *)&msg, 1, 0);
}

// 设置信号函数
void epoll_util::add_sig(int sig, void(handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
}

// 定时处理任务，重新定时以不断触发SIGALRM信号
void epoll_util::timer_handler(){
    timer_lst.tick();
    alarm(timeslot);
}

void epoll_util::show_error(int connfd, const char *info){
    send(connfd, info, strlen(info), 0);
    close(connfd);
}



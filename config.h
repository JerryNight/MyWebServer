#ifndef CONFIG_H
#define CONFIG_H

#include "webserver.h"

class Config {
public:
    Config();
    ~Config(){};

    // 解析参数
    void parseArg(int argc, char* argv[]);

public:
    // 端口号
    int port;
    // 日志写入方式
    int logWrite;
    // 触发组合方式
    int trigMode;
    // listenfd 触发方式
    int listenTrigMode;
    // connectfd 触发方式
    int connectTrigMode;
    // 设置延迟关闭TCP连接，确保数据完整发送
    int optLinger;
    // 数据库连接池数量
    int sqlPollNum;
    // 线程池连接数量
    int threadPollNum;
    // 是否关闭日志
    int close_log;
    // 并发模型选择：Reactor Proactor
    int actorMode;

};



#endif
#include "config.h"

Config::Config(){
    // 端口号
    port = 8080;
    // 日志写入方式：默认同步写入（可设置异步写入）
    logWrite = 0;
    // 触发组合方式:默认listenfd: LT + connectfd: LT
    trigMode = 0;
    // listenfd 触发方式:默认 LT
    listenTrigMode = 0;
    // connectfd 触发方式:默认 LT
    connectTrigMode = 0;
    // 设置延迟关闭TCP连接，确保数据完整发送:默认不使用
    optLinger = 0;
    // 数据库连接池数量:默认8
    sqlPollNum = 8;
    // 线程池连接数量：默认8
    threadPollNum = 8;
    // 是否关闭日志：默认不关闭
    close_log = 0;
    // 并发模型选择：默认 Proactor
    actorMode = 0;
}

void Config::parseArg(int argc, char* argv[]){
    int opt;
    const char* optstring = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, optstring)) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                logWrite = atoi(optarg);
                break;
            case 'm':
                trigMode = atoi(optarg);
                break;
            case 'o':
                optLinger = atoi(optarg);
                break;
            case 's':
                sqlPollNum = atoi(optarg);
                break;
            case 't':
                threadPollNum = atoi(optarg);
                break;
            case 'c':
                close_log = atoi(optarg);
                break;
            case 'a':
                actorMode = atoi(optarg);
                break;
            default:
                break;
        }
    }
}
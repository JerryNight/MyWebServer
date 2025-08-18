#include "config.h"

/**
 * WebServer:
 * 1. 线程池
 * 2. 数据库连接池
 * 3. 定时器
 * 4. 日志
 * 5. 阻塞队列
 * 6. http解析
 */

int main(int argc, char* argv[]){
    // 数据库连接信息
    string user = "root";
    string passWord = "996430";
    string databaseName = "webserver";

    // 解析命令行
    Config config;
    config.parseArg(argc, argv);

    // 启动WebServer
    WebServer server;

    server.init(config.port, user, passWord, databaseName, config.logWrite, 
        config.optLinger, config.trigMode, config.sqlPollNum, config.threadPollNum, config.close_log, config.actorMode);
    // 开启日志

    // 开启数据库连接池

    // 开启线程池

    // 设置触发模式

    // 开启监听

    // 循环运行


    return 0;
}
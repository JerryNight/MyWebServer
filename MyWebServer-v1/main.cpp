#include "server/webserver.h"


int main() {
    WebServer server(8080,3,60000,false, // 端口、ET模式、timeoutMS、延迟退出
                     3306, "root", "Lyt996430$","webserverdb", // MYSQL
                     12, 8, true,1,2048); // 连接池数量、线程池数量，日志开启，日志等级，日志队列容量
    server.Start();
}
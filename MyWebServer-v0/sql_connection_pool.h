#ifndef SQL_CONNECTION_POOL_H
#define SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "locker.h"
#include "log.h"

using std::string;
using std::list;

class sql_connection_pool {
public:
    MYSQL* get_connection(); // 获取数据库连接
    bool release_connection(MYSQL* conn); // 释放连接
    int get_freeconn(); // 获取连接
    void destroy_pool(); // 销毁所有连接
    // 单例模式，懒汉式
    static sql_connection_pool* get_instance();
    void init(string url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log); 
private:
    // 私有构造函数、析构函数
    sql_connection_pool();
    ~sql_connection_pool();

    int _maxConn; // 最大连接数
    int _curConn; // 已用连接数
    int _freeConn; // 空闲连接数
    locker _lock;
    list<MYSQL*> _connList; // 连接池
    sem reserve;  // 信号量
public:
    string url;		 //主机地址
	string port;		 //数据库端口号
	string user;		 //登陆数据库用户名
	string passWord;	 //登陆数据库密码
	string databaseName; //使用数据库名
	int close_log;	     //日志开关
};

/**
 * 将数据库的获取连接 & 释放连接，封装在RAII类中
 * 这样，获取连接后，就不用手动调用release释放了。
 * 用法：MYSQL* conn; sql_connection_RAII mysqlConn(&conn, sql_connection_pool::getInstance());
 */
class sql_connection_RAII {
public:
    sql_connection_RAII(MYSQL** con, sql_connection_pool* sql_pool);
    ~sql_connection_RAII();
private:
    MYSQL* connRAII;
    sql_connection_pool* sql_pollRAII;
};


#endif
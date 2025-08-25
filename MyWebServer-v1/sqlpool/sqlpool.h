#ifndef SQL_POOL_H
#define SQL_POOL_H

#include <queue>
#include <mysql/mysql.h>
#include <mutex>
#include <semaphore.h>

class SqlPool {
public:
    static SqlPool* GetInstance();
    MYSQL* GetConn();
    void FreeConn(MYSQL* conn);
    int GetFreeConnCount();

    void init(const char* host, int port, const char* username,const char* password,const char* dbname,int connsize);
    void ClosePool();

private:
    SqlPool();
    ~SqlPool();

    int max_conn_;   // 最大连接数
    int user_count_; // 已有连接
    int free_count_; // 空闲连接

    std::queue<MYSQL*> sql_pool;
    std::mutex mtx_;
    sem_t semid_;
};




#endif
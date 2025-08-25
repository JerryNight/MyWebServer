#include "sqlpool.h"
#include "../log/log.h"



SqlPool* SqlPool::GetInstance(){
    static SqlPool sqlpool;
    return &sqlpool;
}
MYSQL* SqlPool::GetConn(){
    // 用信号量控制sql_pool，不用判断empty
    MYSQL* sql = nullptr;
    sem_wait(&semid_);
    {
        std::lock_guard<std::mutex> lock(mtx_);
        sql = sql_pool.front();
        sql_pool.pop();
    }
    return sql;
}
void SqlPool::FreeConn(MYSQL* conn){
    assert(conn);
    std::lock_guard<std::mutex> lock(mtx_);
    sql_pool.push(conn);
    sem_post(&semid_);
}
int SqlPool::GetFreeConnCount(){
    std::lock_guard<std::mutex> lock(mtx_);
    return free_count_;
}

void SqlPool::init(const char* host, int port, const char* username,const char* password,const char* dbname,int connsize){
    int count = 0;
    for (int i = 0 ; i < connsize; i++){
        MYSQL* sql = mysql_init(nullptr);
        if (!sql) LOG_ERROR("MYSQL init error");
        sql = mysql_real_connect(sql, host, username, password, dbname, port, nullptr, 0);
        if (!sql) LOG_ERROR("MYSQL connect error");
        sql_pool.push(sql);
        count++;
    }
    max_conn_ = count;
    // 初始化信号量
    sem_init(&semid_, 0, count);
}
void SqlPool::ClosePool(){
    std::lock_guard<std::mutex> lock(mtx_);
    while (!sql_pool.empty()) {
        auto item = sql_pool.front();
        sql_pool.pop();
        mysql_close(item);
    }
}

SqlPool::SqlPool(){
    user_count_ = 0;
    free_count_ = 0;
}
SqlPool::~SqlPool(){
    ClosePool();
}
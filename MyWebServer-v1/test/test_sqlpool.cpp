#include "../sqlpool/sqlpool.h"
#include "../log/log.h"
#include "../sqlpool/sqlpoolRAII.h"
#include <iostream>

void TestSqlPool(){
    Log::GetInstance()->init(1);
    SqlPool* pool = SqlPool::GetInstance();
    pool->init("127.0.0.1",3306,"root","Lyt996430$","webserverdb",10);
    MYSQL* sql = pool->GetConn();
    mysql_query(sql,"select username,passwd from user");
    MYSQL_RES* res = mysql_use_result(sql);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        std::cout << row[0] << std::endl;
        std::cout << row[1] << std::endl;
    }
}
void TestSqlPoolRAII(){
    Log::GetInstance()->init(1);
    SqlPool* pool = SqlPool::GetInstance();
    pool->init("127.0.0.1",3306,"root","Lyt996430$","webserverdb",10);
    MYSQL* sql;
    SqlPoolRAII sqlraii(&sql, pool);
    mysql_query(sql,"select username,passwd from user");
    MYSQL_RES* res = mysql_use_result(sql);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        std::cout << row[0] << std::endl;
        std::cout << row[1] << std::endl;
    }
}

int main(){
    TestSqlPoolRAII();
}
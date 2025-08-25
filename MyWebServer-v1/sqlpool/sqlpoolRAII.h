#ifndef SQLPOOLRAII_H
#define SQLPOOLRAII_H

#include "sqlpool.h"
#include <cassert>

class SqlPoolRAII {
public:
    SqlPoolRAII(MYSQL** mysql, SqlPool* sqlpool){
        assert(sqlpool);
        mysql_ = sqlpool->GetConn();
        sqlpool_ = sqlpool;
        *mysql = mysql_;
    }
    ~SqlPoolRAII(){
        if (mysql_)
            sqlpool_->FreeConn(mysql_);
    }
private:
    SqlPool* sqlpool_;
    MYSQL* mysql_;
};


#endif
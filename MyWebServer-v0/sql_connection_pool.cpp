#include "sql_connection_pool.h"


    // 私有构造函数、析构函数
sql_connection_pool::sql_connection_pool(){
    _curConn = 0;
    _freeConn = 0;
}
sql_connection_pool::~sql_connection_pool(){
    destroy_pool();
}

void sql_connection_pool::init(string Url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log){
    url = url;
	port = Port;
	user = User;
	passWord = PassWord;
	databaseName = DataBaseName;
	close_log = close_log;
    // _maxConn = MaxConn;
    for (int i = 0; i < MaxConn; i++) {
        // 获取一个MYSQL*
        MYSQL* con = NULL;
        con = mysql_init(con);
        // 连接数据库
        con = mysql_real_connect(con, Url.c_str(),User.c_str(),PassWord.c_str(),DataBaseName.c_str(),Port,NULL,0);
        // 把连接放入连接池
        _connList.push_back(con);
        _freeConn++;
    }
    // 信号量赋值
    reserve = sem(_freeConn);
    _maxConn = _freeConn;
}

sql_connection_pool* sql_connection_pool::get_instance(){
    static sql_connection_pool connPool;
    return &connPool; 
}

MYSQL* sql_connection_pool::get_connection(){
    MYSQL* con = NULL;
    if (_connList.empty()) return NULL;
    // 等待信号量
    reserve.wait();
    // 上锁
    _lock.lock();
    con = _connList.front();
    _connList.pop_front();
    _freeConn--;
    _curConn++;
    // 解锁
    _lock.unlock();
}
bool sql_connection_pool::release_connection(MYSQL* conn){
    if (conn == NULL) return false;
    // 上锁
    _lock.lock();
    _connList.push_back(conn);
    _freeConn++;
    _curConn--;
    // 解锁
    _lock.unlock();
    // 信号量增加
    reserve.post();
    return true;
}
int sql_connection_pool::get_freeconn(){
    return this->_freeConn;
}
void sql_connection_pool::destroy_pool(){
    _lock.lock();
    if (_connList.size() > 0){
        for (auto it = _connList.begin(); it != _connList.end(); it++){
            MYSQL *con = *it;
			mysql_close(con);
        }
        _curConn = 0;
        _freeConn = 0;
        _connList.clear();
    }
    _lock.unlock();
}

    
sql_connection_RAII::sql_connection_RAII(MYSQL** con, sql_connection_pool* sql_pool){
    *con = sql_pool->get_connection();
    connRAII = *con;
    sql_pollRAII = sql_pool;
}

sql_connection_RAII::~sql_connection_RAII(){
    sql_pollRAII->release_connection(connRAII);
}

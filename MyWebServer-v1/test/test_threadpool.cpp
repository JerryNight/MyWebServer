#include "../threadpool/threadpool.h"
#include <iostream>

void test(){
    for (int i = 0; i < 30; i++) {
        std::cout << "i=" << i << " ";
    }
    std::cout << std::endl;
}

void TestThreadPool() {
    ThreadPool pool;
    for (int i = 0; i < 10; i++){
        pool.AddTask(test);
    }
}

int main(){
    TestThreadPool();
}
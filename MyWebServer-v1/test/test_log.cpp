#include <iostream>
#include "../log/log.h"


void TestLog() {
    int cnt = 0, level = 0;
    // Log::GetInstance()->init(level, "./log1", ".log", 0);
    // for(level = 3; level >= 0; level--) {
    //     Log::GetInstance()->SetLevel(level);
    //     for(int j = 0; j < 100; j++ ){
    //         for(int i = 0; i < 4; i++) {
    //             LOG_BASE(i,"%s 111111111 %d ============= ", "Test", cnt++);
    //         }
    //     }
    // }
    cnt = 0;
    Log::GetInstance()->init(level, "./log", ".log", 5000);
    for(level = 0; level < 4; level++) {
        Log::GetInstance()->SetLevel(level);
        for(int j = 0; j < 100; j++ ){
            for(int i = 0; i < 4; i++) {
                LOG_BASE(i,"%s 222222222 %d ============= ", "Test", cnt++);
            }
        }
    }
}

int main(){
    TestLog();
}
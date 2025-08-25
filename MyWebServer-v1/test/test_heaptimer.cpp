#include "../timer/heaptimer.h"
#include <iostream>
#include <thread>

void func1(){
    std::cout << "it is 1 time!" << std::endl;
}
void func2(){
    std::cout << "it is 2 time!" << std::endl;
}
void func3(){
    std::cout << "it is 3 time!" << std::endl;
}

void TestHeapTimer(){
    HeapTimer timer;
    timer.add(1, 2000, func1);
    timer.add(2, 2000, func1);
    timer.add(3, 2000, func1);
    timer.pop();
    timer.pop();
    timer.add(4, 2000, func3);
    timer.add(5, 2000, func2);
    timer.adjust(4, 3000);
    // 1 5 4
    for (int i = 0; i < 10; i++){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        timer.tick();
    }
}

int main(){
    TestHeapTimer();
}
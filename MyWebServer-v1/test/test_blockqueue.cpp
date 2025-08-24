#include "../log/blockqueue.h"
#include <string>
#include <iostream>

void TestBlockQueue() {
    BlockQueue<std::string> bqueue;

    std::cout << "bqueue.capacity(): " << bqueue.capacity() << std::endl;
    std::string str1("liyongtao");
    std::string str2("liyongbo");
    std::string str3("liyongqing");
    std::string str4("liyonggang");
    bqueue.push_back(str1);
    bqueue.push_back(str2);
    bqueue.push_back(str3);
    bqueue.push_front(str4);
    std::cout << "bqueue.size(): " << bqueue.size() << std::endl;
    std::cout << "bqueue.front(): " << bqueue.front() << std::endl;
    std::cout << "bqueue.back(): " << bqueue.back() << std::endl;
    
}

int main(){
    TestBlockQueue();
}
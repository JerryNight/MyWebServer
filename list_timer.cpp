#include "list_timer.h"


sort_timer_lst::sort_timer_lst(){
    head = NULL;
    tail = NULL;
}
sort_timer_lst::~sort_timer_lst(){
    timeNode * next = head;
    while (head != NULL){
        next = head->next;
        delete head;
        head = next;
    }
}

void sort_timer_lst::add_timer(timeNode *timer){
    if (timer == NULL) return;
    // 双向链表为空
    if (head == NULL){
        head = timer;
        tail = timer;
        return;
    }
    // 能插入表头或表尾
    if (timer->expire <= head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    } else if (timer->expire >= tail->expire) {
        timer->prev = tail;
        tail->next = timer;
        tail = timer;
        return;
    }
    add_timer(timer, head);
}
void sort_timer_lst::add_timer(timeNode *timer, timeNode* head){
    while (timer->expire > head->expire) {
        head = head->next;
    }
    // timer 要插在head前面，保证timer的前后都有值
    timer->next = head;
    timer->prev = head->prev;
    head->prev->next = timer;
    head->prev = timer;
}
// 时间增加了三个周期
void sort_timer_lst::adjust_timer(timeNode *timer){
    // 为空，退出
    if (timer == NULL) return;
    // 如果已经是表尾，退出
    if (timer->next == NULL) return;
    // 如果timer本身是头结点,且比后一个节点大
    if (timer == head && timer->next != NULL && timer->expire > timer->next->expire) {
        timeNode* node = head;
        node->next = NULL;
        head = head->next;
        head->prev = NULL;
        add_timer(node, head);
    }
    if (timer->next != NULL && timer->expire > timer->next->expire){
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, head);
    }
}
void sort_timer_lst::del_timer(timeNode *timer){
    if (timer == NULL) return;
    if (timer == head && head == tail) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail){
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick(){
    if (head == NULL) return;
    time_t cur = time(NULL);
    timeNode* node = head;
    timeNode* next = NULL;
    while (node != NULL && node->expire < cur) {
        next = node->next;
        // 到期了，执行清理函数
        node->cb_func(node->user_data);
        delete node;
        next->prev = NULL;
        node = next;
    }
}


#include <iostream>
#include <map>
#include <algorithm>
using namespace std;

struct Node
{
    int key;
    int val;
    Node* pre;
    Node* next;
    Node(int k, int v):key(k), val(v), pre(NULL), next(NULL);
};

class LRUCache{

private:
    int count;
    int size ;
    map<int,Node *> mp;
    Node *cacheHead;
    Node *cacheTail;
    
public:
    LRUCache(int capacity) {
        size = capacity;
        cacheHead = NULL;
        cacheTail = NULL;
        count = 0;
    }
    
    int get(int k) {
        if (cacheHead == NULL) {
            return -1;
        }
        map<int, Node*>::iterator it = mp.find(k);
        if (it == mp.end()) {
            return -1;
        } esle {
            Node* p = it->second;
            pushFront(p);
        }
        return cacheHead->value;
    }
    
    void set(intk, int v) {
        if (cacheHead == NULL) {
            cacheHead = (Node*) malloc(sizeof(Node));
            cacheHead->key = k;
            cacheHead->value = v;
            cacheHead->pre = NULL;
            cacheHead->next = NULL;
            mp[key] = cacheHead;
            cacheTail = cacheHead;
            count++;
        } else {
             map<int,Node *>::iterator it=mp.find(key);
             if (it == mp.end()) {
                 if (count == size) {
                     if (cacheHead==cacheTail&&cacheHead!=NULL) { //只有一个节点
                        mp.erase(cacheHead->key);
                        cacheHead->key = key;
                        cacheHead->value = value;
                        mp[key] = cacheHead;
                     } else {
                         Node * p =cacheTail;
                         cacheTail->pre->next = cacheTail->next;  
                         cacheTail = cacheTail->pre;
                         mp.erase(p->key);
                         p->key= key;
                         p->value = value;
                         p->next = cacheHead;
                         p->pre = cacheHead->pre;
                         cacheHead->pre = p;
                         cacheHead = p;
                         mp[cacheHead->key] = cacheHead;
                     }
                 } else {
                      Node * p = (Node *)malloc(sizeof(Node));
                      p->key = key;
                      p->value = value;
                     
                      p->next = cacheHead;
                      p->pre = NULL;
                      cacheHead->pre = p;
                      cacheHead = p;
                      mp[cacheHead->key] = cacheHead;
                      count++;
                 }
             } else {
                 Node *p = it->second;   
                 p->value = value;
                 pushFront(p);
             }
        }
    }
    void pushFront(Node* cur) {
        //双向链表删除节点,并将节点移动链表头部，O(1)
        if (count == 1) {
            return;
        }
        if (cur == cacheHead) {
            return;
        }
        if (cur == cacheTail) {
            cacheTail = cur->pre;
        }
        
        cur->pre->next = cur->next;
        if (cur->next != NULL) {
            cur->next->pre = cur->pre;
        }
        cur->next = cacheHead;
        cur->pre = NULL;
        cacheHead->pre = cur;
        cacheHead = cur;
    }
}


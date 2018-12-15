struct CacheNode{
    int key;
    int val;
    CacheNode *pre, *next;
    CacheNode(int k, int val): key(k), val(val), pre(NULL), next(NULL);
};

// 设置容量（还可以设置失效时间）
LRUCache(int capacity){
    size = capacity;
    head = NULL;
    tail = NULL;
}

// 节点删除操作
void remove(CacaheNode* node){
    if(node->pre != NULL){
        node->pre->next = node->next;
    }else {
        head = next->next;
    }
    if (node -> next != NULL){
        node-> next->pre = node->pre;
    }else{
        tail = node-> pre;
    }
}

// insert
void setHead(CacheNode* node){
    node->next = head;
    node->pre = NULL;
    
    if (head != NULL){
        head->pre = node;
    }
    head = node;
    if(tail == NULL){
        tail = head;
    }
}

//get
int get(int key){
    map<int, CacheNode*>::iterator iter = mp.find(key);
    if (iter != mp.end()){
        CacheNode* node = it->second;
        remove(node);
        setHead(node);
        return node->val;
    }else{
        return -1;
    }
}

void set(int key, int val){
    map<int, CacheNode*>::iterator it = mp.find(key);
    if (it != mp.end()){
        CacheNode* node = it-> second;
        node->val = val;
        remove(node);
        setHead(node);
    }else{
        CacheNode* newNode = new CacheNode(key, val);
        if (mp.size() > size){
            map<int, CacheNode*>:: iterator it = mp.find(tail->key);
            remove(tail);
            mp.earse(iter);
       }
       setHead(newNode);
       mp[key] = newNode;

    }
}


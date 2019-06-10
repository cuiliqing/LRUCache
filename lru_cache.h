
//********************************************************************
//  
//
//  @date:      2018/6/4 20:18
//  @file:      lru_cache.h
//  @author:    
//  @brief:     本地lrucache工具，支持thread-safe, 目前先简单模拟单线程机制看下效率, 后续再优化, 1w key set需要3ms
//
//********************************************************************
#ifndef RECOMM_BASELIB_LRU_CACHE_H
#define RECOMM_BASELIB_LRU_CACHE_H
#include <cstdlib>
#include <map>
#include <cstring>
#include <base/crc32c.h>
#include <base/logging.h>
#include <base/time.h>
#include <base/scoped_lock.h>
#include <base/containers/hash_tables.h>
// define node type
struct lru_queue_t {
    int id; // keyval id
    int expire;
    int conflict;
    lru_queue_t *prev;
    lru_queue_t *next;
};

class RLockGuard{
public:
    explicit RLockGuard(pthread_rwlock_t& lock) : _plock(&lock) {
        pthread_rwlock_rdlock(_plock);
    };
    ~RLockGuard() {
            pthread_rwlock_unlock(_plock);
    }
private:
    pthread_rwlock_t* _plock;
};
class WLockGuard{
public:
    explicit WLockGuard(pthread_rwlock_t& lock) : _plock(&lock) {
        pthread_rwlock_wrlock(_plock);
    };
    ~WLockGuard() {
            pthread_rwlock_unlock(_plock);
    }
private:
    pthread_rwlock_t* _plock;
};
class LRUQueue {
protected:
    // queue functions
    lru_queue_t* queue_init(int size);
    lru_queue_t* queue_head(lru_queue_t* q);
    lru_queue_t* queue_last(lru_queue_t* q);
    bool queue_is_empty(lru_queue_t* q);
    void queue_insert_head(lru_queue_t* q, lru_queue_t* node);
    void queue_insert_tail(lru_queue_t* q, lru_queue_t* node);
    void queue_remove(lru_queue_t* node);
    void print_queue(lru_queue_t* q);
    void queue_clear(lru_queue_t *q);
};
template <class _K, class _V>
class LRUCache : public LRUQueue{
public:
    typedef  BASE_HASH_NAMESPACE::hash<_K> _HASH; //哈希函数
private:
    int size; //cache数量
    lru_queue_t* free_queue; //空闲队列, 双向链表
    lru_queue_t* cache_queue; //缓存队列, 双向链表
    lru_queue_t* node_list; //节点列表，O(1)
    _K* key_list; //key列表, O(1)
    _V* val_list; //val列表, O(1)
    int* bucket;
    int bucket_size;
    // base::Mutex thread_safe_lock; //缓存读写使用同一个锁,简单模拟单线程
    pthread_spinlock_t slock;
    pthread_rwlock_t rwlock;
public:
    LRUCache();
    ~LRUCache();
    int init(int size, double load_factor=0.5);
    const _V* get(const _K& key);
    bool get(const _K& key, _V& value);
    void set(const _K& key, const _V& value, int ttl = 3600);
    void remove(const _K& key);
    void flush_all();
private:
    // init and clear functions
    void bucket_init(int size, double load_factor=0.5);
    void key_init(int size);
    void val_init(int size);
    void bucket_clear();
    void key_clear();
    void val_clear();
    //mix functions
    int hash_key(const _K& key);
    int hash_string(const _K& key);
    int find_key(const _K& key);
    void insert_key(const _K& key, const _V& value, int node_id);
    int remove_key(const _K& key);
};
/**
 * 构造函数
 */
template <class _K, class _V>
LRUCache<_K, _V>::LRUCache()
        : size(0)
        , free_queue(NULL)
        , cache_queue(NULL)
        , node_list(NULL)
        , key_list(NULL)
        , val_list(NULL)
        , bucket(NULL)
        , bucket_size(0) {
    pthread_spin_init(&slock, 0);
    pthread_rwlock_init(&rwlock, NULL);
}
/**
 * 析构函数
 */
template <class _K, class _V>
LRUCache<_K, _V>::~LRUCache() {
    bucket_clear();
    key_clear();
    val_clear();
    queue_clear(free_queue);
    queue_clear(cache_queue);
    pthread_spin_destroy(&slock);
    pthread_rwlock_destroy(&rwlock);
}
/**
 * 初始化函数
 * @tparam _K
 * @tparam _V
 * @param size
 * @param load_factor
 * @return
 */
template <class _K, class _V>
int LRUCache<_K, _V>::init(int size, double load_factor) {
    bucket_init(size, load_factor);
    key_init(size);
    val_init(size);
    free_queue = queue_init(size);
    cache_queue = queue_init(0);
    node_list = free_queue;
    return 0;
}
/**
 * 初始化哈希表
 * @param size
 * @param load_factor
 */
template <class _K, class _V>
void LRUCache<_K, _V>::bucket_init(int size, double load_factor) {
    if (load_factor > 1) {
        load_factor = 1;
    } else if (load_factor < 0.1) {
        load_factor = 0.1;
    }
    int bs_min = size / load_factor;
    bucket_size = 1;
    while (bucket_size < bs_min) {
        bucket_size *= 2;
    }
    bucket = new int[bucket_size];
    memset(bucket, 0, sizeof(int)*bucket_size);
}
/**
 * 释放哈希表
 */
template <class _K, class _V>
void LRUCache<_K, _V>::bucket_clear() {
    if (bucket != NULL) {
        delete[] bucket;
    }
    bucket_size = 0;
}
/**
 * 初始化 key table
 * @param size
 * @return
 */
template <class _K, class _V>
void LRUCache<_K, _V>::key_init(int size) {
    key_list = new _K[size+1];
}
/**
 * 释放 key table
 */
template <class _K, class _V>
void LRUCache<_K, _V>::key_clear() {
    if (key_list != NULL) {
        delete[] key_list;
    }
}
/**
 * 初始化 val table
 * @param size
 * @return
 */
template <class _K, class _V>
void LRUCache<_K, _V>::val_init(int size) {
    val_list = new _V[size+1];
}
/**
 * 释放 val table
 */
template <class _K, class _V>
void LRUCache<_K, _V>::val_clear() {
    if (val_list != NULL) {
        delete[] val_list;
    }
}
/**
 * 设置一个 key/val with ttl
 * @param key
 * @param value
 * @param ttl
 */
template <class _K, class _V>
void LRUCache<_K, _V>::set(const _K &key, const _V &value, int ttl) {
    // BAIDU_SCOPED_LOCK(thread_safe_lock);
    WLockGuard locker_guard(rwlock);
    int node_id = find_key(key);
    lru_queue_t* node = NULL;
    if (node_id > 0 ) { //不存在的key
        node = &node_list[node_id];
        val_list[node_id] = value;
        //LOG(INFO) << "found";
    } else {
        //LOG(INFO) << "not found: ";
        if (queue_is_empty(free_queue)) { //缓存空间不足
            //从 cache_queue LRU 端淘汰
            node = queue_last(cache_queue);
            remove_key(key_list[node->id]); //淘汰该key
            //LOG(INFO) << "evict" << node->id;
        } else { // 还有剩余空间
            node = queue_head(free_queue);
        }
        insert_key(key, value, node->id);
    }
    queue_remove(node); //从 free_queue 或 cache_queue 队列移除，并插入 cache_queue 头部
    queue_insert_head(cache_queue, node);
    if (ttl > 0) {
        node->expire = ttl + base::gettimeofday_s();
    } else {
        node->expire = -1; // 不过期
    }
}
/**
 * 获取 key
 * @param key
 * @return
 */
template <class _K, class _V>
const _V* LRUCache<_K, _V>::get(const _K &key) {
    // BAIDU_SCOPED_LOCK(thread_safe_lock);
    RLockGuard locker_guard(rwlock);
    int node_id = find_key(key);
    if (node_id > 0) {
        lru_queue_t* node = &node_list[node_id];
        if (node->expire > 0 && node->expire < base::gettimeofday_s()) { //过期
            return NULL;
        }
        {
            BAIDU_SCOPED_LOCK(slock);
            queue_remove(node);
            queue_insert_head(cache_queue, node);
        }
        return &val_list[node_id];
    }
    return NULL;
}
/**
 *
 * @tparam _K
 * @tparam _V
 * @param key
 * @param value
 * @return
 */
template <class _K, class _V>
bool LRUCache<_K, _V>::get(const _K &key, _V &value) {
    // BAIDU_SCOPED_LOCK(thread_safe_lock);
    RLockGuard locker_guard(rwlock);
    int node_id = find_key(key);
    if (node_id > 0) {
        lru_queue_t* node = &node_list[node_id];
        if (node->expire > 0 && node->expire < base::gettimeofday_s()) { //过期
            return false;
        }
        {
            BAIDU_SCOPED_LOCK(slock);
            queue_remove(node);
            queue_insert_head(cache_queue, node);
        }
        value = val_list[node_id];
        return true;
    }
    return false;
}
/**
 * 删除 key
 * @param key
 */
template <class _K, class _V>
void LRUCache<_K, _V>::remove(const _K &key) {
    // BAIDU_SCOPED_LOCK(thread_safe_lock);
    WLockGuard locker_guard(rwlock);
    int node_id = remove_key(key);
    if (node_id <= 0) {
        return;
    }
    queue_remove(&node_list[node_id]);
    queue_insert_tail(free_queue, &node_list[node_id]);
}
/**
 * 清空所有cache
 */
template <class _K, class _V>
void LRUCache<_K, _V>::flush_all() {
    // BAIDU_SCOPED_LOCK(thread_safe_lock);
    WLockGuard locker_guard(rwlock);
    while(!queue_is_empty(cache_queue)) {
        lru_queue_t* node = queue_head(cache_queue);
        remove(key_list[node->id]);
    }
}
/**
 * hash值算法
 * @param key
 * @return
 */
/*
template <class _K, class _V>
int LRUCache<_K, _V>::hash_string(const _K& key) {
    int hv = base::crc32c::Value(key.c_str(), key.size());
    hv = hv & (bucket_size - 1);
    return hv;
}
 */
/**
 *
 * @tparam _K
 * @tparam _V
 * @param key
 * @return
 */
template <class _K, class _V>
int LRUCache<_K, _V>::hash_key(const _K &key) {
    const _HASH& hashfn = _HASH();
    return hashfn(key) & (bucket_size - 1);
}
/**
 * 通过 hash 值查找 node_id
 * @param hash
 * @return
 */
template <class _K, class _V>
int LRUCache<_K, _V>::find_key(const _K& key) {
    int hash = hash_key(key);
    int node_id = bucket[hash];
    while (node_id != 0) {
        lru_queue_t* node = &node_list[node_id];
        if (key_list[node_id] == key) {
            return node_id;
        } else {
            node_id = node_list[node_id].conflict; // 哈希碰撞的下一个节点
        }
    }
    return 0;
}
/**
 * 申请一个node插入一个key/val
 * @param key
 * @param value
 * @param node_id
 */
template <class _K, class _V>
void LRUCache<_K, _V>::insert_key(const _K &key, const _V &value, int node_id) {
    lru_queue_t* node = &node_list[node_id];
    key_list[node_id] = key;
    val_list[node_id] = value;
    int hash = hash_key(key);
    node->conflict = bucket[hash];
    bucket[hash] = node_id;
}
/**
 * 按照 key 删除，并返回对应的 node_id
 * @param key
 * @return
 */
template <class _K, class _V>
int LRUCache<_K, _V>::remove_key(const _K &key) {
    int hash = hash_key(key);
    int prev_id = 0;
    int node_id = bucket[hash];
    while (node_id != 0) {
        lru_queue_t* node = &node_list[node_id];
        if (key_list[node_id] == key) {
            break;
        } else {
            prev_id = node_id;
            node_id = node_list[node_id].conflict; // 哈希碰撞的下一个节点
        }
    }
    if (prev_id > 0) {
        node_list[prev_id].conflict = node_list[node_id].conflict;
    } else {
        bucket[hash] = node_list[node_id].conflict;
    }
    node_list[node_id].conflict = 0;
    //无需重置，下次覆盖即可
    //key_list[node_id] = "";
    //val_list[node_id] = "";
    return node_id;
}
#endif //RECOMM_BASELIB_LRU_CACHE_H

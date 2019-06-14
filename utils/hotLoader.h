/*
 * =====================================================================================
 *
 *       Filename:  HotLoader.h
 *
 * =====================================================================================
 */
/*
 //使用举例：
 //启动监控线程
 class A {
 void func() {
 cout << "ok"<<endl;
 }
 };
 int main() {
 const char* data_path = "./";
 const char* conf_file = "ba_frame.conf";//conf内需要含[hot_loader]配置
 int ret = HotLoaderMonitor::Global_init(data_path, conf_file);
 if (ret != 0) {
 fprintf(stderr, "start monitor failed ret=%d", ret);
 return -1;
 }
 HotLoader<A> a2;
 if( a2.init(sample_reload,"keyxxx") != 0){
 return -2;//failed
 }
 a2->func();
 HotLoaderMonitor::Global_release();
 return 0;
 }
 //*/
#ifndef INCLUDE_HOTLOADER_H_
#define INCLUDE_HOTLOADER_H_
#define HOT_LOADER_VERSION "1.1"
#include <gflags/gflags.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <map>
#include <string>
#include <typeinfo>
DECLARE_string(hotloader_conf_path);
DECLARE_string(hotloader_conf_file);
DECLARE_int32(hotloader_interval);
#define HL_CONF_MAX_LEN 1024
//#define HOT_LOADER_USE_BOOST
#ifndef HOT_LOADER_USE_BOOST  // use c++11
#if __cplusplus <= 199711L    // or #if __cplusplus >= 201103L
#pragma message("HotLoader needs at least a C++11 compliant compiler;use boost instead")
//#error
#define HOT_LOADER_USE_BOOST
#endif
#endif
#ifndef HOT_LOADER_USE_BOOST  // use c++11
#include <memory>
#define HOT_LOADER_PTR_TYPE std::shared_ptr
#else
#include "boost/shared_ptr.hpp"
#define HOT_LOADER_PTR_TYPE boost::shared_ptr
#endif
/**
 * 示例：热加载回调函数，data_path为key对应的value值。若未设置触发key，data_path值为conf的绝对路径
 * @return 0：加载成功，1：无需更新,<0 加载失败
 * 注意：外层负责指针的内存释放
 */
template <class T>
int sample_reload(const char* data_path, T*& another) {
    if (NULL == data_path || '\0' == *data_path) {
        return -1;
    }
    another = new (std::nothrow) T();
    if (another == NULL) {
        return -2;
    }
    return 0;
}
namespace wsy {
/* 抽象工厂*/
class HotLoaderAbstractFactory {
public:
    HotLoaderAbstractFactory() { _is_ready = false; }
    virtual ~HotLoaderAbstractFactory() {}
    virtual int reload(bool force = false) = 0;
    virtual const char* get_class_name()   = 0;
    bool _is_ready;
};
/**
 * 内部使用函数：监控线程函数
 */
void* monitor_thread(void* arg);
/* 监控类*/
class HotLoaderMonitor {
public:
    HotLoaderMonitor(HotLoaderAbstractFactory* fact);
    virtual ~HotLoaderMonitor(void);
    static int Global_Init(const std::string& conf_path = FLAGS_hotloader_conf_path,
                           const std::string& conf_file = FLAGS_hotloader_conf_file,
                           int32_t interval             = FLAGS_hotloader_interval);
    static void Global_Release();
    static bool Global_isReady() { return G_isReady; }
    friend void* monitor_thread(void* arg);
public:
    typedef std::pair<std::string, long> VALUE;
    static char hl_conf_path[HL_CONF_MAX_LEN];
    static char hl_conf_file[HL_CONF_MAX_LEN];
    static std::map<std::string, VALUE> update_kv;
    static std::map<std::string, VALUE> global_kv_dict;
private:
    HotLoaderMonitor* _next;
    HotLoaderAbstractFactory* _item;
    static HotLoaderMonitor* G_head;
    static bool G_isReady;
};
/* realization of class factory */
template <class T>
class HotLoader : public HotLoaderAbstractFactory {
public:
    typedef HOT_LOADER_PTR_TYPE<T> Ptr;
public:
    // explicit关键字用来修饰类的构造函数，被修饰的构造函数的类，不能发生相应的隐式类型转换，只能以显示的方式进行类型转换。
    explicit HotLoader() : _monitor(this) {
        strncpy(_class_name, typeid(T).name(), sizeof(_class_name));
        _recall_func  = NULL;
        *_trigger_key = '\0';
        _is_ready     = false;
    }
    virtual ~HotLoader() {}
    /**
     * 初始化操作
     * @param int (*func)(const char*, T*&)回调函数
     * @param key,hot_loader conf文件内才key
     * 若指定了key（例如：bainfo_dict_path），则key发生变化时触发更新操作。传递给回调函数的为key对应的值
     * 若未指定key，则conf文件变化时触发更新操作,传递给回调函数为conf的绝对路径。
     */
    int init(int (*func)(const char*, T*&), const char* trigger_key = NULL) {
        _recall_func = func;
        if (trigger_key) {
            strncpy(_trigger_key, trigger_key, sizeof(_trigger_key));
        }
        int ret = this->reload(true);
        if (ret == 0) {
            _is_ready = true;
        }
        return ret;
    }
    //如果 定义了 operator-> 操作符的类的一个对象，
    //则 point->action 与 point.operator->()->action 相同。即，执行 point 的
    // operator->()， 然后使用该结果重复这三步
    //重载箭头操作符必须返回指向类类型的指针，或者返回定义了自己的箭头操作符的类类型对象
    // WARNING：为了使智能指针引用计数，此处不能返回引用。为此也屏蔽了operator*()重载
    //注:HotLoader<T> a; a->action();
    //含义同：a.operator->().operator->()->action();
    HotLoader::Ptr operator->() const {
        // if( !_sp0 ){// null pointer will coredump by share_ptr
        return _sp0;
    }
    //注意：返回的类型是HotLoader::Ptr，与重载->()相同，方便智能指针引用计数
    //使用方法：HotLoader<T> a;
    //**a.action();第一个星花返回HotLoader::Ptr对象,并增加一次引用,第二个星花返回T&
    HotLoader::Ptr operator*() const { return _sp0; }
    HotLoader::Ptr get() const { return _sp0; }
    bool is_ready() const { return this->_is_ready; }
    /**
     * 热加载执行函数
     */
    int reload(bool force) {
        if (!_monitor.Global_isReady() || NULL == _recall_func) {  //未初始化
            // printf("ready:%d,recall:%p\n",_monitor.Global_isReady(),_recall_func);
            return -100;
        }
        T* another                                             = NULL;
        int ret                                                = 0;
        std::map<std::string, HotLoaderMonitor::VALUE>* kv_map = NULL;
        if (force) {  // force update,kv is global data
            kv_map = &(_monitor.global_kv_dict);
        } else {
            kv_map = &(_monitor.update_kv);
        }
        if ('\0' != *_trigger_key) {
            if (kv_map->find(_trigger_key) == kv_map->end()) {
                // key 无需更新
                return 1;
            }
            const char* value = (*kv_map)[_trigger_key].first.c_str();
            ret               = _recall_func(value, another);
        } else {  //未设置触发key，value 传递conf的全路径
            char abs_file[HL_CONF_MAX_LEN];
            snprintf(abs_file, HL_CONF_MAX_LEN, "%s/%s", _monitor.hl_conf_path,
                     _monitor.hl_conf_file);
            ret = _recall_func(abs_file, another);
        }
        if (ret != 0) {  // 无需替换 或替换失败
            return ret;
        }
        if (another == NULL) {  // failed
            return -101;
        }
        // todo: 测试逻辑是否符合预期
        //将指针保存到内部变量中，函数结束会自动释放。若符合预期，此处逻辑可省略
        // HotLoader:Ptr _sp1 = _sp0;
        _sp0.reset(another);
        malloc_trim(0);
        return 0;
    }
    const char* get_class_name() { return _class_name; }
private:
    //禁止拷贝构造函数，防止智能指针出错、不可控
    /*
         HotLoader& operator=(HotLoader &a) {
         return *this;
         }*/
    char _class_name[1024];
    char _trigger_key[HL_CONF_MAX_LEN];
    HotLoader::Ptr _sp0;
    HotLoaderMonitor _monitor;
    int (*_recall_func)(const char*, T*&);
};
}  // namespace wsy
/* namespace wsy */
#endif /* INCLUDE_HOTLOADER_H_ */


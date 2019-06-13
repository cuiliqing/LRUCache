/*
 * =====================================================================================
 *
 *       Filename:  HotLoader.cpp
 *
 * =====================================================================================
 */
#include "HotLoader.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <map>
#include <string>
#include <Configure.h>
/*
#define HL_LOG_FATAL UB_LOG_FATAL
#define HL_LOG_WARNING UB_LOG_WARNING
#define HL_LOG_NOTICE UB_LOG_NOTICE
//屏蔽trace日志
#ifndef HL_NO_TRACE
#define HL_LOG_TRACE UB_LOG_TRACE
#else
#define HL_LOG_DEBUG(fmt, arg...) \
                do {} while(0)
#define HL_NO_DEBUG
#endif
//屏蔽debug日志
#ifndef HL_NO_DEBUG
#define HL_LOG_DEBUG UB_LOG_DEBUG
#else
#define HL_LOG_DEBUG(fmt, arg...) \
    do {} while(0)
#endif
 */
using namespace std;
namespace wsy {
HotLoaderMonitor* HotLoaderMonitor::G_head           = 0;
bool HotLoaderMonitor::G_isReady                     = false;
char HotLoaderMonitor::hl_conf_path[HL_CONF_MAX_LEN] = "";
char HotLoaderMonitor::hl_conf_file[HL_CONF_MAX_LEN] = "";
std::map<std::string, HotLoaderMonitor::VALUE> HotLoaderMonitor::update_kv;
/**
 * 全局key/value索引
 */
std::map<std::string, HotLoaderMonitor::VALUE> HotLoaderMonitor::global_kv_dict;
/**
 * map中的hotloader conf key
 */
static const char hl_conf_key_name[] = "\twsy";
//更新线程所需要的数据
static pthread_t update_data_tid; /* 更新线程 */
static pthread_mutex_t hl_init_mutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * 标志更新线程是否需要结束
 */
static bool G_running_status;
static int32_t check_time_second_gap = 1;  //检查时间间隔
int get_file_timestamp(const char* file, time_t& create_time, const char* path = NULL) {
    char abs_file[HL_CONF_MAX_LEN];
    if (NULL != path) {
        snprintf(abs_file, HL_CONF_MAX_LEN, "%s/%s", path, file);
    } else {
        strncpy(abs_file, file, HL_CONF_MAX_LEN);
        abs_file[HL_CONF_MAX_LEN - 1] = '\0';
    }
    create_time = 0;
    struct stat file_stat;
    if (0 != stat(abs_file, &file_stat)) {
        return -1;
    }
    create_time = file_stat.st_mtime;
    return 0;
}
long get_timestamp(std::map<std::string, HotLoaderMonitor::VALUE>& dict, const char* key) {
    if (NULL == key || '\0' == key) {
        return 0L;
    }
    long timestamp = 0L;
    if (dict.find(key) != dict.end()) {
        timestamp = dict[key].second;
    }
    return timestamp;
}
std::map<std::string, HotLoaderMonitor::VALUE> get_change_kv(
    std::map<std::string, HotLoaderMonitor::VALUE>& kv_dict,
    char* data_path,
    char* conf_file) {
    std::map<std::string, HotLoaderMonitor::VALUE> update_kv;
    if (NULL == data_path || NULL == conf_file || '\0' == *data_path || '\0' == *conf_file) {
        // HL_LOG_WARNING("hot loader conf data_path[%p] or conf_file[%p] is
        // NULL", data_path,
        //		conf_file);
        return update_kv;
    }
    int err_code = 0;
    comcfg::Configure conf;
    comcfg::Configure lastconf;
    // load new conf
    if (0 != conf.load(data_path, conf_file)) {
        // HL_LOG_WARNING("open configure failed![%s][%s]", data_path,
        // conf_file);
        return update_kv;
    }
    for (int i = 0; i < (int)conf.size(); ++i) {
        const comcfg::ConfigUnit& one_key = conf.get_sub_unit(i);
        if (one_key.selfType() == comcfg::CONFIG_ERROR_TYPE) {
            // HL_LOG_WARNING("read conf[%d] failed", i);
            continue;
        }
        const char* name  = one_key.get_key_name().c_str();
        const char* value = one_key.to_cstr(&err_code);
        if (err_code != 0 || NULL == value) {
            // HL_LOG_WARNING("key[%s] err_code[%d]", name, err_code);
            continue;
        }
        time_t create_time = 0L;
        err_code           = get_file_timestamp(value, create_time);
        if (kv_dict.find(name) == kv_dict.end()) {  // new key
            update_kv[name] = HotLoaderMonitor::VALUE(value, create_time);
        } else {  // old key
            if (kv_dict[name].first != value
                || create_time - kv_dict[name].second
                       > 0) {  // value changed or value timestamp changed
                update_kv[name] = HotLoaderMonitor::VALUE(value, create_time);
            }
        }
    }  // for i
    return update_kv;
}
void update_dict(std::map<std::string, HotLoaderMonitor::VALUE>& kv_dict,
                 std::map<std::string, HotLoaderMonitor::VALUE>& update_kv) {
    for (std::map<std::string, HotLoaderMonitor::VALUE>::iterator it = update_kv.begin();
         it != update_kv.end(); ++it) {
        kv_dict[it->first] = it->second;
    }
}
void* monitor_thread(void* arg) {
    /*
     * create a thread to keep track of conf.data_file's update
     * and update the memory data pointed by qid_list_p if necessary
     */
    // start
    G_running_status = true;
    while (G_running_status) {
        int ret = 0;
        time_t mtime_new;
        ret            = get_file_timestamp(HotLoaderMonitor::hl_conf_file, mtime_new,
                                 HotLoaderMonitor::hl_conf_path);
        long mtime_old = get_timestamp(HotLoaderMonitor::global_kv_dict, hl_conf_key_name);
        long cmp       = (long)mtime_new - mtime_old;
        if (cmp > 0) {  //启动遍历更新
            HotLoaderMonitor::update_kv =
                get_change_kv(HotLoaderMonitor::global_kv_dict, HotLoaderMonitor::hl_conf_path,
                              HotLoaderMonitor::hl_conf_file);
            const HotLoaderMonitor* cur = HotLoaderMonitor::G_head;
            for (; cur; cur = cur->_next) {
                /* if class_name matched, object will then be created and
                 * returned */
                if (!cur->_item->_is_ready) {
                    continue;
                }
                ret = cur->_item->reload();
                if (ret == 0) {
                    // HL_LOG_TRACE("update for [%s] success",
                    // cur->_item->get_class_name());
                } else if (ret < 0) {
                    // HL_LOG_WARNING("update for [%s] failed ret[%d]",
                    // cur->_item->get_class_name(),
                    //		ret);
                } else if (ret > 0) {
                    // HL_LOG_DEBUG("update for [%s] skipped ret[%d]",
                    // cur->_item->get_class_name(),
                    //		ret);
                }
            }
            HotLoaderMonitor::update_kv[hl_conf_key_name] =
                std::pair<std::string, long>("", mtime_new);
            update_dict(HotLoaderMonitor::global_kv_dict, HotLoaderMonitor::update_kv);
        }  // if (cmp > 0) {
        // HL_LOG_DEBUG("hot loader compare_timestamp[%ld]", cmp);
        sleep(check_time_second_gap);  // 1秒检查一次
    }
    return NULL;
}
int HotLoaderMonitor::Global_Init(const std::string& conf_path,
                                  const std::string& conf_file,
                                  int32_t interval) {
    if (G_isReady) {
        return 0;
    }
    // load conf
    int err_code = 0;
    /*
        comcfg::Configure conf;
        if (0 != conf.load(data_path, conf_file)) {
                //HL_LOG_FATAL("open configure failed!");
                return -1;
        }
        const comcfg::ConfigUnit& hot_loader_conf = conf["hot_loader"];
        if (hot_loader_conf.selfType() == comcfg::CONFIG_ERROR_TYPE) {
                //HL_LOG_FATAL("read conf[ hot_loader ] failed");
                return -2;
        }
        // conf :  load conf path DEFAULT:""
        const char* conf_path = hot_loader_conf["conf_path"].to_cstr(&err_code);
        if (err_code != 0) {
                //HL_LOG_FATAL("read conf[ conf_path ] failed");
                return -3;
        }
        strncpy(hl_conf_path, conf_path, HL_CONF_MAX_LEN);
        hl_conf_path[HL_CONF_MAX_LEN - 1] = '\0';
        // conf :  load conf file DEFAULT:""
        const char* conf_file_path =
       hot_loader_conf["conf_file"].to_cstr(&err_code); if (err_code != 0) {
                //HL_LOG_FATAL("read conf[ conf_file ] failed");
                return -4;
        }
        strncpy(hl_conf_file, conf_file_path, HL_CONF_MAX_LEN);
        hl_conf_file[HL_CONF_MAX_LEN - 1] = '\0';
        int _check_time_second_gap =
       hot_loader_conf["check_time_second_gap"].to_int32(&err_code); if
       (err_code == 0 && _check_time_second_gap >= 1) { check_time_second_gap =
       _check_time_second_gap; } else {
                //HL_LOG_TRACE("check_time_second_gap use default[%d]",
       check_time_second_gap);
        }
         */
    strncpy(hl_conf_path, conf_path.c_str(), HL_CONF_MAX_LEN);
    hl_conf_path[HL_CONF_MAX_LEN - 1] = '\0';
    strncpy(hl_conf_file, conf_file.c_str(), HL_CONF_MAX_LEN);
    hl_conf_file[HL_CONF_MAX_LEN - 1] = '\0';
    check_time_second_gap = interval;
    //=============== init kv map =========================
    time_t mtime_new;
    get_file_timestamp(conf_file.c_str(), mtime_new, conf_path.c_str());
    global_kv_dict = get_change_kv(global_kv_dict, const_cast<char*>(conf_path.c_str()),
                                   const_cast<char*>(conf_file.c_str()));
    global_kv_dict[hl_conf_key_name] = std::pair<std::string, long>("", mtime_new);
    //=====================================================
    if (0 != pthread_create(&update_data_tid, NULL, monitor_thread, NULL)) {
        // HL_LOG_FATAL("create thread for %s failed", "HotLoaderMonitor");
        return -7;
    }
    G_isReady = true;
    return 0;
}
void HotLoaderMonitor::Global_Release() {
    if (G_isReady) {
        void* status;
        G_running_status = false;
        pthread_join(update_data_tid, &status);
    }
    G_isReady = false;
}
/* ctor of HotLoaderMonitor, add a class factory to list */
HotLoaderMonitor::HotLoaderMonitor(HotLoaderAbstractFactory* fact) : _item(fact) {
    //加锁,保证链表正确
    pthread_mutex_lock(&hl_init_mutex);
    _next  = G_head;
    G_head = this;
    pthread_mutex_unlock(&hl_init_mutex);
}
/* delete linkage from CFactoryList when some class factory destroyed */
HotLoaderMonitor::~HotLoaderMonitor() {
    //加锁
    pthread_mutex_lock(&hl_init_mutex);
    HotLoaderMonitor** m_nextp = &HotLoaderMonitor::G_head;
    for (; *m_nextp; m_nextp = &(*m_nextp)->_next) {
        if (*m_nextp == this) {
            *m_nextp = (*m_nextp)->_next;
            break;
        }
    }
    pthread_mutex_unlock(&hl_init_mutex);
}
}  // namespace wsy


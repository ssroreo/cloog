//
// Created by chaichengxun on 2021/12/21.
//

#ifndef CLOOG_CLOOG_H
#define CLOOG_CLOOG_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <sys/time.h>
#include <mutex>
#include <condition_variable>

enum LOG_LEVEL
{
    FATAL = 1,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
};

struct utc_timer
{
    utc_timer()
    {
        time_t now_sec = time(0);
        //set _sys_acc_sec, _sys_acc_min
        _sys_acc_sec = now_sec;
        _sys_acc_min = _sys_acc_sec / 60;
        //use _sys_acc_sec calc year, mon, day, hour, min, sec
        struct tm cur_tm;
        localtime_r(&now_sec, &cur_tm);
        year = cur_tm.tm_year + 1900;
        mon  = cur_tm.tm_mon + 1;
        day  = cur_tm.tm_mday;
        hour = cur_tm.tm_hour;
        min  = cur_tm.tm_min;
        sec  = cur_tm.tm_sec;
        reset_utc_fmt();
    }

    uint64_t get_curr_time(int* p_msec = nullptr)
    {
        time_t now_sec = time(0);
        if (p_msec)
            *p_msec = now_sec;
        //if not in same seconds
        if (now_sec != _sys_acc_sec)
        {
            sec = now_sec % 60;
            _sys_acc_sec = now_sec;
            //or if not in same minutes
            if (_sys_acc_sec / 60 != _sys_acc_min)
            {
                //use _sys_acc_sec update year, mon, day, hour, min, sec
                _sys_acc_min = _sys_acc_sec / 60;
                struct tm cur_tm;
                localtime_r(&now_sec, &cur_tm);
                year = cur_tm.tm_year + 1900;
                mon  = cur_tm.tm_mon + 1;
                day  = cur_tm.tm_mday;
                hour = cur_tm.tm_hour;
                min  = cur_tm.tm_min;
                //reformat utc format
                reset_utc_fmt();
            }
            else
            {
                //reformat utc format only sec
                reset_utc_fmt_sec();
            }
        }
        return now_sec;
    }

    int year, mon, day, hour, min, sec;
    char utc_fmt[20];

private:
    void reset_utc_fmt()
    {
        snprintf(utc_fmt, 20, "%d-%02d-%02d %02d:%02d:%02d", year, mon, day, hour, min, sec);
    }

    void reset_utc_fmt_sec()
    {
        snprintf(utc_fmt + 17, 3, "%02d", sec);
    }

    uint64_t _sys_acc_min;
    uint64_t _sys_acc_sec;
};

class cell_buffer
{
public:
    enum buffer_status
    {
        FREE,
        FULL
    };

    explicit cell_buffer(uint32_t len):
            _status(FREE),
            _prev(nullptr),
            _next(nullptr),
            _total_len(len),
            _used_len(0)
    {
        _data = new char[len];
        if (!_data)
        {
            fprintf(stderr, "no space to allocate _data\n");
            exit(1);
        }
    }

    uint32_t avail_len() const { return _total_len - _used_len; }

    bool empty() const { return _used_len == 0; }

    void append(const char* log_line, uint32_t len)
    {
        if (avail_len() < len)
            return ;
        memcpy(_data + _used_len, log_line, len);
        _used_len += len;
    }

    void clear()
    {
        _used_len = 0;
        _status = FREE;
    }

    void persist(FILE* fp)
    {
        uint32_t wt_len = fwrite(_data, 1, _used_len, fp);
        if (wt_len != _used_len)
        {
            fprintf(stderr, "write log to disk error, wt_len %u\n", wt_len);
        }
    }

    buffer_status _status;

    cell_buffer* _prev;
    cell_buffer* _next;

private:
    cell_buffer(const cell_buffer&);
    cell_buffer& operator=(const cell_buffer&);

    uint32_t _total_len;
    uint32_t _used_len;
    char* _data;
};

class cloog
{
public:
    //for thread-safe singleton
    static cloog* ins()
    {
        std::call_once(_once, cloog::init);
        return _ins;
    }

    static void init()
    {
        while (!_ins) _ins = new cloog();
    }

    void init_path(const char* log_dir, const char* prog_name, int level);

    int get_level() const { return _level; }

    void persist();

    void try_append(const char* lvl, const char* format, ...);

    void be_thdo();

private:
    cloog();

    bool decis_file(int year, int mon, int day);

    cloog(const cloog&);
    const cloog& operator=(const cloog&);

    int _buff_cnt;

    cell_buffer* _curr_buf;
    cell_buffer* _prst_buf;

//    cell_buffer* _last_buf;

    FILE* _fp;
    pid_t _pid;
    int _year, _mon, _day, _log_cnt;
    char _prog_name[128];
    char _log_dir[512];

    bool _env_ok;//if log dir ok
    int _level;
    uint64_t _lst_lts;//last can't log error time(s) if value != 0, log error happened last time

    utc_timer _tm;

    static std::mutex _mutex;
    static std::mutex _cond_mutex;
    static std::condition_variable _cond;

    static uint32_t _one_buff_len;

    //singleton
    static cloog* _ins;
    static std::once_flag _once;
};

#define LOG_MEM_SET(mem_lmt) \
do \
{ \
if (mem_lmt < 90 * 1024 * 1024) \
{ \
mem_lmt = 90 * 1024 * 1024; \
} \
else if (mem_lmt > 1024 * 1024 * 1024) \
{ \
mem_lmt = 1024 * 1024 * 1024; \
} \
cloog::_one_buff_len = mem_lmt; \
} while (0)

#define LOG_INIT(log_dir, prog_name, level) \
do \
{ \
cloog::ins()->init_path(log_dir, prog_name, level); \
std::thread t(&cloog::be_thdo, cloog::ins()); \
t.detach(); \
} while (0)

//format: [LEVEL][yy-mm-dd h:m:s.ms][tid]file_name:line_no(func_name):content
#define LOG_TRACE(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= TRACE) \
{ \
cloog::ins()->try_append("[TRACE]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} \
} while (0)

#define LOG_DEBUG(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= DEBUG) \
{ \
cloog::ins()->try_append("[DEBUG]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} \
} while (0)

#define LOG_INFO(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= INFO) \
{ \
cloog::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} \
} while (0)

#define LOG_NORMAL(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= INFO) \
{ \
cloog::ins()->try_append("[INFO]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} \
} while (0)

#define LOG_WARN(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= WARN) \
{ \
cloog::ins()->try_append("[WARN]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} \
} while (0)

#define LOG_ERROR(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= ERROR) \
{ \
cloog::ins()->try_append("[ERROR]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args);            \
} \
} while (0)

#define LOG_FATAL(fmt, args...) \
do \
{ \
cloog::ins()->try_append("[FATAL]", "[%u]%s:%d(%s): " fmt "\n", \
std::this_thread::get_id(), __FILE__, __LINE__, __FUNCTION__, ##args); \
} while (0)

#endif //CLOOG_CLOOG_H

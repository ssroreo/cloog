//
// Created by chaichengxun on 2021/12/21.
//

#ifndef CLOOG_CLOOG_H
#define CLOOG_CLOOG_H

#include <cstdio>
#include <cstring>
#include <thread>
#include <ctime>
#include <chrono>
#include <mutex>
#include <condition_variable>

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
#define BOLD_ON_RED "\033[1m\033[41m"      /* Bold Red Background*/


enum LOG_LEVEL
{
    FATAL = 1,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
};

struct utc_timer;

class cell_buffer;

class cloog
{
public:
    //for thread-safe singleton
    static cloog* ins();

    static void init();

    void init_path(const char* log_dir, const char* prog_name, int level);

    int get_level() const { return _level; }

    void persist();

    void try_append(const char* lvl, const char* format, ...);

    void be_thdo();

    void set_max_mem(const uint64_t max_mem);

    void set_max_filesize(const uint64_t max_filesize);

private:
    cloog();

    bool decis_file(int year, int mon, int day);

    cloog(const cloog&) = delete;
    const cloog& operator=(const cloog&) = delete;

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

    utc_timer* _tm;

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
cloog::ins()->try_append("[TRACE]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);    \
printf(WHITE fmt RESET "\n", ##args);               \
} \
} while (0)

#define LOG_DEBUG(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= DEBUG) \
{ \
cloog::ins()->try_append("[DEBUG]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args); \
printf(CYAN fmt RESET "\n", ##args);            \
} \
} while (0)

#define LOG_INFO(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= INFO) \
{ \
cloog::ins()->try_append("[INFO]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);   \
printf(GREEN fmt RESET "\n", ##args);  \
} \
} while (0)

#define LOG_NORMAL(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= INFO) \
{ \
cloog::ins()->try_append("[INFO]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);  \
printf(GREEN fmt RESET "\n", ##args); \
} \
} while (0)

#define LOG_WARN(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= WARN) \
{ \
cloog::ins()->try_append("[WARN]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);  \
printf(BOLDYELLOW fmt RESET "\n", ##args); \
} \
} while (0)

#define LOG_ERROR(fmt, args...) \
do \
{ \
if (cloog::ins()->get_level() >= ERROR) \
{ \
cloog::ins()->try_append("[ERROR]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);        \
printf(BOLDRED fmt RESET "\n", ##args);     \
} \
} while (0)

#define LOG_FATAL(fmt, args...) \
do \
{ \
cloog::ins()->try_append("[FATAL]", "[0x%x]: " fmt "\n", \
std::this_thread::get_id(), ##args);                     \
printf(BOLD_ON_RED  fmt RESET "\n", ##args);             \
} while (0)

#define LOG_MEMUSE(size) \
do                       \
{                        \
  cloog::ins()->set_max_mem(size); \
}while(0)                \

#define LOG_FILESIZE(size) \
do                       \
{                        \
  cloog::ins()->set_max_filesize(size); \
}while(0)                \

#endif //CLOOG_CLOOG_H

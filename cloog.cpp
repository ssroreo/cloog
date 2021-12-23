//
// Created by chaichengxun on 2021/12/21.
//

#include "cloog.h"

#include <errno.h>
#include <unistd.h>//access
#include <assert.h>//assert
#include <stdarg.h>//va_list
#include <sys/stat.h>//mkdir

static uint64_t MEM_USE_LIMIT   = (3u * 1024 * 1024 * 1024);
static uint64_t LOG_USE_LIMIT   = (1u * 1024 * 1024 * 1024);
static uint64_t LOG_LEN_LIMIT   = (4 * 1024);
static uint64_t RELOG_THRESOLD  = 5;

std::mutex cloog::_mutex;
std::mutex cloog::_cond_mutex;
std::condition_variable cloog::_cond;

cloog* cloog::_ins = nullptr;
std::once_flag cloog::_once;
uint32_t cloog::_one_buff_len = 30*1024*1024;//30MB

cloog::cloog():
        _buff_cnt(3),
        _curr_buf(nullptr),
        _prst_buf(nullptr),
        _fp(nullptr),
        _log_cnt(0),
        _env_ok(false),
        _level(INFO),
        _lst_lts(0),
        _tm()
{
    //create double linked list
    cell_buffer* head = new cell_buffer(_one_buff_len);
    if (!head)
    {
        fprintf(stderr, "no space to allocate cell_buffer\n");
        exit(1);
    }
    cell_buffer* current;
    cell_buffer* prev = head;
    for (int i = 1;i < _buff_cnt; ++i)
    {
        current = new cell_buffer(_one_buff_len);
        if (!current)
        {
            fprintf(stderr, "no space to allocate cell_buffer\n");
            exit(1);
        }
        current->_prev = prev;
        prev->_next = current;
        prev = current;
    }
    prev->_next = head;
    head->_prev = prev;

    _curr_buf = head;
    _prst_buf = head;

    _pid = getpid();
}

void cloog::init_path(const char* log_dir, const char* prog_name, int level)
{
    std::lock_guard<std::mutex>lock(_mutex);

    strncpy(_log_dir, log_dir, 512);
    //name format:  name_year-mon-day-t[tid].log.n
    strncpy(_prog_name, prog_name, 128);

    mkdir(_log_dir, 0777);
    //查看是否存在此目录、目录下是否允许创建文件
    if (access(_log_dir, F_OK | W_OK) == -1)
    {
        fprintf(stderr, "logdir: %s error: %s\n", _log_dir, strerror(errno));
    }
    else
    {
        _env_ok = true;
    }
    if (level > TRACE)
        level = TRACE;
    if (level < FATAL)
        level = FATAL;
    _level = level;
}

void cloog::persist()
{
    while (true)
    {
        {
            //check if _prst_buf need to be persist
            std::lock_guard<std::mutex>lock(_mutex);
            if (_prst_buf->_status == cell_buffer::FREE)
            {
                std::unique_lock<std::mutex> lk(_cond_mutex);
                _cond.wait_for(lk, std::chrono::seconds(1), []{return true;});
            }
            if (_prst_buf->empty())
            {
                //give up, go to next turn
                continue;
            }
            if (_prst_buf->_status == cell_buffer::FREE)
            {
                assert(_curr_buf == _prst_buf);//to test
                _curr_buf->_status = cell_buffer::FULL;
                _curr_buf = _curr_buf->_next;
            }
        }
        //decision which file to write
        int year = _tm.year, mon = _tm.mon, day = _tm.day;
        if (!decis_file(year, mon, day))
            continue;
        //write
        _prst_buf->persist(_fp);
        fflush(_fp);
        {
            std::lock_guard<std::mutex>lock(_mutex);
            _prst_buf->clear();
            _prst_buf = _prst_buf->_next;
        }
    }
}

void cloog::try_append(const char* lvl, const char* format, ...)
{
    int ms;
    uint64_t curr_sec = _tm.get_curr_time(&ms);
    if (_lst_lts && curr_sec - _lst_lts < RELOG_THRESOLD)
        return ;

    char log_line[LOG_LEN_LIMIT];
    int prev_len = snprintf(log_line, LOG_LEN_LIMIT, "%s[%s.%03d]", lvl, _tm.utc_fmt, ms);

    va_list arg_ptr;
    va_start(arg_ptr, format);

    //TO OPTIMIZE IN THE FUTURE: performance too low here!
    int main_len = vsnprintf(log_line + prev_len, LOG_LEN_LIMIT - prev_len, format, arg_ptr);

    va_end(arg_ptr);

    uint32_t len = prev_len + main_len;

    _lst_lts = 0;
    bool tell_back = false;

    {
        std::lock_guard<std::mutex>lock(_mutex);
        if (_curr_buf->_status == cell_buffer::FREE && _curr_buf->avail_len() >= len)
        {
            _curr_buf->append(log_line, len);
        }
        else
        {
            //1. _curr_buf->status = cell_buffer::FREE but _curr_buf->avail_len() < len
            //2. _curr_buf->status = cell_buffer::FULL
            if (_curr_buf->_status == cell_buffer::FREE)
            {
                _curr_buf->_status = cell_buffer::FULL;//set to FULL
                cell_buffer* next_buf = _curr_buf->_next;
                //tell backend thread
                tell_back = true;

                //it suggest that this buffer is under the persist job
                if (next_buf->_status == cell_buffer::FULL)
                {
                    //if mem use < MEM_USE_LIMIT, allocate new cell_buffer
                    if (_one_buff_len * (_buff_cnt + 1) > MEM_USE_LIMIT)
                    {
                        fprintf(stderr, "no more log space can use\n");
                        _curr_buf = next_buf;
                        _lst_lts = curr_sec;
                    }
                    else
                    {
                        cell_buffer* new_buffer = new cell_buffer(_one_buff_len);
                        _buff_cnt += 1;
                        new_buffer->_prev = _curr_buf;
                        _curr_buf->_next = new_buffer;
                        new_buffer->_next = next_buf;
                        next_buf->_prev = new_buffer;
                        _curr_buf = new_buffer;
                    }
                }
                else
                {
                    //next buffer is free, we can use it
                    _curr_buf = next_buf;
                }
                if (!_lst_lts)
                    _curr_buf->append(log_line, len);
            }
            else//_curr_buf->status == cell_buffer::FULL, assert persist is on here too!
            {
                _lst_lts = curr_sec;
            }
        }
    }
    if (tell_back)
    {
        _cond.notify_all();
    }
}

bool cloog::decis_file(int year, int mon, int day)
{
    if (!_env_ok)
    {
        if (_fp)
            fclose(_fp);
        _fp = fopen("/dev/null", "w");
        return _fp != nullptr;
    }
    if (!_fp)
    {
        _year = year, _mon = mon, _day = day;
        char log_path[1024] = {};
        sprintf(log_path, "%s/%s.%d%02d%02d.%u.log", _log_dir, _prog_name, _year, _mon, _day, _pid);
        _fp = fopen(log_path, "w");
        if (_fp)
            _log_cnt += 1;
    }
    else if (_day != day)
    {
        fclose(_fp);
        char log_path[1024] = {};
        _year = year, _mon = mon, _day = day;
        sprintf(log_path, "%s/%s.%d%02d%02d.%u.log", _log_dir, _prog_name, _year, _mon, _day, _pid);
        _fp = fopen(log_path, "w");
        if (_fp)
            _log_cnt = 1;
    }
    else if (ftell(_fp) >= LOG_USE_LIMIT)
    {
        fclose(_fp);
        char old_path[1024] = {};
        char new_path[1024] = {};
        //mv xxx.log.[i] xxx.log.[i + 1]
        for (int i = _log_cnt - 1;i > 0; --i)
        {
            sprintf(old_path, "%s/%s.%d%02d%02d.%u.log.%d", _log_dir, _prog_name, _year, _mon, _day, _pid, i);
            sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.%d", _log_dir, _prog_name, _year, _mon, _day, _pid, i + 1);
            rename(old_path, new_path);
        }
        //mv xxx.log xxx.log.1
        sprintf(old_path, "%s/%s.%d%02d%02d.%u.log", _log_dir, _prog_name, _year, _mon, _day, _pid);
        sprintf(new_path, "%s/%s.%d%02d%02d.%u.log.1", _log_dir, _prog_name, _year, _mon, _day, _pid);
        rename(old_path, new_path);
        _fp = fopen(old_path, "w");
        if (_fp)
            _log_cnt += 1;
    }
    return _fp != nullptr;
}

void cloog::be_thdo()
{
    cloog::ins()->persist();
}

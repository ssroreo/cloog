#include "cloog.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

void thread_wirte(int id)
{
    for (int i = 0; i < 10; i++)
    {
        LOG_INFO("thread%d write:%d",id,i);
        std::this_thread::sleep_for(2000ms);
    }
}

int main()
{
    //set log path, log file name and log level
    LOG_INIT("log_path", "log_filename", TRACE);
    LOG_FATAL("Fatal log");
    LOG_ERROR("error log");
    LOG_WARN("warn log");
    LOG_DEBUG("debug log");
    LOG_TRACE("trace log");
    std::thread t1(thread_wirte,1);
    std::thread t2(thread_wirte,2);
    t1.join();
    t2.join();
    //LOG_EXIT() is optional (only mandatory if using Windows).
    LOG_EXIT();
    return 0;
}
cloog
===  
Fast, thread safe C++ logging library.   

Building From Source
=== 
Build static library:  
`mkdir build && cd build && cmake .. && make`  
  
or build dynamic library:  
`mkdir build && cd build && cmake -DCLOOG_BUILD_SHARED=ON .. && make`  

  
Usage samples
=== 
```cpp
#include "cloog.h"
int main()
{
    //set log path, log file name and log level
    LOG_INIT(“/Users/oreo/log_path“, “log_filename”, TRACE);
    LOG_FATAL("Fatal log");
    LOG_ERROR("error log");
    LOG_WARN("warn log");
    LOG_DEBUG("debug log");
    LOG_TRACE("trace log");
    //LOG_EXIT() is optional (only mandatory if using Windows).
    LOG_EXIT();
    return 0;
}
```



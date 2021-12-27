cloog
===  
Fast, thread safe C++ logging library.   

Building From Source
=== 
`g++ -o libcloog.so cloog.cpp --shared -std=c++11`  
  
Usage samples
=== 
```cpp
#include "cloog.h"
int main()
{
    LOG_INIT("path", "filename", INFO);
    for (size_t i = 0; i < 10; i++)
    {
        LOG_FATAL("log_test_%d",i);
    }
    return 0;
}
```



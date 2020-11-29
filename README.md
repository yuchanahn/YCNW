## How To Make C++ Server

useing header only lib 'YCNW'

```cpp
#include "yc_net.hpp"

int main()
{
    server_setting = server_setting_t{
        .io_thread_number = 2,
        .worker_thread_number = 2,
        .port = 1234
    };	

    main_server();
}
```

/std:c++latest
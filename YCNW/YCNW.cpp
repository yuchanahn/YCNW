#include <iostream>
#include "yc_net.hpp"
#include "yc_strand.hpp"


int main()
{
    server_setting = server_setting_t{
        .io_thread_number = 2,
        .worker_thread_number = 2,
        .port = 2738
    };

    bool stop = false;

    int a = 0;
    auto w = yc_net::create_worker();
    strand_run(stop);
    yc_net::add_sync_worker(w, [&] { for (int i = 0; i < 100000; i++) a++; });
    yc_net::add_sync_worker(w, [&] { for (int i = 0; i < 100000; i++) a++; });
    yc_net::add_sync_worker(w, [&] { printf("%d\n", a); });

    main_server();
}
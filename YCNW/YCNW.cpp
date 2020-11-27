#include <iostream>
#include "yc_net.hpp"
#include "yc_strand.hpp"
#include "packet_data.hpp"


int main()
{
    packet_data_load();

    server_setting = server_setting_t{
        .io_thread_number = 2,
        .worker_thread_number = 2,
        .port = 2738
    };

    bool stop = false;

    int a = 0;
    auto w = yc_net::create_worker();
    strand_run(stop);

    yc_net::bind_ev<p_test_packet_t>([](p_test_packet_t* packet, auto id) {
        printf("SOCKET : %d - %d\n", id, packet->number);
    });


    main_server();
}
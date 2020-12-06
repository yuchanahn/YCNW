#include <iostream>
#include "yc_net.hpp"
#include "yc_worker.hpp"
#include "packet_data.hpp"
#include "yc_time.hpp"

auto w = yc_net::create_worker();


void loop()
{
    yc::time::update_delta_time();

    static size_t fps = 0;
    static float dt = 0;
    fps++;
    if ((dt += yc::time::get_delta_time()) > 1.f)
    {
        printf("%lld\n", fps);
        fps = 0;
        dt = 0;
    }

    yc_net::add_sync_worker(w, loop);
}

int main()
{
    packet_data_load();

    server_setting = server_setting_t{
        .io_thread_number = 2,
        .worker_thread_number = 2,
        .port = 2738
    };

    yc_net::connect_callback = [](yc::socket_t socket) {
        printf("connect! [SOCKET : %lld, ADDR : %s]\n", socket, yc_net::get_clnt_addrs(socket).c_str());
    };

    yc_net::disconnect_callback = [](yc::socket_t socket) {
        printf("disconnect! [SOCKET : %lld, ADDR : %s]\n", socket, yc_net::get_clnt_addrs(socket).c_str());
    };


    bool stop = false;
    int a = 0;
    strand_run(stop);

    yc_net::bind_ev<p_test_packet_t>([](p_test_packet_t* packet, yc::socket_t socket) {
        printf("SOCKET : %lld - %d\n", socket, packet->number);

        auto p = p_packet01_t{
            999
        };
        auto p2 = p_packet02_t{
            555
        };

        yc_net::send(&p, socket);
        yc_net::send(&p2, socket);
    });

    yc_net::add_sync_worker(w, loop);

    main_server();
}
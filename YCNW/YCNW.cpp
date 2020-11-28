#include <iostream>
#include "yc_net.hpp"
#include "yc_strand.hpp"
#include "packet_data.hpp"


auto w = yc_net::create_worker();
int cnt = 0;
void loop()
{

    if (cnt < 10) {
        for (auto& i : clnts | std::views::filter(is_act_true))
        {
            p_test_packet_t p;
            p.number = 9999;

            yc_net::send(&p, i.socket);
            cnt++;
        }
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

    bool stop = false;
    int a = 0;
    strand_run(stop);

    yc_net::bind_ev<p_test_packet_t>([](p_test_packet_t* packet, yc::socket_t socket) {
        printf("SOCKET : %lld - %d\n", socket, packet->number);
    });

    yc_net::add_sync_worker(w, loop);

    main_server();
}
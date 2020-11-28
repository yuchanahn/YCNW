#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "packet_data.hpp"
#include "yc_client.hpp"
#include <WinSock2.h>

#include <thread>

#define BUFSIZE 1024

template <typename T>
void send(T& t, yc::byte_t buf[BUFSIZE], yc::socket_t socket)
{
    auto len = ((packet_t<T>*) & t)->pack((unsigned char*)buf);
    ::send(socket, (const char*)buf, len, 0);
}

int main()
{
	packet_data_load();

	yc_client master;
	master.connect("127.0.0.1", 2738);

	yc_net::bind_ev<p_test_packet_t>([](p_test_packet_t* test_packet, auto) {
		printf("test_packet : %d\n", test_packet->number);
	});

	std::thread th([&] {
		while (1)
		{
			int rt = master.read_packet();
			if (rt == -1)
			{
				printf("error\n");
			}
			else if (rt == 0)
			{
				printf("disconnect!\n");
			}
		}
		});

	yc::byte_t buf[1024];
	while (1)
	{
		p_test_packet_t t;
		std::cin >> t.number;
		send(t,buf, master.get_socket());
	}

	th.join();
	return 0;
}
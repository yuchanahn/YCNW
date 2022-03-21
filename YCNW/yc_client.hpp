#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")

#include "yc_packet.hpp"
#include <winsock2.h>

class yc_client
{
	SOCKET socket;
	SOCKADDR_IN serveraddr;
	WSADATA wsa;
	char buf[1024];
	char* mIp_address = nullptr;
	yc_read_manager* r;
public:

	const SOCKET& get_socket()
	{
		return socket;
	}
	bool connect(const char* ip_address, u_short port);
	int read_packet();
	yc_client();
	~yc_client();
};

yc_client::yc_client()
{
	r = new yc_read_manager();
}

yc_client::~yc_client()
{
	closesocket(socket);
	WSACleanup();
}

bool yc_client::connect(const char* ip_address, u_short port)
{
	if (mIp_address != ip_address)
	{
		mIp_address = (char*)ip_address;

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
		socket = ::socket(AF_INET, SOCK_STREAM, 0);

		if (socket == INVALID_SOCKET) return false;

		serveraddr.sin_family = AF_INET;
		serveraddr.sin_addr.s_addr = inet_addr(ip_address);
		serveraddr.sin_port = htons(port);
	}

	if (::connect(socket, (SOCKADDR*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR)
	{
		return false;
	}
	return true;
}

int yc_client::read_packet()
{
	auto received = recv(socket, buf, 1024, 0);
	if (received == SOCKET_ERROR)
		return SOCKET_ERROR;
	else if (received == 0)
		return 0;

	std::vector<char> b;
	for (int i = 0; i < received; i++)
		b.push_back(buf[i]);

	r->read(b, received);

	return 1;
}

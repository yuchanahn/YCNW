#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS 

#include <optional>
#include <functional>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>
#include <vector>
#include <algorithm>
#include <ranges>

#include "curried.hpp"
#include "yc_function.hpp"
#include "yc_strand.hpp"


#define BUFSIZE 1024

class ClientHandle
{
public:
    SOCKET			mSock;
    SOCKADDR_IN		_addr;
};

struct io_data_t
{
    OVERLAPPED overlapped;
    WSABUF wsaBuf;
    char buffer[BUFSIZE];

    enum eio_type
    {
        i,
        o
    };
    eio_type io_type;
    unsigned int code;
};

unsigned int __stdcall CompletionThread(LPVOID pComPort);

#pragma comment(lib, "ws2_32.lib")

template <typename E, typename F>
auto is_success_or(E e, F f) {
    if (e) f(e.value());
};

auto error_f = [](std::string s) { 
    printf("%s", s.c_str());
    exit(1); 
};

struct client_t
{
    unsigned int code;
    SOCKET socket;
    bool is_active;
};




struct server_setting_t
{
    // 0 is std::thread::hardware_concurrency();
    int io_thread_number;
    // 0 is std::thread::hardware_concurrency();
    int strand_thread_number;
    int port;
};



std::vector<client_t> clnts;

server_setting_t server_setting;

static auto io_init = [](io_data_t* io_data, io_data_t::eio_type t, int len) {
    memset(&io_data->overlapped, 0, sizeof(OVERLAPPED));
    io_data->wsaBuf.buf = io_data->buffer;
    io_data->wsaBuf.len = len;
    io_data->io_type = t;

    return io_data;
};

static auto in_io_init = std::bind(io_init, std::placeholders::_1, io_data_t::eio_type::i, BUFSIZE);

auto strand_run = [](bool& stop_button, yc_net::strand_pool& strand_pool) {
    std::vector<std::thread> ths;

    int tn = server_setting.strand_thread_number ? server_setting.strand_thread_number
                                                 : std::thread::hardware_concurrency();

    for (int i = 0; i < server_setting.strand_thread_number; ++i)
    {
        ths.push_back(std::thread([&stop_button, &strand_pool] { strand_pool.run_in_this_thread(stop_button); }));
    }
};


int main_server()
{
    WSADATA wsaData;
   
    is_success_or(
        WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ? std::make_optional("WSAStartup() error\n") : std::nullopt,
        error_f
    );

    int tn = server_setting.io_thread_number ? server_setting.io_thread_number
                                             : std::thread::hardware_concurrency();

    HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    for (int i = 0; i < tn; ++i)
        _beginthreadex(NULL, 0, CompletionThread, (LPVOID)hCompletionPort, 0, NULL);

    SOCKET hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

    SOCKADDR_IN servAddr;
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(2738);

    bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
    listen(hServSock, 5);

    io_data_t* io_data;
    ClientHandle* socket_data;

    int RecvBytes;
    int Flags;

    while (1)
    {
        SOCKADDR_IN clntAddr;
        int addrLen = sizeof(clntAddr);

        clnts.push_back(
            client_t{ 
                static_cast<unsigned int>(clnts.size()),
                accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen), 
                true
            });

        socket_data = new ClientHandle;
        socket_data->mSock = clnts.back().socket;
        memcpy(&(socket_data->_addr), &clntAddr, addrLen);

        auto hIOCP = CreateIoCompletionPort(
            (HANDLE)socket_data->mSock,
            hCompletionPort,
            (ULONG_PTR)(socket_data),
            0);
        if (NULL == hIOCP || hCompletionPort != hIOCP)
        {
            printf("[error] CreateIoCompletionPort(): %d", GetLastError());
            return false;
        }

        io_data = in_io_init(new io_data_t);

        Flags = 0;
        printf("%s|%lld\n", inet_ntoa(socket_data->_addr.sin_addr), socket_data->mSock);
        WSARecv(
            socket_data->mSock, 
            &(io_data->wsaBuf), 
            1, 
            (LPDWORD)&RecvBytes, 
            (LPDWORD)&Flags, 
            &(io_data->overlapped), 
            NULL);
    }

    return 0;
}


unsigned int __stdcall CompletionThread(LPVOID pComPort)
{
    HANDLE cp = (HANDLE)pComPort;
    io_data_t* io_data = nullptr;
    DWORD flags;

    ClientHandle* clnt_info = NULL;
    BOOL bSuccess = TRUE;
    DWORD len;

    while (1) {
        bSuccess = GetQueuedCompletionStatus(
            cp,
            &len,                       
            (PULONG_PTR)&clnt_info,     
            (LPOVERLAPPED*)&io_data,
            INFINITE);                  

        if (bSuccess) {
            if (len == 0)
            {
                clnts[io_data->code].is_active = false;
                closesocket(clnt_info->mSock);
                delete io_data;
                continue;
            }

            if (io_data->io_type == io_data->i)
            {
                io_data->wsaBuf.buf[len] = '\0';
                printf("%s : %s\n", inet_ntoa(clnt_info->_addr.sin_addr), io_data->wsaBuf.buf);

                io_data->wsaBuf.len = len;


                for (auto& i : clnts | std::views::filter(is_act_true))
                {
                    auto wio_data = io_init(new io_data_t, io_data_t::eio_type::o, len);
                    std::copy(io_data->buffer, io_data->buffer + io_data->wsaBuf.len, wio_data->buffer);

                    WSASend(i.socket, &(wio_data->wsaBuf), 1, NULL, 0, &(wio_data->overlapped), NULL);
                }
                in_io_init(io_data);
                flags = 0;
                WSARecv(
                    clnt_info->mSock,
                    &(io_data->wsaBuf),
                    1,
                    NULL,
                    &flags,
                    &(io_data->overlapped),
                    NULL);
            }
            if (io_data->io_type == io_data->o)
            {
                delete io_data;
            }
        }
        else {

        }
    }

    return 0;
}
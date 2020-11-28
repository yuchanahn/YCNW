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

#include "yc_curried.hpp"
#include "yc_function.hpp"
#include "yc_strand.hpp"
#include "yc_packet.hpp"



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
    io_data_t* in_io_data;
    io_data_t* out_io_data;
    DWORD flag;

    yc_read_manager packet_reader;
    char send_buf[1024];
};

struct server_setting_t
{
    // 0 = hardware concurrency;
    int io_thread_number;
    // 0 = hardware concurrency;
    int worker_thread_number;
    int port;
};



std::vector<client_t> clnts;
std::unordered_map<size_t, yc_net::worker_info_t*> clnts_io_worker;
std::unordered_map<yc::socket_t, size_t> clnts_socket_to_code;

server_setting_t server_setting;

static auto io_init = [](io_data_t* io_data, io_data_t::eio_type t, int len) {
    memset(&io_data->overlapped, 0, sizeof(OVERLAPPED));
    io_data->wsaBuf.buf = io_data->buffer;
    io_data->wsaBuf.len = len;
    io_data->io_type = t;

    return io_data;
};

static auto in_io_init = std::bind(io_init, std::placeholders::_1, io_data_t::eio_type::i, BUFSIZE);

auto strand_run = [](bool& stop_button) {
    static std::vector<std::thread> ths;

    int th_cnt = server_setting.worker_thread_number ? server_setting.worker_thread_number
                                                      : std::thread::hardware_concurrency();
    for (int i = 0; i < th_cnt; ++i){
        ths.push_back(std::thread([&stop_button] { yc_net::run_wokers_in_this_thread(stop_button); }));
    }
};


namespace yc_net {
    template <typename T>
    void send(T* packet, yc::socket_t socket) {
        yc::byte_t buf_[1024];
        auto len = ((packet_t<T>*) packet)->pack((yc::byte_t*)buf_);

        add_sync_worker(clnts_io_worker[clnts_socket_to_code[socket]], [socket, buf = std::move(buf_), len] {
            auto io_data = clnts[clnts_socket_to_code[socket]].out_io_data;
            std::copy(buf, buf + len, io_data->buffer);
            io_init(io_data, io_data_t::o, len);
            
            WSASend(socket, &(io_data->wsaBuf), 1, NULL, 0, &(io_data->overlapped), NULL);
        });
    };
}

int main_server()
{
    WSADATA wsaData;
   
    is_success_or(
        WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 ? std::make_optional("WSAStartup() error\n") : std::nullopt,
        error_f
    );

    HANDLE hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    int th_cnt = server_setting.io_thread_number ? server_setting.io_thread_number
                                                  : std::thread::hardware_concurrency();
    for (int i = 0; i < th_cnt; ++i)
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
        socket_data = new ClientHandle;

        size_t idx = 0;

        if (std::find_if(clnts.begin(), 
                         clnts.end(), 
                         [&](auto& i) { idx = i.code; return !(i.is_active); }) != clnts.end())
        {
            clnts[idx].socket = accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen);
            clnts[idx].is_active = true;
        }
        else {
            clnts.push_back(
                client_t{
                    static_cast<unsigned int>(clnts.size()),
                    accept(hServSock, (SOCKADDR*)&clntAddr, &addrLen),
                    true,
                    new io_data_t,
                    new io_data_t,
                    0
                });
            idx = clnts.size() - 1;
        }
        auto client_socket = clnts[idx].socket;
        clnts_socket_to_code[client_socket] = clnts[idx].code;
        clnts_io_worker[clnts[idx].code] = yc_net::create_worker();
        socket_data->mSock = client_socket;
        
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

        io_data = in_io_init(clnts[idx].in_io_data);

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

auto client_disconnect = [](io_data_t* io_data, ClientHandle* clnt_info) {
    printf("client[socket : %lld, addr : %s] 접속 종료!\n", clnt_info->mSock, inet_ntoa(clnt_info->_addr.sin_addr));
    clnts[io_data->code].is_active = false;
    closesocket(clnt_info->mSock);

    clnts_io_worker[io_data->code]->is_active = false;
    clnts_socket_to_code.erase(clnt_info->mSock);
};


unsigned int __stdcall CompletionThread(LPVOID pComPort)
{
    HANDLE cp = (HANDLE)pComPort;
    io_data_t* io_data = nullptr;

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
                client_disconnect(io_data, clnt_info);
                continue;
            }

            if (io_data->io_type == io_data->i)
            {
                auto sock = clnt_info->mSock;
                auto in_io_data = clnts[io_data->code].in_io_data;
                auto c_code = io_data->code;

                yc_net::add_sync_worker(clnts_io_worker[io_data->code], [sock, in_io_data, c_code, len] {
                    if (!(clnts[c_code].is_active)) {
                        return;
                    }

                    clnts[c_code].packet_reader.read((unsigned char*)in_io_data->wsaBuf.buf, len, sock);

                    in_io_init(in_io_data);
                    clnts[c_code].flag = 0;
                    WSARecv(
                        sock,
                        &(in_io_data->wsaBuf),
                        1,
                        NULL,
                        &clnts[c_code].flag,
                        &(in_io_data->overlapped),
                        NULL);
                });
                
            }
            if (io_data->io_type == io_data->o)
            {

            }
        }
        else {
            client_disconnect(io_data, clnt_info);
            continue;
        }
    }

    return 0;
}
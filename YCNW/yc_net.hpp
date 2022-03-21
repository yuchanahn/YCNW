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

#include "yc_function.hpp"
#include "yc_worker.hpp"
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
    bool is_active = false;
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
    DWORD flag;

    yc_read_manager packet_reader;
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
std::unordered_map<yc::socket_t, SOCKADDR_IN> clnts_addrs;

server_setting_t server_setting;


// 쓰레드에서 스레드 넘버로 풀링 할 수 있게끔 해놓자.
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
    for (int i = 0; i < th_cnt; ++i) {
        ths.push_back(std::thread([&stop_button] { yc_net::run_wokers_in_this_thread(stop_button); }));
    }
};

namespace yc_io_sp
{
    std::unordered_map<std::thread::id, std::vector<io_data_t*>> io_datas;

    io_data_t* create_io() {
        std::vector<io_data_t*>& datas = io_datas[std::this_thread::get_id()];
        io_data_t* d = nullptr;

        for (auto& i : datas | std::views::filter(pis_act_false))
        {
            d = i;
        }
        if (!d)
        {
            datas.push_back(new io_data_t());
            d = datas.back();
        }
        d->is_active = true;
        return d;
    };
}

namespace yc_net {
    std::function<void(yc::socket_t socket)> disconnect_callback;
    std::function<void(yc::socket_t socket)> connect_callback;

    auto get_clnt_addrs = [](yc::socket_t socket) {
        return std::string(inet_ntoa(clnts_addrs[socket].sin_addr));
    };

    template <typename T>
    void send(T* packet, yc::socket_t socket) {
        yc::byte_t buf_[1024];
        auto len = ((packet_t<T>*) packet)->pack((yc::byte_t*)buf_);

        auto io_data = yc_io_sp::create_io();
        std::copy(buf_, buf_ + len, io_data->buffer);

        add_sync_worker(clnts_io_worker[clnts_socket_to_code[socket]], [socket, io_data, len] {
            auto code = clnts_socket_to_code[socket];
            io_init(io_data, io_data_t::o, len);
            io_data->code = code;
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
    servAddr.sin_port = htons(server_setting.port);

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
        io_data = in_io_init(yc_io_sp::create_io());
        io_data->code = clnts[idx].code;
        Flags = 0;

        clnts_addrs[socket_data->mSock] = socket_data->_addr;

        //printf("%s|%lld\n", inet_ntoa(socket_data->_addr.sin_addr), socket_data->mSock);
        WSARecv(
            socket_data->mSock,
            &(io_data->wsaBuf),
            1,
            (LPDWORD)&RecvBytes,
            (LPDWORD)&Flags,
            &(io_data->overlapped),
            NULL);
        yc_net::connect_callback(socket_data->mSock);
    }

    return 0;
}

auto client_disconnect = [](io_data_t* io_data, ClientHandle* clnt_info) {
    clnts[io_data->code].is_active = false;
    closesocket(clnt_info->mSock);

    clnts_io_worker[io_data->code]->is_active = false;
    clnts_socket_to_code.erase(clnt_info->mSock);

    yc_net::disconnect_callback(clnt_info->mSock);
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
                auto in_io_data = yc_io_sp::create_io();
                auto c_code = io_data->code;
                auto ch = *clnt_info;

                std::copy(io_data, (io_data_t*)(((char*)io_data) + sizeof(io_data_t)), in_io_data);

                io_data->is_active = false;
                yc_net::add_sync_worker(clnts_io_worker[io_data->code], [sock, in_io_data, c_code, len, ch] {
                    if (!(clnts[c_code].is_active)) {
                        return;
                    }
                    try
                    {
                        clnts[c_code].packet_reader.read((unsigned char*)in_io_data->buffer, len, sock);
                    }
                    catch (const std::exception&)
                    {
                        client_disconnect(in_io_data, (ClientHandle*)&ch);
                    }

                    in_io_init(in_io_data);
                    in_io_data->code = clnts[c_code].code;

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
                io_data->is_active = false;
            }
        }
        else {
            client_disconnect(io_data, clnt_info);
            continue;
        }
    }

    return 0;
}
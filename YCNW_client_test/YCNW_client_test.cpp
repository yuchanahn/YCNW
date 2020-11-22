#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <winsock2.h>
#include <thread>

void ErrorHandling(const char* message);

#pragma comment(lib, "ws2_32.lib")



auto packet_reader = [](int s) {
    char recv_msg[1024] = { 0 };
    while (1) {
        auto received = recv(s, recv_msg, 1024, 0);

        if (received == SOCKET_ERROR)   return SOCKET_ERROR;
        else if (received == 0)         return 0;

        recv_msg[min(received, 1023)] = 0;
        printf("Recv[%s]\n", recv_msg);
    }
};

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        ErrorHandling("WSAStartup() error!");

    SOCKET hSocket = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (hSocket == INVALID_SOCKET)
        ErrorHandling("socket() error");

    SOCKADDR_IN recvAddr;
    memset(&recvAddr, 0, sizeof(recvAddr));
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    recvAddr.sin_port = htons(2738);

    if (connect(hSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR)
        ErrorHandling("connect() error!");

    char message[1024] = { 0 };
    
    int sendBytes = 0;
    int recvBytes = 0;
    int flags = 0;


    std::thread th([&] { packet_reader(hSocket); });

    while (true)
    {
        flags = 0;
        printf("전송할데이터(종료를원할시exit)\n");

        std::cin >> message;

        if (!strcmp(message, "exit")) break;

        auto len = strlen(message);

        send(hSocket, message, len, 0);
    }

    closesocket(hSocket);

    WSACleanup();

    th.join();
    return 0;
}

void ErrorHandling(const char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);

    exit(1);
}

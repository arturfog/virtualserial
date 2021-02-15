#include "TCPClient.h"
#include <winsock2.h>
#include <iostream>
#include "COMPortManager.h"

int TCPClient::Connect()
{
    const char* DEFAULT_PORT = "3000";
    const char* SERVER_IP = "10.0.2.2";

    WSADATA wsaData;
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    printf("Connecting to server ....\n");
    // Resolve the server address and port
    iResult = getaddrinfo(SERVER_IP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ClientConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ClientConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ClientConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ClientConnectSocket);
            ClientConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ClientConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    printf("[OK]\n");

    ghWriteMutex = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

    ghReadMutex = CreateMutex(
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

    if (ghWriteMutex == NULL || ghReadMutex == NULL)
    {
        CloseHandle(ghWriteMutex);
        CloseHandle(ghReadMutex);
        printf("CreateMutex error: %d\n", GetLastError());
        return 1;
    }

    return 0;
}

int TCPClient::Write(const char* buffer, const int bufferLen)
{
    int iResult;
    char msg[2*COMPortManager::PACKET_SIZE];

    if (ClientConnectSocket == INVALID_SOCKET) {
        printf("Invalid client socket!\n");
        WSACleanup();
        return 1;
    }

    WaitForSingleObject(
        ghWriteMutex,    // handle to mutex
        INFINITE);  // no time-out interval

    if (bufferLen > 0) {
        sprintf_s(msg, "DIRECTION:%d\nLEN:%d\nMSG:", (int)COMPortManager::DIRECTION::TX, bufferLen);
        strncat_s(msg, buffer, COMPortManager::PACKET_SIZE-1);
        // Send an initial buffer
        iResult = send(ClientConnectSocket, msg, COMPortManager::PACKET_SIZE, 0);
        if (iResult == SOCKET_ERROR) {
            printf("TCPClient::Write send failed with error: %d\n", WSAGetLastError());
            closesocket(ClientConnectSocket);
            WSACleanup();
            ReleaseMutex(ghWriteMutex);
            return 1;
        }
        printf("TCPClient::Write(const char* buffer, int bufferLen) .... [%c] [%d]\n", buffer[0], bufferLen);
    }

    ReleaseMutex(ghWriteMutex);
    return 0;
}

int TCPClient::Read(char* lpBuffer, int bufferLen)
{
    int iResult;

    if (ClientConnectSocket == INVALID_SOCKET) {
        printf("Invalid client socket!\n");
        WSACleanup();
        return 1;
    }

    WaitForSingleObject(
        ghWriteMutex,    // handle to mutex
        INFINITE);  // no time-out interval

    // Send an initial buffer
    char msg[2 * COMPortManager::PACKET_SIZE];
    sprintf_s(msg, "DIRECTION:%d\nLEN:%d\nMSG:", (int)COMPortManager::DIRECTION::RX, bufferLen);
    iResult = send(ClientConnectSocket, msg, 2 * COMPortManager::PACKET_SIZE, 0);
    if (iResult == SOCKET_ERROR) {
        printf("TCPClient::Read send failed with error: %d\n", WSAGetLastError());
        closesocket(ClientConnectSocket);
        WSACleanup();
        ReleaseMutex(ghWriteMutex);
        return 1;
    }
    printf("TCPClient::Read(char* lpBuffer, int bufferLen) ....\n");

    // Receive until the peer closes the connection
    {
        SecureZeroMemory(lpBuffer, COMPortManager::PACKET_SIZE);
        iResult = recv(ClientConnectSocket, lpBuffer, bufferLen, 0);
        if (iResult > 0) 
        {
            printf("TCPClient::Read Bytes received: [%c]\n", lpBuffer[0]);
        }
        else if (iResult == 0)
        {
            iResult = 0;
            printf("Connection closed\n");
        }
        else
        {
            iResult = 0;
            printf("recv failed with error: %d\n", WSAGetLastError());
        }

    }

    ReleaseMutex(ghWriteMutex);
    return iResult;
}

int TCPClient::Close() {
    int iResult;
    CloseHandle(ghWriteMutex);
    CloseHandle(ghReadMutex);
    printf("Closing client socket .... ");
    // shutdown the connection since no more data will be sent
    iResult = shutdown(ClientConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientConnectSocket);
        WSACleanup();
        printf("[ERROR]\n");
        return 1;
    }
    // cleanup
    closesocket(ClientConnectSocket);
    WSACleanup();

    printf("[OK]\n");
    return 0;
}
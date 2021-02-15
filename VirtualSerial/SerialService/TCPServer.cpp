#include "TCPServer.h"
#include <winsock2.h>
#include <iostream>

bool TCPServer::RUN_SERVER = TRUE;
COMPortManager* TCPServer::cpm = NULL;
SOCKET TCPServer::ListenSocket = INVALID_SOCKET;
TCPServer::TCPServer(COMPortManager* c) : dwThreadIdArray{ 0 }, hThreadArray{ NULL }, DEFAULT_PORT{"3000"}
{
    cpm = c;
}

void TCPServer::Close() 
{
    printf("Closing server socket .... ");
    // Close all thread handles and free memory allocations.
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (hThreadArray[i] != NULL) {
            CloseHandle(hThreadArray[i]);
        }
    }

    RUN_SERVER = FALSE;
    // No longer need server socket
    closesocket(ListenSocket);
    printf("[OK]\n");
}

int TCPServer::Start()
{
    WSADATA wsaData;
    int iResult;

    struct addrinfo* result = NULL;
    struct addrinfo hints;

    printf("Starting TCP server .... ");
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        printf("[ERROR]\n");
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        printf("[ERROR]\n");
        WSACleanup();
        return 1;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        printf("[ERROR]\n");
        return 1;
    }

    // Setup the TCP listening socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        printf("[ERROR]\n");
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        printf("[ERROR]\n");
        return 1;
    }

    printf("[OK]\n");
    // Create the thread to begin execution on its own.
    hThreadArray[0] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        HandleClient,           // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadIdArray[0]);   // returns the thread identifier 
    
    return 0;
}

void TCPServer::Wait()
{
    printf("Waiting for TCP server threads to close ....");
    WaitForMultipleObjects(1, hThreadArray, TRUE, 10000);
    printf("[OK]\n");
}

COMPortManager::SerialMessage TCPServer::ParseMSG(const char* recvbuf, int buffLen)
{
    COMPortManager::SerialMessage sm;
    SecureZeroMemory(sm.msg, COMPortManager::PACKET_SIZE);

    int dir = 0;
    const int ret = sscanf_s(recvbuf, "DIRECTION:%d\nLEN:%d\nMSG:", &dir, &sm.len);
    if (ret > 1) {
        if (dir == (int)COMPortManager::DIRECTION::TX)
        {
            sm.d = COMPortManager::DIRECTION::TX;
            const char* idx = strstr(recvbuf, "MSG:");
            if (idx != NULL) {
                printf("memcpy ...");
                memcpy(sm.msg, idx+4, sm.len);
                printf("[OK]\n");
            }
        }
        else
        {
            sm.d = COMPortManager::DIRECTION::RX;
        }
        //printf("Parsing result .... dir: %d len: %d\n", (int)sm.d, sm.len);
    }
    else {
        sm.len = 0;
        sm.d = COMPortManager::DIRECTION::NONE;
    }
    return sm;
}

DWORD WINAPI TCPServer::HandleClient(LPVOID lpParam)
{
    int iResult;
    int iSendResult;
    const int recvbuflen = 2*COMPortManager::PACKET_SIZE;
    char recvbuf[2*COMPortManager::PACKET_SIZE];
    SOCKET ClientSocket = INVALID_SOCKET;

    while (RUN_SERVER) {
        // Accept a client socket
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) 
        {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }

        // Receive until the peer shuts down the connection
        do 
        {
            SecureZeroMemory(recvbuf, recvbuflen);
            iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
            if (iResult > 0) 
            {
                //printf("Bytes received: %d\n", iResult);
                COMPortManager::SerialMessage sm = ParseMSG(recvbuf, recvbuflen);

                if (sm.d == COMPortManager::DIRECTION::RX) 
                {
                    printf("Handling request to READ: %d\n", sm.len);
                    // Read requested number of bytes from real COM port
                    SecureZeroMemory(sm.msg, COMPortManager::PACKET_SIZE);
                    cpm->ReadRealCOM(sm.msg, sm.len);
                    // Send back to client
                    iSendResult = send(ClientSocket, sm.msg, sm.len, 0);
                    if (iSendResult == SOCKET_ERROR) 
                    {
                        printf("send failed with error: %d\n", WSAGetLastError());
                        closesocket(ClientSocket);
                        WSACleanup();
                        return 1;
                    }
                    printf("TCPServer::HandleClient(LPVOID lpParam) Bytes sent: %d\n", iSendResult);
                }
                else if (sm.d == COMPortManager::DIRECTION::TX) 
                {
                    printf("Handling request to WRITE: %d [%c]\n", sm.len, sm.msg[0]);
                    if (sm.len > 0) {
                        cpm->WriteRealCOM(sm.msg, sm.len);
                    }
                }

            }
            else if (iResult == 0) 
            {
                printf("Connection closing...\n");
            }
            else 
            {
                printf("recv failed with error: %d\n", WSAGetLastError());
                closesocket(ClientSocket);
                WSACleanup();
                return 1;
            }

        } while (iResult > 0);
    }

    // No longer need server socket
    closesocket(ListenSocket);
    printf("TCPServer::HandleClient closesocket ...\n");
    // shutdown the connection since we're done
    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return 1;
    }
    // cleanup
    closesocket(ClientSocket);
    WSACleanup();

    return 0;
}

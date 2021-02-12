#pragma once
#include <ws2tcpip.h>
#include "COMPortManager.h"
class TCPServer
{
public:
	TCPServer(COMPortManager* c);
	void Close();
	int Start();
	void Wait();
	static COMPortManager::SerialMessage ParseMSG(const char* recvbuf, int buffLen);
	static DWORD WINAPI HandleClient(LPVOID lpParam);
private:
	static const int MAX_CLIENTS = 3;
	static SOCKET ListenSocket;
	static COMPortManager* cpm;
	static bool RUN_SERVER;
	DWORD   dwThreadIdArray[MAX_CLIENTS];
	HANDLE  hThreadArray[MAX_CLIENTS];
	char DEFAULT_PORT[6];
};
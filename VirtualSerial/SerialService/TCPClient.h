#pragma once
#include <ws2tcpip.h>
class TCPClient
{
public:
	int Connect();
	int Write(const char* buffer, int bufferLen);
	int Read(char* lpBuffer, int bufferLen);
	int Close();
private:
	SOCKET ClientConnectSocket = INVALID_SOCKET;
};


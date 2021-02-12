#pragma once
#include <windows.h>
class COMPortManager
{
public:
    static const int PACKET_SIZE = 1024;

    enum class DIRECTION {
        TX = 0,
        RX,
        NONE
    };

    struct SerialMessage {
        DIRECTION d = { DIRECTION::NONE };
        int len = 0;
        char msg[PACKET_SIZE] = { 0 };
    };

    void ReadConfigFile();
    bool OpenRealCOM();
    bool InitVirtualCom(const char* comPortName);
    void CloseRealCOM();
    void PrintCommState(DCB dcb);
    void SetBaudRate(int br, HANDLE hCOM);
    void SetTimeout(HANDLE hCOM);
    int ReadRealCOM(LPVOID buffer, int bufferLen);
    int WriteRealCOM(LPCVOID buffer, int bufferLen);
private:
    char COMPort[5];
    HANDLE hComm = INVALID_HANDLE_VALUE;
    COMMTIMEOUTS timeouts;
};


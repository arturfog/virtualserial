#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <Windows.h>
#include <tchar.h>
#include <signal.h>  

#include "COMPortManager.h"
#include "TCPClient.h"
#include "TCPServer.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

bool SERVER_MODE = TRUE;
bool WAIT_FOR_PIPE = TRUE;
COMPortManager cpm;
TCPClient client;
TCPServer server(&cpm);

int waitForMsg()
{
    LPCTSTR lpszPipename;
    if (SERVER_MODE == TRUE) {
        lpszPipename = TEXT("\\\\.\\pipe\\virtualserialpipe2");
    }
    else {
        lpszPipename = TEXT("\\\\.\\pipe\\virtualserialpipe3");
    }
    HANDLE hPipe = INVALID_HANDLE_VALUE;

    printf("Waiting for mesasges from virtual driver ....\n");
    while (WAIT_FOR_PIPE)
    {
        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_READ |  // read and write access 
            GENERIC_WRITE,
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file

        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs. 
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            _tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
            Sleep(500);
        }
        else 
        {
            // All pipe instances are busy, so wait for 20 seconds. 
            if (!WaitNamedPipe(lpszPipename, 20000))
            {
                printf("Could not open pipe: 20 second wait timed out.");
            }
        }
        Sleep(500);
    }

    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbWritten, dwMode;
    dwMode = PIPE_READMODE_MESSAGE;
    fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle 
        &dwMode,  // new pipe mode 
        NULL,     // don't set maximum bytes 
        NULL);    // don't set maximum time 
    if (!fSuccess)
    {
        _tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    COMPortManager::SerialMessage smRet;
    COMPortManager::SerialMessage smOut;

    int bytesRead = 0;
    do
    {
        // Read from the pipe. 
        fSuccess = ReadFile(
            hPipe,    // pipe handle 
            &smRet,    // buffer to receive reply 
            sizeof(COMPortManager::SerialMessage),  // size of buffer 
            &cbRead,  // number of bytes read 
            NULL);    // not overlapped 

        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
            break;

        if (smRet.d == COMPortManager::DIRECTION::TX) {
            if (SERVER_MODE) {
                // Forward data from virtual port to real port
                if (smRet.len > 1) {
                    printf("Writting: %d bytes \"%c\"\n", smRet.len, smRet.msg[0]);
                }
                cpm.WriteRealCOM(smRet.msg, smRet.len);
            }
            else 
            {
                // Forward data from virtual port by TCP to server
                client.Write(smRet.msg, smRet.len);
            }
        }
        else if (smRet.d == COMPortManager::DIRECTION::RX) {
            int len = (smRet.len < COMPortManager::PACKET_SIZE ? smRet.len : COMPortManager::PACKET_SIZE);
            //printf("Request Reading: %d bytes\n", len);
            if (SERVER_MODE) {
                bytesRead = cpm.ReadRealCOM(smOut.msg, len);
                smOut.len = bytesRead;
                smOut.d = COMPortManager::DIRECTION::NONE;
                if (smOut.len > 1) {
                    printf("Reading: %d bytes\n", smOut.len);
                }
                fflush(stdout);
            }
            else 
            {
                // Read data by TCP from server
                bytesRead = client.Read(smOut.msg, len);
                smOut.len = bytesRead;
                smOut.d = COMPortManager::DIRECTION::NONE;
            }
            // Write data to virtual driver, so app can read it
            fSuccess = WriteFile(
                hPipe,                  // pipe handle 
                &smOut,             // message 
                sizeof(COMPortManager::SerialMessage),  // message length 
                &cbWritten,             // bytes written 
                NULL);                  // not overlapped 

            if (!fSuccess)
            {
                _tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
                return -1;
            }
        }
    } while (1);  // repeat loop if ERROR_MORE_DATA 

    if (!fSuccess)
    {
        _tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
        return -1;
    }

    CloseHandle(hPipe);

    return 0;
}

void SignalHandler(int signal)
{
    if (signal == SIGINT) {
        WAIT_FOR_PIPE = FALSE;
        if (SERVER_MODE)
        {
            cpm.CloseRealCOM();
            server.Close();
            server.Wait();
        }
        else {
            client.Close();
        }
    }
}

int main(int argc, char* argv[])
{
    signal(SIGINT, SignalHandler);

    if (argc > 1) {
        if (strcmp(argv[1], "--client") == 0)
        {
            SERVER_MODE = FALSE;
        }
    }

    if (SERVER_MODE) {
        cpm.OpenRealCOM();
        cpm.InitVirtualCom("COM4");
        server.Start();
        waitForMsg();
    }
    else {
        if (client.Connect() == 0)
        {
            waitForMsg();
        }
    }

    if (SERVER_MODE) 
    { 
        cpm.CloseRealCOM();
        server.Close();
        server.Wait();
    }
    else { 
        client.Close();
    }
}
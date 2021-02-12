#include "COMPortManager.h"
#include <stdio.h>

void
COMPortManager::ReadConfigFile()
{
    FILE* fp = NULL;
    errno_t err;
    err = fopen_s(&fp, "C:\\Users\\Artur\\serial.cfg", "r");
    if (err == 0)
    {
        //printf("Config file open .... [ OK ]\n");
        fread(COMPort, sizeof(char), 4, fp);
        //printf("Selected port: %s\n", COMPort);
        fclose(fp);
    }
}

bool
COMPortManager::OpenRealCOM()
{
    if (hComm != INVALID_HANDLE_VALUE) {
        return TRUE;
    }

    printf("COM port open .... ");
    ReadConfigFile();
    char finalComPath[13] = "\\\\.\\";
    strcat_s(finalComPath, COMPort);


    hComm = CreateFileA(finalComPath,                //port name
        GENERIC_READ | GENERIC_WRITE, //Read/Write
        0,                            // No Sharing
        NULL,                         // No Security
        OPEN_EXISTING,// Open existing port only
        0,            // Non Overlapped I/O
        NULL);        // Null for Comm Devices

    if (hComm == INVALID_HANDLE_VALUE) {
        printf("[ ERROR ]\n");
        return FALSE;
    }

    printf("[ OK ]\n");

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    SetBaudRate(CBR_115200, hComm);
    SetTimeout(hComm);
    return TRUE;
}

bool COMPortManager::InitVirtualCom(const char* comPortName)
{
    HANDLE hCommVirtual = INVALID_HANDLE_VALUE;

    char finalComPath[13] = "\\\\.\\";
    strcat_s(finalComPath, comPortName);

    printf("Virtual COM port open .... ");
    hCommVirtual = CreateFileA(finalComPath,                //port name
        GENERIC_READ | GENERIC_WRITE, //Read/Write
        0,                            // No Sharing
        NULL,                         // No Security
        OPEN_EXISTING,// Open existing port only
        0,            // Non Overlapped I/O
        NULL);        // Null for Comm Devices

    if (hCommVirtual == INVALID_HANDLE_VALUE) {
        printf("[ ERROR ]\n");
        return FALSE;
    }

    printf("[ OK ]\n");
    PurgeComm(hCommVirtual, PURGE_RXCLEAR | PURGE_TXCLEAR);
    //SetTimeout(hCommVirtual);
    SetBaudRate(CBR_115200, hCommVirtual);

    printf("Virtual COM port close .... ");
    if (hCommVirtual != INVALID_HANDLE_VALUE) {
        CloseHandle(hCommVirtual); //Closing the Serial Port
    }

    printf("[ OK ]\n");
}

void
COMPortManager::CloseRealCOM()
{
    printf("COM port close .... ");
    if (hComm != INVALID_HANDLE_VALUE) {
        CloseHandle(hComm); //Closing the Serial Port
        hComm = INVALID_HANDLE_VALUE;
    }
    printf("[ OK ]\n");
}

void COMPortManager::PrintCommState(DCB dcb)
{
    //  Print some of the DCB structure values
    printf("\nBaudRate = %d, ByteSize = %d, Parity = %d, StopBits = %d\n",
        dcb.BaudRate,
        dcb.ByteSize,
        dcb.Parity,
        dcb.StopBits);
}

void COMPortManager::SetBaudRate(int br, HANDLE hCOM)
{
    DCB dcb;
    SecureZeroMemory(&dcb, sizeof(DCB));
    printf("GetCommState .... ");
    GetCommState(hCOM, &dcb);
    dcb.DCBlength = sizeof(DCB);
    printf("[ OK ]\n");
    PrintCommState(dcb);
    dcb.BaudRate = br;
    //dcb.ByteSize = 8;
    //dcb.StopBits = 1;
    //dcb.Parity = 0;
    //dcb.fInX = 1;
    //dcb.fOutX = 1;
    PrintCommState(dcb);
    printf("SetCommState .... ");
    SetCommState(hCOM, &dcb);
    printf("[ OK ]\n");
}

void COMPortManager::SetTimeout(HANDLE hCOM)
{
    printf("SetCommTimeouts .... ");
    timeouts.ReadIntervalTimeout = 20;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    if (!SetCommTimeouts(hCOM, &timeouts))
    {
        printf("Unable to set timeouts\n");
    }
    printf("[ OK ]\n");
}

int
COMPortManager::WriteRealCOM(LPCVOID buffer, int bufferLen)
{
    DWORD dNoOfBytesWritten = 0;

    if (OpenRealCOM() == TRUE) {
        bool Status = WriteFile(hComm,        // Handle to the Serial port
            buffer,// Data to be written to the port
            bufferLen,  //No of bytes to write
            &dNoOfBytesWritten, //Bytes written
            NULL);
        if (Status == FALSE)
        {
            return -1;
        }
        return dNoOfBytesWritten;
    }
    return 0;
}

int
COMPortManager::ReadRealCOM(LPVOID buffer, int bufferLen)
{
    DWORD dNoOfBytesRead = 0;
    bool Status = ReadFile(hComm,        // Handle to the Serial port
        buffer,// Data to be read from the port
        bufferLen,  //No of bytes to read
        &dNoOfBytesRead, //Bytes readed
        NULL);
    if (Status == FALSE)
    {
        return -1;
    }
    return dNoOfBytesRead;
}
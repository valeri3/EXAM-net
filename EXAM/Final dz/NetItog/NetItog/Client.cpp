#define WIN32_LEAN_AND_MEAN

#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 4096
#define SERVER_IP "127.0.0.1"
#define DEFAULT_PORT "8888"

SOCKET client_socket;
bool isFirstOrder = true; // ���������� ����� 1 ��� ������� ����

string toLower(const string& str) // ������� �������������� ������ � ������ �������
{
    string lowerStr = str;
    // ��������� transform ��� �������������� ������ � ������ �������.
    // ��� ������� � ����, ��� ������������� ����� for

    transform(
        lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
        [](unsigned char c) { return tolower(c); }
    );

    return lowerStr;
}

string getOrder() // ������� ��� ����� ���������� � ������
{
    string order;

    if (isFirstOrder == true)
    {
        cout << "\t\tMenu" << endl;
        cout << "hamburger - 5 ���\nsprite - 1 ���\npotato free - 3 ���\nsmoothie - 4 ���\nfalafel - 7 ���\n(������� ����� ������)\n" << endl;
        cout << "������� ��� �����: ";
        isFirstOrder = false;
    }

    getline(cin, order);
    return toLower(order); // ��������� � ������ ������� ��� ����� �������� �������� �� �������
}

DWORD WINAPI Sender(void* param) 
{
    while (true) 
    {
        string order = getOrder(); // �������� ����� �� ������������
        if (!order.empty()) 
        {
            send(client_socket, order.c_str(), order.size(), 0);
            cout << "����� ���������. ������� ��������� ����� ��� ������� Enter ��� ������." << endl;
        }
        Sleep(100); // ��������� �������� ��� �������������� ��������� ��������
    }
}

DWORD WINAPI Receiver(void* param)
{
    while (true) {
        char response[DEFAULT_BUFLEN];
        int result = recv(client_socket, response, DEFAULT_BUFLEN, 0);
        response[result] = '\0';

        // cout << "...\nYou have new response from server: " << response << "\n";
        cout << response << "\n";
        // cout << "Please insert your query for server: ";
    }
}

BOOL WINAPI ConsoleHandler(DWORD signal) 
{
    if (signal == CTRL_CLOSE_EVENT) 
    {
        const char* quitMsg = "quit";
        send(client_socket, quitMsg, strlen(quitMsg), 0);

        // �������� ��� �������� �������� ��������� ����� ��������� ������
        Sleep(1000);
        closesocket(client_socket);
        WSACleanup();
    }
    return TRUE;
}

// UDP multicast � ������� 

int main()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // �������� ���������� ������������ ������

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) // �������������� ���������� ������� �������� �������
    {
        cout << "Error: Could not set control handler" << endl;
        return 1;
    }

    setlocale(0, "ru");
    system("title Client");

    ///////////////////////////////////////////////////////////////////////////////
    
    WSADATA wsaData; // initialize Winsock
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // ��������� ����� ������� � ����
    addrinfo* result = nullptr;
    iResult = getaddrinfo(SERVER_IP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 2;
    }

    addrinfo* ptr = nullptr;

    
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) // �������� ������������ � ������, ���� �� �������
    {
        client_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol); // ������� ����� �� ������� ������� ��� ����������� � �������

        if (client_socket == INVALID_SOCKET) 
        {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 3;
        }

        iResult = connect(client_socket, ptr->ai_addr, (int)ptr->ai_addrlen);

        if (iResult == SOCKET_ERROR) 
        {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (client_socket == INVALID_SOCKET) 
    {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 5;
    }

    CreateThread(0, 0, Sender, 0, 0, 0);
    CreateThread(0, 0, Receiver, 0, 0, 0);

    Sleep(INFINITE);

    ///////////////////////////////////////////////////////////////////////////////
}
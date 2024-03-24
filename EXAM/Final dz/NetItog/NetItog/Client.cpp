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
bool isFirstOrder = true; // переменная чтобы 1 раз вывести меню

string toLower(const string& str) // Функция преобразования строки в нижний регистр
{
    string lowerStr = str;
    // Применяем transform для преобразования строки в нижний регистр.
    // Это быстрее и чище, чем использование цикла for

    transform(
        lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
        [](unsigned char c) { return tolower(c); }
    );

    return lowerStr;
}

string getOrder() // Функция для сбора информации о заказе
{
    string order;

    if (isFirstOrder == true)
    {
        cout << "\t\tMenu" << endl;
        cout << "hamburger - 5 сек\nsprite - 1 сек\npotato free - 3 сек\nsmoothie - 4 сек\nfalafel - 7 сек\n(Вводить через пробел)\n" << endl;
        cout << "Введите ваш заказ: ";
        isFirstOrder = false;
    }

    getline(cin, order);
    return toLower(order); // переводим в нижний регистр для более простого парсинга на сервере
}

DWORD WINAPI Sender(void* param) 
{
    while (true) 
    {
        string order = getOrder(); // Получаем заказ от пользователя
        if (!order.empty()) 
        {
            send(client_socket, order.c_str(), order.size(), 0);
            cout << "Заказ отправлен. Введите следующий заказ или нажмите Enter для выхода." << endl;
        }
        Sleep(100); // небольшая задержка для предотвращения активного ожидания
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

        // Задержка для гарантии отправки сообщения перед закрытием сокета
        Sleep(1000);
        closesocket(client_socket);
        WSACleanup();
    }
    return TRUE;
}

// UDP multicast с примера 

int main()
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // Получаем дескриптор стандартного вывода

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) // Зарегистрируем обработчик события закрытия консоли
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

    // разрешить адрес сервера и порт
    addrinfo* result = nullptr;
    iResult = getaddrinfo(SERVER_IP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) 
    {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 2;
    }

    addrinfo* ptr = nullptr;

    
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) // пытаться подключиться к адресу, пока не удастся
    {
        client_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol); // создать сокет на стороне клиента для подключения к серверу

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
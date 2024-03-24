#include <winsock2.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <queue>
#include <thread>
#include <mutex>

using namespace std;
int completedOrdersCount = 0; // ���������� ������� ������������ �������

//  ����� ������������� ��� ������� �������� ���� � ��������
//	unordered_map ��������� ����� �������� ����� 
//  ������������� �� �������� �����, ���� �� �������
//  � ������� ���������� ��� �������������� ��������

unordered_map<string, int> preparationTime =
{
	{"hamburger", 5},
	{"sprite", 1},
	{"potato_free", 3},
	{"smoothie", 4},
	{"falafel", 7},
};

struct Order
{
	SOCKET clientSocket;
	string orderDetail;
	time_t orderTime; // ����� ����������� ������
	int preparationTime; // ��������� ����� ������������� � ��������
};

queue<Order> ordersQueue;
// ������������ queue ������ vector, 
// queue ������������� ������� ������� "������ ������ � ������ ����" FIFO.
// ��� �������� ������ � ��������, � �� ����� ��� � vector ����� ���� �� 
// ������� ��������� �������� ���������

void ProcessOrders()
{
	mutex queueMutex;

	while (true)
	{
		if (!ordersQueue.empty())
		{
			lock_guard<mutex> guard(queueMutex); // mutex ��� ������������� ������� � �������
			//������� � �������������� lock_guard ������������ �������������� ������ 
			// ��� �������� ������� � �������������� ������� (������������ ��������) 
			// ��� ������ �� ������� ���������

			Order& currentOrder = ordersQueue.front(); // ���� ������ ����� �� �������, ����� ���������� �� ����, �� �� ������� ��� �� �������
			time_t currentTime = time(nullptr); // ����� ������� �����
			double timePassed = difftime(currentTime, currentOrder.orderTime); // �������, ������� ������� ������ � ������� ������ �� ������

			// ���� ����� ������������� ������ ������, ������ �������� �������, ��� ����� �����, � ������� ��� �� �������
			if (timePassed >= currentOrder.preparationTime)
			{
				completedOrdersCount++; // ����������� ������� �������
				string response = "��� ����� �����! ����� ������: " + to_string(completedOrdersCount) + ".";
				send(currentOrder.clientSocket, response.c_str(), response.length(), 0);
				ordersQueue.pop();
			}
		}

		Sleep(1000); // ������� 1 ������� ����� ��������� ���������
	}
}

int calculatePreparationTime(const string& originalOrder) // ������� ��� ������� ������� ������������� ������ 
{
	int totalTime = 0;
	bool isValidOrder = false; // ���� ��� ��������, ���� �� ���� �� ���� �������� ������� � ������

	// �������� ����������� ������ ��� "potato free" -> "potato_free"
	string modifiedOrder = originalOrder;
	size_t pos = modifiedOrder.find("potato free");

	while (pos != string::npos)
	{
		modifiedOrder.replace(pos, 11, "potato_free");
		pos = modifiedOrder.find("potato free", pos + 1);
	}

	// ������� ��������� ������� ��� �������� ���������� ������� �������� ������
	unordered_map<string, int> itemCounts;
	stringstream ss(modifiedOrder);
	string word;

	while (ss >> word)
	{
		itemCounts[word]++;
	}

	for (unordered_map<string, int>::const_iterator i = itemCounts.begin(); i != itemCounts.end(); ++i)
	{
		const string& key = i->first;
		int value = i->second;
		//	�� ��������� ������� ������� �������� ������ � ����
		// (unordered_map preparationTime), ��� ���� � �������� �����,
		//  � �������� � ����� ��� �������������.���� ������� ������������,
		//  �������� ��� ����� ������������� �� ���������� ������ � ������,
		//  �������� ����� �����.
		unordered_map<string, int>::const_iterator found = preparationTime.find(key);
		if (found != preparationTime.end())
		{
			totalTime += found->second * value; // ����� ������������� * ����������
			isValidOrder = true;
		}
	}

	if (!isValidOrder) // ���� ����� �� �������� �� ����� �������� �������
	{
		return -1; // ���������� -1, ��� ��������� ����������� ������
	}

	return totalTime;
}

void AddOrderToQueue(SOCKET clientSocket, const string& orderDetail) // ��� ������� ���������� ������ � �������
{
	Order newOrder;
	newOrder.clientSocket = clientSocket; // ��������� ����� �������, ����� �����, ���� ���������� �����
	newOrder.orderDetail = orderDetail; // ��������� ������ ������ ��� ����������� ���������
	newOrder.orderTime = time(nullptr); // ��������� ����� ����������� ������
	newOrder.preparationTime = calculatePreparationTime(orderDetail); // ��������� ����� ����� ������������� ������
	ordersQueue.push(newOrder); // ��������� ����� � ������� �� ���������
}

#define MAX_CLIENTS 30
#define DEFAULT_BUFLEN 4096
#pragma comment(lib, "ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // ��������� �������������� _WINSOCK_DEPRECATED_NO_WARNINGS

SOCKET server_socket;

int main()
{
	setlocale(0, "ru");
	system("title Server");
	puts("Start server... DONE.");

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) // ������������� Winsock
	{
		printf("Failed. Error Code: %d", WSAGetLastError());
		return 1;
	}

	
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) // create a socket // �������� ���������� ������
	{
		printf("Could not create socket: %d", WSAGetLastError());
		return 2;
	}
	// puts("Create socket... DONE.");
	// prepare the sockaddr_in structure // ��������� ������ �������

	sockaddr_in server;
	server.sin_family = AF_INET; // ������������� IPv4 �������
	server.sin_addr.s_addr = INADDR_ANY; // ��������� ����������� �� ��� ��������� ����������
	server.sin_port = htons(8888); // ����� ����� ��� �����������

	
	if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) // bind socket // �������� ������ � ������ �������
	{
		printf("Bind failed with error code: %d", WSAGetLastError());
		return 3;
	}

	listen(server_socket, MAX_CLIENTS); // ������� �������� ����������

	
	puts("Server is waiting for incoming connections...\nPlease, start one or more client-side app."); // ������ ����� � ������ �������� �����������

	// ������ ������ ��������� ������, ��� ����� ������
	// ����� ������������ �������
	// fd means "file descriptors"

	// ������������� ��������� ��� ������������ ���������� �������
	fd_set readfds; // https://docs.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-fd_set
	SOCKET client_socket[MAX_CLIENTS] = {};

	thread processingThread(ProcessOrders); // ������ ������ ��� ��������� ������� � ������� ������
	processingThread.detach();	// ����������� �����, �������� ��� �������� ���������� 
								//��� ���������� �������� ���������, ������� ������������� ������� ������� ����� ������

	while (true)
	{
		
		FD_ZERO(&readfds); // �������� ����� fdset
		FD_SET(server_socket, &readfds);// �������� ������� ����� � fdset

		for (int i = 0; i < MAX_CLIENTS; i++)// �������� �������� ������ � fdset
		{
			SOCKET s = client_socket[i];

			if (s > 0)
			{
				FD_SET(s, &readfds);
			}
		}

		if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) // ��������� ���������� �� ����� �� �������, ����-��� ����� NULL, ������� ����� ����������
		{
			printf("select function call failed with error code : %d", WSAGetLastError());
			return 4;
		}

		// ���� ���-�� ��������� �� ������-������, �� ��� �������� ����������
		SOCKET new_socket; // ���������� ��� ������ ����������� ������
		sockaddr_in address; // ����� �������
		int addrlen = sizeof(sockaddr_in);

		if (FD_ISSET(server_socket, &readfds)) // ���� ���� ���������� �� ��������� ������...
		{
			if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) < 0)
			{
				perror("accept function error");
				return 5;
			}

			// ������������� ��������� ������� � ������ ������ - ������������ � �������� �������� � ���������
			printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			
			for (int i = 0; i < MAX_CLIENTS; i++) // �������� ����� ����� � ������ �������
			{
				if (client_socket[i] == 0) // ���� ����� ��������� ����...
				{
					client_socket[i] = new_socket; // ���������� ����� �����
					printf("Adding to list of sockets at index %d\n", i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_CLIENTS; i++) // ���� �����-�� �� ���������� ������� ���������� ���-��
		{
			SOCKET s = client_socket[i];  // ������� �������������� ����� �������
			// ���� ������ ������������ � ������� ������

			if (FD_ISSET(s, &readfds))
			{
				char client_message[DEFAULT_BUFLEN]; // ����� ��� ��������� �� �������
				int client_message_length = recv(s, client_message, DEFAULT_BUFLEN, 0); // �������� ��������� ��������� �� �������

				if (client_message_length > 0) // ���� �������� ���������...
				{
					client_message[client_message_length] = '\0'; // ��������� ������ ����-�������� ��� ���������� ���������.

					// ���������, �������� �� ��������� �������� �� ����������
					if (strcmp(client_message, "quit") == 0)
					{
						cout << "Client #" << i << " requested to disconnect.\n";

						closesocket(s); // ��������� �����
						client_socket[i] = 0; // ����������� ���� ��� ������ ����������

						continue; // ��������� � ���������� �������� �����
					}

					string order(client_message); // ����������� ��������� ������� � ������

					int prepTime = calculatePreparationTime(order); // ������������ ����� �������������

					if (prepTime == -1) // ���� ����� �� ����� ���� ��������...
					{
						// ���� ����� ���������, ���������� ��������� �������
						string response = "��������, ��� ����� �������� ����������� �������.";

						send(s, response.c_str(), response.length(), 0); // ���������� �������
					}
					else // ���� ����� �������...
					{
						stringstream responseStream;

						responseStream << "\n��� ����� ����� ����� ����� " << prepTime << " ������(�).";

						string response = responseStream.str(); // ���������� ��������� ��� �������

						send(s, response.c_str(), response.length(), 0); // ���������� ���������� �� ����� ������� ������������� ������� �������

						// ��������� ����� � �������
						AddOrderToQueue(s, order); // ��� ������ ��������� ����� � ������� ���������
					}
				}
				else // ���� ���������� ���� ������� �������� ��� ��������� ������...
				{
					if (client_message_length == 0)
					{
						// ������ ��������� ������ ����������
						cout << "Client #" << i << " disconnected.\n";
					}
					else
					{
						// ��������� ������
						cout << "recv failed with error: " << WSAGetLastError() << " from client #" << i << "\n";
					}

					closesocket(s); // ��������� �����
					client_socket[i] = 0; // ����������� ����� ��� ����� �����������
					FD_CLR(s, &readfds); // ������� ����� �� fd_set
				}
			}
		}
	}
	WSACleanup();
}
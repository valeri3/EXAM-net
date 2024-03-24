#include <winsock2.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <queue>
#include <thread>
#include <mutex>

using namespace std;
int completedOrdersCount = 0; // Глобальный счётчик обработанных заказов

//  Время приготовления для каждого элемента меню в секундах
//	unordered_map позволяет легко находить время 
//  приготовления по названию блюда, чего не достичь
//  с простой структурой без дополнительных проверок

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
	time_t orderTime; // Время поступления заказа
	int preparationTime; // Расчетное время приготовления в секундах
};

queue<Order> ordersQueue;
// Использовали queue вместо vector, 
// queue автоматически следует правилу "первым пришёл — первым ушёл" FIFO.
// Это упрощает работу с заказами, в то время как с vector нужно было бы 
// вручную управлять порядком элементов

void ProcessOrders()
{
	mutex queueMutex;

	while (true)
	{
		if (!ordersQueue.empty())
		{
			lock_guard<mutex> guard(queueMutex); // mutex для синхронизации доступа к очереди
			//Мьютекс с использованием lock_guard обеспечивает автоматический захват 
			// при создании объекта и автоматическую очистку (освобождение мьютекса) 
			// при выходе из области видимости

			Order& currentOrder = ordersQueue.front(); // Берём первый заказ из очереди, чтобы посмотреть на него, но не удаляем его из очереди
			time_t currentTime = time(nullptr); // Узнаём текущее время
			double timePassed = difftime(currentTime, currentOrder.orderTime); // Считаем, сколько времени прошло с момента заказа до сейчас

			// Если время приготовления заказа прошло, сервер сообщает клиенту, что заказ готов, и удаляет его из очереди
			if (timePassed >= currentOrder.preparationTime)
			{
				completedOrdersCount++; // Увеличиваем счётчик заказов
				string response = "Ваш заказ готов! Номер заказа: " + to_string(completedOrdersCount) + ".";
				send(currentOrder.clientSocket, response.c_str(), response.length(), 0);
				ordersQueue.pop();
			}
		}

		Sleep(1000); // Ожидаем 1 секунду перед следующей проверкой
	}
}

int calculatePreparationTime(const string& originalOrder) // Функция для расчета времени приготовления заказа 
{
	int totalTime = 0;
	bool isValidOrder = false; // Флаг для проверки, есть ли хотя бы один валидный элемент в заказе

	// Точечная модификация только для "potato free" -> "potato_free"
	string modifiedOrder = originalOrder;
	size_t pos = modifiedOrder.find("potato free");

	while (pos != string::npos)
	{
		modifiedOrder.replace(pos, 11, "potato_free");
		pos = modifiedOrder.find("potato free", pos + 1);
	}

	// Создаем временный словарь для подсчета количества каждого элемента заказа
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
		//	Мы проверяем наличие каждого элемента заказа в меню
		// (unordered_map preparationTime), где ключ — название блюда,
		//  а значение — время его приготовления.Если элемент присутствует,
		//  умножаем его время приготовления на количество порций в заказе,
		//  суммируя общее время.
		unordered_map<string, int>::const_iterator found = preparationTime.find(key);
		if (found != preparationTime.end())
		{
			totalTime += found->second * value; // Время приготовления * количество
			isValidOrder = true;
		}
	}

	if (!isValidOrder) // Если заказ не содержит ни одной валидной позиции
	{
		return -1; // Возвращаем -1, как индикатор невалидного заказа
	}

	return totalTime;
}

void AddOrderToQueue(SOCKET clientSocket, const string& orderDetail) // Это функция добавления заказа в очередь
{
	Order newOrder;
	newOrder.clientSocket = clientSocket; // Сохраняем сокет клиента, чтобы знать, кому отправлять ответ
	newOrder.orderDetail = orderDetail; // Сохраняем детали заказа для последующей обработки
	newOrder.orderTime = time(nullptr); // Фиксируем время поступления заказа
	newOrder.preparationTime = calculatePreparationTime(orderDetail); // Вычисляем общее время приготовления заказа
	ordersQueue.push(newOrder); // Добавляем заказ в очередь на обработку
}

#define MAX_CLIENTS 30
#define DEFAULT_BUFLEN 4096
#pragma comment(lib, "ws2_32.lib") // Winsock library
#pragma warning(disable:4996) // отключаем предупреждение _WINSOCK_DEPRECATED_NO_WARNINGS

SOCKET server_socket;

int main()
{
	setlocale(0, "ru");
	system("title Server");
	puts("Start server... DONE.");

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) // Инициализация Winsock
	{
		printf("Failed. Error Code: %d", WSAGetLastError());
		return 1;
	}

	
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) // create a socket // Создание серверного сокета
	{
		printf("Could not create socket: %d", WSAGetLastError());
		return 2;
	}
	// puts("Create socket... DONE.");
	// prepare the sockaddr_in structure // Настройка адреса сервера

	sockaddr_in server;
	server.sin_family = AF_INET; // Использование IPv4 адресов
	server.sin_addr.s_addr = INADDR_ANY; // Принимать подключения на все доступные интерфейсы
	server.sin_port = htons(8888); // Номер порта для подключений

	
	if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) // bind socket // Привязка сокета к адресу сервера
	{
		printf("Bind failed with error code: %d", WSAGetLastError());
		return 3;
	}

	listen(server_socket, MAX_CLIENTS); // слушать входящие соединения

	
	puts("Server is waiting for incoming connections...\nPlease, start one or more client-side app."); // Сервер готов к приему входящих подключений

	// размер нашего приемного буфера, это длина строки
	// набор дескрипторов сокетов
	// fd means "file descriptors"

	// Инициализация структуры для отслеживания активности сокетов
	fd_set readfds; // https://docs.microsoft.com/en-us/windows/win32/api/winsock/ns-winsock-fd_set
	SOCKET client_socket[MAX_CLIENTS] = {};

	thread processingThread(ProcessOrders); // Запуск потока для обработки заказов в фоновом режиме
	processingThread.detach();	// Отсоединяем поток, позволяя ему работать независимо 
								//При завершении основной программы, система автоматически очистит ресурсы этого потока

	while (true)
	{
		
		FD_ZERO(&readfds); // очистить сокет fdset
		FD_SET(server_socket, &readfds);// добавить главный сокет в fdset

		for (int i = 0; i < MAX_CLIENTS; i++)// добавить дочерние сокеты в fdset
		{
			SOCKET s = client_socket[i];

			if (s > 0)
			{
				FD_SET(s, &readfds);
			}
		}

		if (select(0, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) // дождитесь активности на любом из сокетов, тайм-аут равен NULL, поэтому ждите бесконечно
		{
			printf("select function call failed with error code : %d", WSAGetLastError());
			return 4;
		}

		// если что-то произошло на мастер-сокете, то это входящее соединение
		SOCKET new_socket; // Переменная для нового клиентского сокета
		sockaddr_in address; // Адрес клиента
		int addrlen = sizeof(sockaddr_in);

		if (FD_ISSET(server_socket, &readfds)) // Если есть активность на серверном сокете...
		{
			if ((new_socket = accept(server_socket, (sockaddr*)&address, &addrlen)) < 0)
			{
				perror("accept function error");
				return 5;
			}

			// информировать серверную сторону о номере сокета - используется в командах отправки и получения
			printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			
			for (int i = 0; i < MAX_CLIENTS; i++) // добавить новый сокет в массив сокетов
			{
				if (client_socket[i] == 0) // Если нашли свободный слот...
				{
					client_socket[i] = new_socket; // Запоминаем новый сокет
					printf("Adding to list of sockets at index %d\n", i);
					break;
				}
			}
		}

		for (int i = 0; i < MAX_CLIENTS; i++) // если какой-то из клиентских сокетов отправляет что-то
		{
			SOCKET s = client_socket[i];  // Текущий обрабатываемый сокет клиента
			// если клиент присутствует в сокетах чтения

			if (FD_ISSET(s, &readfds))
			{
				char client_message[DEFAULT_BUFLEN]; // Буфер для сообщения от клиента
				int client_message_length = recv(s, client_message, DEFAULT_BUFLEN, 0); // Пытаемся прочитать сообщение от клиента

				if (client_message_length > 0) // Если получили сообщение...
				{
					client_message[client_message_length] = '\0'; // Завершаем строку нуль-символом для корректной обработки.

					// Проверяем, является ли сообщение командой на отключение
					if (strcmp(client_message, "quit") == 0)
					{
						cout << "Client #" << i << " requested to disconnect.\n";

						closesocket(s); // Закрываем сокет
						client_socket[i] = 0; // Освобождаем слот для нового соединения

						continue; // Переходим к следующему итерации цикла
					}

					string order(client_message); // Преобразуем сообщение клиента в строку

					int prepTime = calculatePreparationTime(order); // Рассчитываем время приготовления

					if (prepTime == -1) // Если заказ не может быть выполнен...
					{
						// Если заказ невалиден, отправляем сообщение клиенту
						string response = "Извините, ваш заказ содержит недоступные позиции.";

						send(s, response.c_str(), response.length(), 0); // Уведомляем клиента
					}
					else // Если заказ валиден...
					{
						stringstream responseStream;

						responseStream << "\nВаш заказ будет готов через " << prepTime << " секунд(ы).";

						string response = responseStream.str(); // Составляем сообщение для клиента

						send(s, response.c_str(), response.length(), 0); // Отправляем информацию об общем времени приготовления обратно клиенту

						// Добавляем заказ в очередь
						AddOrderToQueue(s, order); // Эта строка добавляет заказ в очередь обработки
					}
				}
				else // Если соединение было закрыто клиентом или произошла ошибка...
				{
					if (client_message_length == 0)
					{
						// Клиент корректно закрыл соединение
						cout << "Client #" << i << " disconnected.\n";
					}
					else
					{
						// Произошла ошибка
						cout << "recv failed with error: " << WSAGetLastError() << " from client #" << i << "\n";
					}

					closesocket(s); // Закрываем сокет
					client_socket[i] = 0; // Освобождаем место для новых подключений
					FD_CLR(s, &readfds); // Удаляем сокет из fd_set
				}
			}
		}
	}
	WSACleanup();
}
#pragma once
//ipconfig
#include<queue>
#include<string>
#include <Windows.h>
#include<map>
#include"CheckClient.h"
#define MAX 100
#define MAXREQUEST 1000

int  clientId = -1;

WIN32_FIND_DATA findData;

SOCKET serverSocket, clientSocket, clientSocketFile, serverSocketFile;

//int numberOfClient=2;
//bool clientOnline[MAX] = { false };

//std::vector<HANDLE> Client(numberOfClient);// handle tu client
DWORD dwWaitStatus;
std::wstring SERVER_FOLDER = L"C:\\Users\\NGUYEN MANH DUNG\\Desktop\\HDHcode\\Datafolder\\Server";
std::vector<std::wstring> clientFolders;

std::mutex queueMutex;
std::condition_variable queueCondition;
std::queue<int> clientIdRequest; // Hàng đợi chứa các yêu cầu từ client
std::queue<int> initQueue;
std::queue<dataInfo> oneClientRequest;
std::map<int, std::queue<dataInfo>> cloneUpdate;

std::atomic<bool> busy(false); // Trạng thái bận rộn của server
std::atomic<bool> done(false); // 'D'

std::mutex clientMutex;            // Mutex bảo vệ truy cập vào clientSockets và clientOnline
std::map<int, SOCKET> clientSockets; // Sử dụng std::map để lưu trữ socket của client
std::map<int, SOCKET> clientSocketsFile; // Sử dụng std::map để lưu trữ socket file của client


std::mutex clientSocketsMutex; // Khai báo mutex để bảo vệ access đến clientSockets
std::map<std::wstring, std::time_t> myMap;
std::vector<int> activeClient;
int currentId=-1;
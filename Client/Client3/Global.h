#pragma once
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <fstream>
#include<vector>
#include<Windows.h>
#include<thread>
#include<mutex>
#include <ctime> // Để xử lý time_t
#include <sstream>
#include<queue>
#include <locale> // Thư viện để hỗ trợ wchar_t
#include <cwchar> // for std::mbstowcs
#include<map>
#include <condition_variable>


extern std::condition_variable Condition;
extern int myId;
extern std::wstring clientFolders;
extern HANDLE Client;
extern DWORD dwWaitStatus;
extern WIN32_FIND_DATA findData;
extern SOCKET serverSocket, serverSocketFile;
extern std::map<std::wstring, std::time_t> myMap;


struct dataInfo {
    char msg; // 'I' - start; 'C' - change; 'F' - folder; 'f' - file; D - done
    int myId;
    size_t fileSize; // Size of file data
    std::wstring fileName; // File name
    std::wstring directoryInfo; // Directory info
    std::time_t modifiedTime;

};
extern std::queue<dataInfo> oneClientRequest;
extern bool update;//clone
extern bool serverIsHandling;
extern std::queue<dataInfo> oneServerRequest;
extern std::mutex queueMutex;  // Khai báo mutex
extern bool cloning;
extern bool justupdate;
extern bool folderupdate;
extern bool fileupdate;
extern int countupdate;
extern std::string ip;
void printQueue(std::queue<dataInfo> q);
void clearQueue(std::queue<dataInfo>& q);
bool sentDataFuncion(SOCKET sock, const dataInfo& data);
bool receiveDataFuncion(SOCKET sock, dataInfo& data);
bool SendFileToServer(SOCKET sockfd, const std::wstring& srcFile);
bool ReceiveFileFromServer(SOCKET sockfd, const std::wstring& destFile);
void cloneFromServer(SOCKET serverSocket, SOCKET serverSocketFile);
bool writeMapToTxt(const std::map<std::wstring, std::time_t>& myMap);
bool readFromTxtToMap(std::map<std::wstring, std::time_t>& myMap);
bool makeDataMap(const std::wstring serverFolder);
bool updateDataMap();
void cloneFromServerMain(SOCKET serverSocket, SOCKET serverSocketFile);
#pragma once
#include <winsock2.h>
#include<string>
#include<queue>
#include<map>
#include <ctime> // Để xử lý time_t

struct dataInfo {
    char msg; // 'I' - start; 'C' - change; 'F' - folder; 'f' - file; Done; N next
    int myId;
    size_t fileSize; // Size of file data
    std::wstring fileName; // File name
    std::wstring directoryInfo; // Directory info
    std::time_t modifiedTime; 
};

void turnOnActive(int i);
void printActiveClient();
bool checkOffLine(int i);

bool sentDataFuncion(SOCKET sock, const dataInfo& fileData);
	
bool receiveDataFuncion(SOCKET sock, dataInfo & fileData);

bool SendFileToClient(SOCKET sockfd, const std::wstring& srcFile);

bool ReceiveFileFromClient(SOCKET sockfd, const std::wstring& destFile);

void HandleClientUpdate(SOCKET clientSocket, SOCKET clientSocketFile);
void printQueue(std::queue<dataInfo> q);
bool makeDataMap(const std::wstring serverFolder);
bool updateDataMap();
bool writeMapToTxt(const std::map<std::wstring, std::time_t>& myMap);
bool readFromTxtToMap(std::map<std::wstring, std::time_t>& myMap);
bool checkActive(int clientId);
void cloneToOneClient(int clientId, const std::wstring& serverFolder, SOCKET clientSocket, SOCKET clientSocketFile);// phải tách luồng, chi duoc gui file
bool cloneToOtherClient(int clientId);
bool cloneFirstTime(int clientId, SOCKET clientSocket, SOCKET clientSocketFile);
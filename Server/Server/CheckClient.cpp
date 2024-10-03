#include "CheckClient.h"
#include<iostream>
#include <winsock2.h>
#include<fstream>
#include<vector>
#include <cwchar> // for std::mbstowcs
#include<thread>
#include<mutex>
#include<map>
#include <string>
#include <sstream>
#include <ctime>
#include <locale> // Thư viện để hỗ trợ wchar_t

extern int  clientId;

extern WIN32_FIND_DATA findData;

extern SOCKET serverSocket, clientSocket;
extern std::mutex queueMutex;
extern std::condition_variable queueCondition;


//extern int numberOfClient;
//extern bool clientOnline[];

extern std::vector<HANDLE> Client;// handle tu client
extern DWORD dwWaitStatus;
extern std::wstring SERVER_FOLDER;
extern std::vector<std::wstring> clientFolders;
extern std::map<int, std::queue<dataInfo>> cloneUpdate;
extern std::queue<int> clientIdRequest; // Hàng đợi chứa các yêu cầu từ client
extern std::map<int, SOCKET> clientSockets; // Sử dụng std::map để lưu trữ socket của client
extern std::atomic<bool> done; // Trạng thái bận rộn của server
extern std::map<std::wstring, std::time_t> myMap;
extern std::map<int, SOCKET> clientSocketsFile; // Sử dụng std::map để lưu trữ socket file của client
extern std::queue<dataInfo> oneClientRequest;

#pragma comment(lib, "ws2_32.lib")  // Link with ws2_32.lib

//void addClient() {
//	numberOfClient++;
//}
//
////bat trang thai cua client;
//void turnOnActive(int i) {
//	if (clientId >= 0 || clientId < numberOfClient) {
//		clientOnline[i] = true;
//	}
//}

//void printActiveClient() {
//	std::cerr << "Active Client: ";
//	for (int i = 1; i <= numberOfClient; i++) {
//		if (clientOnline[i]) {
//			std::cerr << i << ", ";
//		}
//	}
//	std::cerr << std::endl;
//}

//
bool makeDataMap(const std::wstring serverFolder) {//tao map chua thong tin
	//extern std::map<std::wstring, dataInfo> myMap;
	// Kiểm tra xem file có rỗng không

	std::time_t tgian;
	std::ifstream file(L"map.txt");
	if (!file.peek() == std::ifstream::traits_type::eof()) {// file có data thì đọc từ file
		std::cerr << "map.txt has data" << std::endl;
		readFromTxtToMap(myMap);
	}
	else {// file ko data thì đọc từ folder
		WIN32_FIND_DATA findFileData;
		HANDLE hFind = FindFirstFile((serverFolder + L"\\*").c_str(), &findFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			std::wcerr << L"Error: Unable to access folder " << serverFolder << std::endl;
			return false;
		}

		do {
			if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
				std::wstring srcFile = serverFolder + L"\\" + findFileData.cFileName;
				std::wstring relativeFile = srcFile.substr(serverFolder.length() + 1, srcFile.length());
				if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

					// Thêm vào map
					tgian = findFileData.ftLastWriteTime.dwLowDateTime;
					myMap[relativeFile] = tgian;
					makeDataMap(srcFile);
				}
				else {// neu la file
					tgian = findFileData.ftLastWriteTime.dwLowDateTime;
					myMap[relativeFile] = tgian;

				}
			}
		} while (FindNextFile(hFind, &findFileData) != 0);


		return true;
		FindClose(hFind);
	}
}
bool updateDataMap(char Action, std::wstring path, time_t tgian = 0) {//D;delete, A:add;
	//relative path
	switch (Action) {
	case 'D'://Delele
		myMap.erase(path);
		std::wcerr << "Delete from Map: " << path << std::endl;
		break;
	case 'A'://Add
		myMap[path] = tgian;
		break;
	default:
		std::cerr << "ERROR at Update Map" << std::endl;
		break;

	}

	return true;
}
bool writeMapToTxt(const std::map<std::wstring, std::time_t>& myMap) {// da co mao san roi, chi ghi thoi
	// Xóa file hiện có nếu tồn tại
	std::wstring outputFilePath = L"map.txt";
	remove(std::string(outputFilePath.begin(), outputFilePath.end()).c_str());

	std::wofstream outputFile(outputFilePath);
	if (!outputFile.is_open()) {
		std::wcerr << L"Failed to open output file: " << outputFilePath << std::endl;
		return false;
	}

	for (const auto& entry : myMap) {
		outputFile << entry.first << " " << entry.second << std::endl;
	}

	outputFile.close();
	return true;
}
bool readFromTxtToMap(std::map<std::wstring, std::time_t>& myMap) {// da co txt san roi chi ghi ra Map thoi
	std::wifstream inputFile(L"map.txt");
	if (!inputFile.is_open()) {
		std::wcerr << L"Failed to open input file: map.txt" << std::endl;
		return false;
	}

	std::wstring path;
	std::time_t timeValue;

	while (inputFile >> path >> timeValue) {
		myMap[path] = timeValue; // Lưu vào map
	}

	inputFile.close();
	return true;
}

bool sentDataFuncion(SOCKET sock, const dataInfo& data) {
	// Gửi dữ liệu nguyên thủy (msg, myId, fileSize, modifiedTime)
	if (send(sock, reinterpret_cast<const char*>(&data.msg), sizeof(data.msg), 0) == SOCKET_ERROR ||
		send(sock, reinterpret_cast<const char*>(&data.myId), sizeof(data.myId), 0) == SOCKET_ERROR ||
		send(sock, reinterpret_cast<const char*>(&data.fileSize), sizeof(data.fileSize), 0) == SOCKET_ERROR ||
		send(sock, reinterpret_cast<const char*>(&data.modifiedTime), sizeof(data.modifiedTime), 0) == SOCKET_ERROR) {
		std::cerr << "Failed to send primitive data." << std::endl;
		return false;
	}

	// Gửi kích thước của fileName và directoryInfo
	size_t fileNameSize = data.fileName.size() * sizeof(wchar_t);
	size_t directoryInfoSize = data.directoryInfo.size() * sizeof(wchar_t);

	if (send(sock, reinterpret_cast<const char*>(&fileNameSize), sizeof(fileNameSize), 0) == SOCKET_ERROR ||
		send(sock, reinterpret_cast<const char*>(&directoryInfoSize), sizeof(directoryInfoSize), 0) == SOCKET_ERROR) {
		std::cerr << "Failed to send string sizes." << std::endl;
		return false;
	}

	// Gửi dữ liệu của fileName và directoryInfo
	if (send(sock, reinterpret_cast<const char*>(data.fileName.c_str()), fileNameSize, 0) == SOCKET_ERROR ||
		send(sock, reinterpret_cast<const char*>(data.directoryInfo.c_str()), directoryInfoSize, 0) == SOCKET_ERROR) {
		std::cerr << "Failed to send wstrings." << std::endl;
		return false;
	}

	return true;
}

bool receiveDataFuncion(SOCKET sock, dataInfo& data) {
	// Nhận dữ liệu nguyên thủy (msg, myId, fileSize, modifiedTime)
	if (recv(sock, reinterpret_cast<char*>(&data.msg), sizeof(data.msg), 0) == SOCKET_ERROR ||
		recv(sock, reinterpret_cast<char*>(&data.myId), sizeof(data.myId), 0) == SOCKET_ERROR ||
		recv(sock, reinterpret_cast<char*>(&data.fileSize), sizeof(data.fileSize), 0) == SOCKET_ERROR ||
		recv(sock, reinterpret_cast<char*>(&data.modifiedTime), sizeof(data.modifiedTime), 0) == SOCKET_ERROR) {
		std::cerr << "Failed to receive primitive data." << std::endl;// lỗi
		return false;
	}

	// Nhận kích thước của fileName và directoryInfo
	size_t fileNameSize;
	size_t directoryInfoSize;
	if (recv(sock, reinterpret_cast<char*>(&fileNameSize), sizeof(fileNameSize), 0) == SOCKET_ERROR ||
		recv(sock, reinterpret_cast<char*>(&directoryInfoSize), sizeof(directoryInfoSize), 0) == SOCKET_ERROR) {
		std::cerr << "Failed to receive string sizes." << std::endl;
		return false;
	}

	// Nhận fileName và directoryInfo
	wchar_t* fileNameBuffer = new wchar_t[fileNameSize / sizeof(wchar_t)];
	wchar_t* directoryInfoBuffer = new wchar_t[directoryInfoSize / sizeof(wchar_t)];

	if (recv(sock, reinterpret_cast<char*>(fileNameBuffer), fileNameSize, 0) == SOCKET_ERROR ||
		recv(sock, reinterpret_cast<char*>(directoryInfoBuffer), directoryInfoSize, 0) == SOCKET_ERROR) {
		std::cerr << "Failed to receive wstrings." << std::endl;
		delete[] fileNameBuffer;
		delete[] directoryInfoBuffer;
		return false;
	}

	// Chuyển đổi từ buffer sang std::wstring
	data.fileName = std::wstring(fileNameBuffer, fileNameSize / sizeof(wchar_t));
	data.directoryInfo = std::wstring(directoryInfoBuffer, directoryInfoSize / sizeof(wchar_t));

	delete[] fileNameBuffer;
	delete[] directoryInfoBuffer;

	return true;
}


bool ReceiveFileFromClient(SOCKET sockfd, const std::wstring& destFile) {// socketfile
	std::ofstream file(destFile, std::ios::binary);
	if (!file.is_open()) {
		std::wcerr << L"Failed to open file for writing: " << destFile << std::endl;
		return false;
	}

	// Nhận kích thước file
	size_t fileSize;
	if (recv(sockfd, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0) == SOCKET_ERROR) {
		std::wcerr << L"Failed to receive file size." << std::endl;
		return false;
	}

	// Nhận dữ liệu file và lưu vào file đích
	char buffer[4096];
	size_t bytesReceived = 0;
	while (fileSize > 0) {
		int bytesRead = recv(sockfd, buffer, sizeof(buffer), 0);
		if (bytesRead == SOCKET_ERROR || bytesRead == 0) {
			std::wcerr << L"Failed to receive file data." << std::endl;
			return false;
		}

		file.write(buffer, bytesRead);
		fileSize -= bytesRead;
		bytesReceived += bytesRead;
	}

	std::wcout << L"File " << destFile << L" received successfully. Total bytes received: " << bytesReceived << std::endl;
	file.close();
	return true;
}

void HandleClientUpdate(SOCKET clientSocket, SOCKET clientSocketFile) {// add file from client to server - done
	dataInfo msgdata;
	std::map<std::wstring, std::time_t> tempMap(myMap);// tao 1 Map tam de xem file nao ma client vua xoa


	while (true) {
		if (oneClientRequest.empty() == false) {
			msgdata = oneClientRequest.front();
			oneClientRequest.pop();

			if (msgdata.msg == 'D') {
				std::cout << "break when meet D" << std::endl;
				break;
			}

			else if (msgdata.msg == 'F') { // Thư mục
				std::cerr << msgdata.msg << L" From Client " << msgdata.myId << std::endl;

				std::wstring serverDirectory = SERVER_FOLDER + L"\\" + msgdata.directoryInfo;

				if (!CreateDirectory(serverDirectory.c_str(), NULL)) {
					if (GetLastError() == ERROR_ALREADY_EXISTS) {
						std::wcerr << L"Directory already exists: " << serverDirectory << std::endl;

					}
					else {
						std::wcerr << L"Failed to create directory on server: " << serverDirectory << std::endl;
					}
				}
				else {
					std::wcerr << L"Successfully created directory: " << serverDirectory << std::endl;
					updateDataMap('A', msgdata.directoryInfo, msgdata.modifiedTime);//add

				}

				tempMap.erase(msgdata.directoryInfo);
			}
			else if (msgdata.msg == 'f') { // File
				std::wstring fullFilePath = SERVER_FOLDER + L"\\" + msgdata.directoryInfo;

				if (!(myMap.count(msgdata.directoryInfo) > 0 && myMap.at(msgdata.directoryInfo) == msgdata.modifiedTime)) {

					dataInfo response;
					response.myId = msgdata.myId;
					response.msg = 'n'; // next: tai file len di
					response.fileSize = 0;
					response.fileName = L"NULL";
					response.directoryInfo = L"NULL";
					response.modifiedTime = 0;

					sentDataFuncion(clientSocket, response);
					std::cerr << "Sent n to Client" << std::endl;

					ReceiveFileFromClient(clientSocketFile, fullFilePath);

					response.msg = 'd'; // done
					sentDataFuncion(clientSocket, response);
					std::cerr << "Sent d to Client" << std::endl;
					updateDataMap('A', msgdata.directoryInfo, msgdata.modifiedTime);//add
				}
				else {
					dataInfo response;
					response.myId = msgdata.myId;
					response.msg = 'l'; // lext: ko phai gui file len
					response.fileSize = 0;
					response.fileName = L"NULL";
					response.directoryInfo = L"NULL";
					response.modifiedTime = 0;

					sentDataFuncion(clientSocket, response);
					std::cerr << "Sent l to Client" << std::endl;
				}

				tempMap.erase(msgdata.directoryInfo);
			}

			dataInfo response;
			response.myId = msgdata.myId;
			response.msg = 'N'; // next
			response.fileSize = 0;
			response.fileName = L"NULL";
			response.directoryInfo = L"NULL";
			response.modifiedTime = 0;

			sentDataFuncion(clientSocket, response);
			std::cerr << "Sent N to Client" << std::endl;
		}
	}

	// xoá các folder mà ko có ở client
	for (auto it = tempMap.begin(); it != tempMap.end(); ) {
		std::wstring key = it->first;
		std::wstring key2 = SERVER_FOLDER + L"\\" + key;

		DWORD attributes = GetFileAttributes(key2.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES) {
			std::wcerr << L"Error: Unable to access " << key << std::endl;
			return;
		}

		// Nếu là folder, sử dụng RemoveDirectory
		if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (RemoveDirectory(key2.c_str())) {
				std::wcout << L"Deleted folder: " << key << std::endl;
				updateDataMap('D', key);//add
			}
			else {
				std::wcerr << L"Failed to delete folder: " << key << std::endl;
			}
		}
		else { // Nếu là file, sử dụng DeleteFile
			if (DeleteFile(key2.c_str())) {
				std::wcout << L"Deleted file: " << key << std::endl;
				updateDataMap('D', key);//add
			}
			else {
				std::wcerr << L"Failed to delete file: " << key << std::endl;
			}
		}
		it = tempMap.erase(it); // erase trả về iterator tiếp theo
	}
}


void printQueue(std::queue<dataInfo> q) {
	std::cout << " Queue: ";
	while (!q.empty()) {
		std::cout << q.front().msg << " ";  // Lấy phần tử đầu tiên
		q.pop();  // Loại bỏ phần tử đầu tiên (chỉ tác động lên bản sao)
	}
	std::cout << std::endl;
}

bool SendFileToClient(SOCKET sockfd, const std::wstring& srcFile) {// gui file
	std::ifstream file(srcFile, std::ios::binary);
	if (!file.is_open()) {
		std::wcerr << L"Failed to open file: " << srcFile << std::endl;
		return false;
	}

	// Lấy kích thước file
	file.seekg(0, std::ios::end);
	size_t fileSize = file.tellg();
	file.seekg(0, std::ios::beg);




	// Gửi kích thước file trước
	if (send(sockfd, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0) == SOCKET_ERROR) {
		std::wcerr << L"Failed to send file size." << std::endl;
		return false;
	}

	// Gửi dữ liệu file
	char buffer[4096];
	size_t bytesSent = 0;
	while (fileSize > 0) {
		file.read(buffer, sizeof(buffer));
		std::streamsize bytesRead = file.gcount();

		if (send(sockfd, buffer, bytesRead, 0) == SOCKET_ERROR) {
			std::wcerr << L"Failed to send file data." << std::endl;
			return false;
		}

		fileSize -= bytesRead;
		bytesSent += bytesRead;
	}

	std::wcout << L"File " << srcFile << L" sent successfully. Total bytes sent: " << bytesSent << std::endl;
	return true;
}



void cloneToOneClient(int clientId, const std::wstring& serverFolder, SOCKET clientSocket, SOCKET clientSocketFile) {// phải tách luồng, chi duoc gui file
	if (cloneUpdate.find(clientId) == cloneUpdate.end()) {
		std::cerr << "Cant find ClientId " << clientId << std::endl;
		return ;
	}

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile((serverFolder + L"\\*").c_str(), &findFileData);
	// send D F f  rev n,l, N, d (cloneUpdate.at(clientId).push(datamsg);)
	if (hFind == INVALID_HANDLE_VALUE) {
		std::wcerr << L"Error: Unable to access folder " << serverFolder << std::endl;
		return ;
	}

	do {
		// Bỏ qua thư mục '.' và '..'
		if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
			std::wstring srcFile = serverFolder + L"\\" + findFileData.cFileName;
			std::wstring relativeFile = srcFile.substr(SERVER_FOLDER.length() + 1, srcFile.length());

			// Kiểm tra nếu là thư mục
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

				dataInfo msgdata;
				msgdata.myId = -1;
				msgdata.msg = 'F';
				msgdata.fileSize = 0;
				msgdata.fileName = L"Folder";
				msgdata.directoryInfo = relativeFile;
				msgdata.modifiedTime = findFileData.ftLastWriteTime.dwLowDateTime;



				sentDataFuncion(clientSocket, msgdata);
				std::wcerr << msgdata.msg << " to client " << msgdata.directoryInfo << std::endl;


				while (true) {
					if (cloneUpdate.at(clientId).empty() == false) {
						msgdata = cloneUpdate.at(clientId).front();
						cloneUpdate.at(clientId).pop();
						std::wcerr << "Receive " << msgdata.msg << "From Client" << std::endl;
						if (msgdata.msg != 'N') {
							std::cerr << "EXPECT--------------------------------N" << std::endl;
							return ;
						}
						break;
					}
				}



				//UpdateToServer(srcFile, serverSocket, serverSocketFile);
				cloneToOneClient(clientId, srcFile, clientSocket, clientSocketFile);
			}
			else {
				// Thông báo cho server về tệp và sau đó gửi tệp
				dataInfo msgdata;
				msgdata.myId = -1;
				msgdata.msg = 'f';
				msgdata.fileSize = 0;
				msgdata.fileName = L"Folder";
				msgdata.directoryInfo = relativeFile;
				msgdata.modifiedTime = findFileData.ftLastWriteTime.dwLowDateTime;

				sentDataFuncion(clientSocket, msgdata);
				std::wcerr << msgdata.msg << " to client " << msgdata.directoryInfo << std::endl;
				std::wstring fileInfo = srcFile;


				while (true) {
					if (cloneUpdate.at(clientId).empty() == false) {
						msgdata = cloneUpdate.at(clientId).front();
						cloneUpdate.at(clientId).pop();
						std::wcerr << "Receive " << msgdata.msg << "From Server" << std::endl;

						if (msgdata.msg == 'l') {
							goto label;
						}
						else if (msgdata.msg == 'n') {
							std::cerr << "nhan duoc 'n'" << std::endl;
							break;
						}
						else {
							std::cerr << "Error receive n or l" << std::endl;
							return ;
						}

						//break;
					}
				}

				//send(serverSocket, reinterpret_cast<const char*>(fileInfo.c_str()), fileInfo.size() * sizeof(wchar_t), 0);

				if (!SendFileToClient(clientSocketFile, srcFile)) {
					std::wcerr << L"Failed to send file: " << srcFile << std::endl;
				}

				while (true) {
					if (cloneUpdate.at(clientId).empty() == false) {
						msgdata = cloneUpdate.at(clientId).front();
						cloneUpdate.at(clientId).pop();
						std::wcerr << "Receive " << msgdata.msg << "From Client" << std::endl;
						if (msgdata.msg != 'd') {

							std::cerr << "EXPECT--------------------------------d" << std::endl;

							return ;
						}
						break;
					}
				}
			label:

				while (true) {
					if (cloneUpdate.at(clientId).empty() == false) {
						msgdata = cloneUpdate.at(clientId).front();
						cloneUpdate.at(clientId).pop();
						std::wcerr << "Receive " << msgdata.msg << "From client" << std::endl;
						if (msgdata.msg != 'N') {
							return ;
						}
						break;
					}
				}

			}
		}

	} while (FindNextFile(hFind, &findFileData) != 0);


	FindClose(hFind);

	std::cout << "-----------------" << std::endl;



	//clearQueue(oneServerRequest);

	//return true;
}


bool cloneToOtherClient(int clientId) {

	std::cerr << "Start Clone to Other Client" << std::endl;
	std::vector<std::thread> threadClone;
	for (auto it = clientSockets.begin(); it != clientSockets.end(); it++) {
		std::wstring temp = SERVER_FOLDER;
		if (it->first != clientId) {

			auto it1 = clientSockets.find(it->first);
			auto it2 = clientSocketsFile.find(it->first);
			if (it1 == clientSockets.end()) {
				std::cerr << "Client socket not found for client ID: " << clientId << std::endl;
				return false;
			}
			if (it2 == clientSocketsFile.end()) {
				std::cerr << "Client socket file not found for client ID: " << clientId << std::endl;
				return false;
			}

			dataInfo response;
			response.myId = -1;
			response.msg = 'U'; // next: tai file len di
			response.fileSize = 0;
			response.fileName = L"NULL";
			response.directoryInfo = L"NULL";
			response.modifiedTime = 0;
			sentDataFuncion(it->second, response);
			std::cerr << "Sent U to client: " << it->first << std::endl;



			threadClone.push_back(std::thread(cloneToOneClient, it->first, temp, it1->second, it2->second));

			
		}
	}
	for (auto& thread : threadClone) {
		thread.join();
	}

	for (auto it = clientSockets.begin(); it != clientSockets.end(); it++) {
		if (it->first != clientId) {		
			dataInfo response;
			response.myId = -1;
			response.msg = 'D'; // next: tai file len di
			response.fileSize = 0;
			response.fileName = L"NULL";
			response.directoryInfo = L"NULL";
			response.modifiedTime = 0;
			sentDataFuncion(it->second, response);
			std::cout << "Sennt D to client\nDone update to client " << it->first << std::endl;
		}

	}


	std::cerr << "-------------------------Current processing----------------------" << std::endl;

	return true;
}

bool cloneFirstTime(int clientId, SOCKET clientSocket, SOCKET clientSocketFile) {

	dataInfo response;
	response.myId = -1;
	response.msg = 'U'; // next: tai file len di
	response.fileSize = 0;
	response.fileName = L"NULL";
	response.directoryInfo = L"NULL";
	response.modifiedTime = 0;
	sentDataFuncion(clientSocket, response);
	std::cerr << "Sent U to client: " << clientId << std::endl;
	cloneToOneClient(clientId, SERVER_FOLDER, clientSocket, clientSocketFile);
	response.msg = 'D';
	sentDataFuncion(clientSocket, response);

	return true;
}
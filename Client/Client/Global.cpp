#include "Global.h"

int myId = 1;
std::wstring clientFolders = L"C:\\Users\\NGUYEN MANH DUNG\\Desktop\\HDHcode\\Datafolder\\Client1";
HANDLE Client;
DWORD dwWaitStatus;
WIN32_FIND_DATA findData;

SOCKET serverSocket, serverSocketFile;

bool serverIsHandling = false;
std::queue<dataInfo> oneServerRequest;
std::mutex queueMutex;  // Khai báo mutex
std::map<std::wstring, std::time_t> myMap;
std::queue<dataInfo> oneClientRequest;
std::condition_variable Condition;
std::string ip = "127.0.0.1";
bool justupdate = false;
bool update = false;
bool cloning = false;
int countupdate = 0;
bool folderupdate = false;
bool fileupdate = false;// khi file update sẽ có 2 detect change
bool makeDataMap(const std::wstring clientFolders) {//tao map chua thong tin
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
		HANDLE hFind = FindFirstFile((clientFolders + L"\\*").c_str(), &findFileData);
		if (hFind == INVALID_HANDLE_VALUE) {
			std::wcerr << L"Error: Unable to access folder " << clientFolders << std::endl;
			return false;
		}

		do {
			if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
				std::wstring srcFile = clientFolders + L"\\" + findFileData.cFileName;
				std::wstring relativeFile = srcFile.substr(clientFolders.length() + 1, srcFile.length());
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


void printQueue(std::queue<dataInfo> q) {
	std::cout << " Queue: ";
	while (!q.empty()) {
		std::cout << q.front().msg << " ";  // Lấy phần tử đầu tiên
		q.pop();  // Loại bỏ phần tử đầu tiên (chỉ tác động lên bản sao)
	}
	std::cout << " End OneQueue " << std::endl;
}
void clearQueue(std::queue<dataInfo>& q) {
	while (!q.empty()) {
		q.pop();
	}
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
		std::cerr << "Failed to receive primitive data." << std::endl;
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

bool SendFileToServer(SOCKET sockfd, const std::wstring& srcFile) {// gui file
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


bool ReceiveFileFromServer(SOCKET sockfd, const std::wstring& destFile) {
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


void cloneFromServerMain(SOCKET serverSocket, SOCKET serverSocketFile) {
	while (true) {
		//std::unique_lock<std::mutex> lock(queueMutex);
		//Condition.wait(lock, [] { return update == true; }); // Đợi cho đến khi update = true;
		////	lock.unlock();
		//if (cloning == true ) continue;
		if (update == true) {
			std::unique_lock<std::mutex> lock(queueMutex);
			update = false;
			cloning = true;
			cloneFromServer(serverSocket, serverSocketFile);
			cloning = false;
			justupdate = true;
			lock.unlock();
			//update = false;
			Condition.notify_all();
		}
	}
}

void cloneFromServer(SOCKET serverSocket, SOCKET serverSocketFile) {// 1 luồng để clone
	//send n,d,l, N rev D, F, f (oneClientRequest.push(msgdata));

	//std::unique_lock<std::mutex> lock(queueMutex);
	//Condition.wait(lock, [] { return update==true;}); // Đợi cho đến khi update = true;
	dataInfo msgdata;
	cloning = true;
	std::map<std::wstring, std::time_t> tempMap(myMap);// tao 1 Map tam de xem file nao ma client vua xoa


	while (true) {
		if (oneClientRequest.empty() == false) {
			msgdata = oneClientRequest.front();
			oneClientRequest.pop();

			if (msgdata.msg == 'D') {
				cloning = false;
				std::cout << "break when meet D" << std::endl;
				break;
			}

			else if (msgdata.msg == 'F') { // Thư mục
				std::cerr << msgdata.msg << L" From Server " << msgdata.myId << std::endl;

				std::wstring serverDirectory = clientFolders + L"\\" + msgdata.directoryInfo;

				if (!CreateDirectory(serverDirectory.c_str(), NULL)) {
					if (GetLastError() == ERROR_ALREADY_EXISTS) {
						std::wcerr << L"Directory already exists: " << serverDirectory << std::endl;

					}
					else {
						std::wcerr << L"Failed to create directory on Client: " << serverDirectory << std::endl;
					}
				}
				else {
					std::wcerr << L"Successfully created directory: " << serverDirectory << std::endl;
					folderupdate = true;
					countupdate = 0;
					updateDataMap('A', msgdata.directoryInfo, msgdata.modifiedTime);//add

				}
				tempMap.erase(msgdata.directoryInfo);
			}
			else if (msgdata.msg == 'f') { // File
				std::wstring fullFilePath = clientFolders + L"\\" + msgdata.directoryInfo;

				if (!(myMap.count(msgdata.directoryInfo) > 0 && myMap.at(msgdata.directoryInfo) == msgdata.modifiedTime)) {

					dataInfo response;
					response.myId = myId;
					response.msg = 'n'; // next: tai file len di
					response.fileSize = 0;
					response.fileName = L"NULL";
					response.directoryInfo = L"NULL";
					response.modifiedTime = 0;

					sentDataFuncion(serverSocket, response);
					std::cerr << "Sent n to Client" << std::endl;

					ReceiveFileFromServer(serverSocketFile, fullFilePath);

					countupdate = 0;// nếu có file thì sẽ detect 2 lần, còn nếu ko có file thì detect 1 lần, reset về 1 để bình thường
					// nếu vừa update thì sẽ dectect 2 lần, lúc này ko gửi, lần 3 mới gửi
					fileupdate = true;

					response.msg = 'd'; // done
					sentDataFuncion(serverSocket, response);
					std::cerr << "Sent d to Server" << std::endl;
					updateDataMap('A', msgdata.directoryInfo, msgdata.modifiedTime);//add
				}
				else {
					dataInfo response;
					response.myId = myId;
					response.msg = 'l'; // lext: ko phai gui file len
					response.fileSize = 0;
					response.fileName = L"NULL";
					response.directoryInfo = L"NULL";
					response.modifiedTime = 0;

					sentDataFuncion(serverSocket, response);
					std::cerr << "Sent l to Client" << std::endl;
				}

				tempMap.erase(msgdata.directoryInfo);
			}

			dataInfo response;
			response.myId = myId;
			response.msg = 'N'; // next
			response.fileSize = 0;
			response.fileName = L"NULL";
			response.directoryInfo = L"NULL";
			response.modifiedTime = 0;

			sentDataFuncion(serverSocket, response);
			std::cerr << "Sent N to Server" << "with ID= " << response.myId << std::endl;
		}
	}
	for (auto it = tempMap.begin(); it != tempMap.end(); ) {
		std::wstring key = it->first;
		std::wstring key2 = clientFolders + L"\\" + key;

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
	std::cerr << "Done update from Server" << std::endl;
	writeMapToTxt(myMap);
	justupdate = true;
}
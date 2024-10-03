#include"Global.h"
#pragma comment(lib, "ws2_32.lib")  // Link with ws2_32.lib



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int init() {
	WSADATA wsaData;
	struct sockaddr_in serverAddr, serverAddrFile;
	char buffer[1024] = { 0 };

	// Khởi tạo Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed." << std::endl;
		return 1;
	}

	// Tạo socket cho dữ liệu thường
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed." << std::endl;
		WSACleanup();
		return 1;
	}

	// Tạo socket cho dữ liệu file
	serverSocketFile = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocketFile == INVALID_SOCKET) {
		std::cerr << "Socket File creation failed." << std::endl;
		closesocket(serverSocket); // Đóng socket đầu tiên
		WSACleanup();
		return 1;
	}

	// Cấu hình địa chỉ server cho dữ liệu thường (cổng 54000)
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(54000); // Cổng 54000
	inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr); // Kết nối tới localhost

	// Cấu hình địa chỉ server cho dữ liệu file (cổng 5000)
	serverAddrFile.sin_family = AF_INET;
	serverAddrFile.sin_port = htons(55000); // Cổng 5000
	inet_pton(AF_INET, ip.c_str(), &serverAddrFile.sin_addr); // Kết nối tới localhost

	// Kết nối tới server cho dữ liệu thường
	if (connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Connect failed." << std::endl;
		closesocket(serverSocket);
		closesocket(serverSocketFile); // Đóng cả hai socket
		WSACleanup();
		return 1;
	}

	// Kết nối tới server cho dữ liệu file
	if (connect(serverSocketFile, (struct sockaddr*)&serverAddrFile, sizeof(serverAddrFile)) == SOCKET_ERROR) {
		std::cerr << "Connect File failed." << std::endl;
		closesocket(serverSocket);
		closesocket(serverSocketFile); // Đóng cả hai socket
		WSACleanup();
		return 1;
	}

	std::cout << "da ket noi toi server!" << std::endl;

	// Gửi dữ liệu tới server thông qua socket thường
	dataInfo msgdata;
	msgdata.myId = myId;
	msgdata.msg = 'I';
	msgdata.fileSize = 0;
	msgdata.fileName = L"NULL";
	msgdata.directoryInfo = L"NULL";

	sentDataFuncion(serverSocket, msgdata);

	std::cout << "Data sent: " << msgdata.msg << " " << msgdata.myId << std::endl;

	std::cout << "End Init" << std::endl;

	return 0;
}



//clone ve cac client truoc

// sau do moi xu ly hang doi


void UpdateToServer(const std::wstring& clientFolder, SOCKET serverSocket, SOCKET serverSocketFile) {
	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile((clientFolder + L"\\*").c_str(), &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		std::wcerr << L"Error: Unable to access folder " << clientFolder << std::endl;
		return;
	}

	do {
		// Bỏ qua thư mục '.' và '..'
		if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0) {
			std::wstring srcFile = clientFolder + L"\\" + findFileData.cFileName;
			std::wstring relativeFile = srcFile.substr(clientFolders.length() + 1, srcFile.length());

			// Kiểm tra nếu là thư mục
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {

				dataInfo msgdata;
				msgdata.myId = myId;
				msgdata.msg = 'F';
				msgdata.fileSize = 0;
				msgdata.fileName = L"Folder";
				msgdata.directoryInfo = relativeFile;
				msgdata.modifiedTime = findFileData.ftLastWriteTime.dwLowDateTime;



				sentDataFuncion(serverSocket, msgdata);
				std::wcerr << msgdata.msg << " to server " << msgdata.directoryInfo << std::endl;
				serverIsHandling = true;


				if (receiveDataFuncion(serverSocket, msgdata)) {
					std::wcerr << "Receive " << msgdata.msg << "From Server" << std::endl;
					if (msgdata.msg != 'N') {
						break;
					}
				}
				else {
					std::wcerr << "Faile to Receiver from Server" << std::endl;
				}




				UpdateToServer(srcFile, serverSocket, serverSocketFile);
			}
			else {
				// Thông báo cho server về tệp và sau đó gửi tệp
				dataInfo msgdata;
				msgdata.myId = myId;
				msgdata.msg = 'f';
				msgdata.fileSize = 0;
				msgdata.fileName = L"Folder";
				msgdata.directoryInfo = relativeFile;
				msgdata.modifiedTime = findFileData.ftLastWriteTime.dwLowDateTime;

				sentDataFuncion(serverSocket, msgdata);
				std::wcerr << msgdata.msg << " to server " << msgdata.directoryInfo << std::endl;
				std::wstring fileInfo = srcFile;

				if (receiveDataFuncion(serverSocket, msgdata)) {// nhan 'n'
					std::wcerr << "Receive " << msgdata.msg << "From Server" << std::endl;
					//if (msgdata.msg != 'n') {// gửi file lên đi
					//	break;
					//}
					if (msgdata.msg == 'l') {
						goto label;
					}
					else if (msgdata.msg == 'n') {
						std::cerr << "nhan duoc 'n'" << std::endl;
					}
					else {
						break;
					}
				}
				else {
					std::wcerr << "Faile to Receiver from Server" << std::endl;
				}


				//send(serverSocket, reinterpret_cast<const char*>(fileInfo.c_str()), fileInfo.size() * sizeof(wchar_t), 0);

				if (!SendFileToServer(serverSocketFile, srcFile)) {
					std::wcerr << L"Failed to send file: " << srcFile << std::endl;
				}

				if (receiveDataFuncion(serverSocket, msgdata)) {
					std::wcerr << "Receive " << msgdata.msg << "From Server" << std::endl;
					if (msgdata.msg != 'd') {//nhận file xong
						break;
					}
				}
			label:
				if (receiveDataFuncion(serverSocket, msgdata)) {
					std::wcerr << "Receive " << msgdata.msg << "From Server" << std::endl;
					if (msgdata.msg != 'N') {
						break;
					}
				}
				else {
					std::wcerr << "Faile to Receiver from Server" << std::endl;
				}


			}
		}

	} while (FindNextFile(hFind, &findFileData) != 0);


	FindClose(hFind);

	std::cout << "-----------------" << std::endl;
	//clearQueue(oneServerRequest);

}

void Manage() {// luồng riêng
	while (true) {
		dataInfo msgdata;
		bool received = receiveDataFuncion(serverSocket, msgdata);
		if (received && msgdata.msg == 'R') {
			std::wstring temp = clientFolders;
			UpdateToServer(temp, serverSocket, serverSocketFile);
			dataInfo msgdata;
			msgdata.myId = myId;
			msgdata.msg = 'D';
			msgdata.fileSize = 0;
			msgdata.fileName = L"NULL";
			msgdata.directoryInfo = L"NULL";
			msgdata.modifiedTime = 0;

			if (!sentDataFuncion(serverSocket, msgdata)) {
				std::cerr << "Failed to send 'D' to server." << std::endl;
			}
			else {
				std::cerr << "Done Scanning File sent 'D' to Server" << std::endl;
			}
		}
		else if (received && msgdata.msg == 'U') {
			std::cerr << "Received U from server" << std::endl;
			cloning = true;
			update = true;// bat dau update
			//std::thread cloneThread(cloneFromServer, serverSocket, serverSocketFile);
			//cloneThread.join();
			Condition.notify_one();
		}
		else if (received && (msgdata.msg == 'F' || msgdata.msg == 'f' || msgdata.msg == 'D')) {
			if (msgdata.msg == 'D') {
				countupdate = 0;
			}
			oneClientRequest.push(msgdata);
			std::wcerr << "Push To Request Queue " << msgdata.msg << " From " << msgdata.myId << " " << msgdata.directoryInfo << std::endl;

		}
		else {
			std::cerr << "Khong biet gui gi :vvvvv" << std::endl;
		}

	}
}

// Cập nhật lên server
//UpdateToServer(clientFolders[i], SERVER_FOLDER);
//DeleteFileServer(clientFolders[i], SERVER_FOLDER);

// Cập nhật các client khác bằng cách gửi id của mình lên, server sẽ cập nhập các client khác

/////////////////////////update lên server
// gửi id của mình lên server để vào hàng đợi, đợi đến khi server hết bận và đến lượt mình thì sẽ thông báo đến mình
//lúc này mình làm việc với server, ở server sẽ có 1 hàm compare với ojb win32_find_data mình gửi lên, lúc này server xoá file ở server hoặc clone file thiếu từ client
// khi xong 1 luồng của server sẽ xử lý update về các client khác
// server rảnh khi đã xong quá trình update cho các máy khác online hoặc sẽ có 1 thread riêng phụ trách việc update cho máy khác

////////////////////////update về máy mình
//server sẽ gửi cho mình 1 request để update trên máy mình
//lúc này mình bị xoá file hoặc, thêm file từ server về
//trong lúc này thì tắt trạng thái detect thay đổi

/*sentDataStruct data;
data.myId = myId;*/
int changeDetect() {// luồng riêng
	std::unique_lock<std::mutex> lock(queueMutex);
	if (!cloning) {
		dwWaitStatus = WaitForSingleObject(Client, 0);
		if (dwWaitStatus == WAIT_OBJECT_0) {
			std::cout << "---------------------------------------------Thu muc da thay doi---------------------------------------------------";

			//if (cloning) return 0;

			countupdate++;
			dataInfo msgdata;
			msgdata.myId = myId;
			msgdata.msg = 'C';
			msgdata.fileSize = 0;
			msgdata.fileName = L"NULL";
			msgdata.directoryInfo = L"NULL";
			msgdata.modifiedTime = 0;



			//if (justupdate == false) {
			//	if (sentDataFuncion(serverSocket, msgdata)) {
			//		std::wcerr << " Sent " << msgdata.msg << " to Server" << std::endl;
			//	}
			//	else {
			//		std::wcerr << " false sent " << msgdata.msg << " to Server" << std::endl;
			//	}
			//}
			//else {
			//	justupdate = false;
			//}


			//if (justupdate==true) {// chi can co lenh U
			//	if (fileupdate==true) {
			//		if (countupdate == 3) {
			//			if (sentDataFuncion(serverSocket, msgdata)) {
			//				std::wcerr << " Sent " << msgdata.msg << " to Server" << std::endl;
			//			}
			//			else {
			//				std::wcerr << " false sent " << msgdata.msg << " to Server" << std::endl;
			//			}
			//			fileupdate = false;
			//		}
			//	}
			//	else {
			//		justupdate = false;
			//	}
			//}
			//else {
			//	if (sentDataFuncion(serverSocket, msgdata)) {
			//		std::wcerr << " Sent " << msgdata.msg << " to Server" << std::endl;
			//	}
			//	else {
			//		std::wcerr << " false sent " << msgdata.msg << " to Server" << std::endl;
			//	}
			//	//justupdate = false;
			//}

			if (folderupdate) {// thì thôi
				folderupdate = false;
			}
			else if (fileupdate) {
				if (countupdate == 2) {
					fileupdate = false;
				}
			}
			else {
				if (sentDataFuncion(serverSocket, msgdata)) {
					std::wcerr << " Sent " << msgdata.msg << " to Server" << std::endl;
				}
				else {
					std::wcerr << " false sent " << msgdata.msg << " to Server" << std::endl;
				}
			}



			if (!FindNextChangeNotification(Client)) {
				std::cerr << "Khong the theo doi thay doi thu muc " << std::endl;
				return 1;
			}
		}
	}
	lock.unlock();
	return 0;
}


int main() {
	makeDataMap(clientFolders);// tạo Map

	init();

	// Tạo một thread để xử lý các request từ server
	//std::thread serverThread(receiveFromServer, serverSocket);
	std::thread manageThread(Manage);
	std::thread cloneThread(cloneFromServerMain, serverSocket, serverSocketFile);
	//std::thread clone(cloneFromServer);

	// Phần còn lại của chương trình
	Client = FindFirstChangeNotification(
		clientFolders.c_str(),    // LPCWSTR
		FALSE,                      // Không theo dõi thư mục con
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_DIR_NAME // Theo dõi thay đổi tên file và nội dung file
	);

	if (Client == INVALID_HANDLE_VALUE) {
		std::cout << "Client " << " Error" << std::endl;
		return 1;
	}
	else {
		std::cout << "Client " << " Running" << std::endl;
	}

	while (true) {
		if (changeDetect()) std::cerr << "Khong the cap nhap" << std::endl;
			Sleep(500);
	}

	// Đợi thread xử lý request từ server kết thúc (thường là không bao giờ trong trường hợp này)
	manageThread.join();
	cloneThread.join();
	//clone.join();
	//serverThread.join();

	// Kết thúc theo dõi
	FindCloseChangeNotification(Client);

	return 0;
}

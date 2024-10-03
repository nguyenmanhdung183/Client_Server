#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <Windows.h>
#include "Global.h"
#include"CheckClient.h"


#pragma comment(lib, "Ws2_32.lib")

void clearQueue(std::queue<dataInfo>& q) {
    while (!q.empty()) {
        q.pop();
    }
}

void handleClient(SOCKET clientSocket, SOCKET clientSocketFile) {//xu ly data day len, luồng nhận msgdata
    char buffer[1024] = { 0 };
    int clientId = -1;

    while (true) {
        dataInfo datamsg;
        datamsg.myId = -1;
        receiveDataFuncion(clientSocket, datamsg);
        clientId = datamsg.myId;
       // std::cerr << "MAIN RECEIVE " << datamsg.msg << " From Client " << datamsg.myId << std::endl;
        if (clientId != -1 && datamsg.msg == 'C') {


            {
                std::lock_guard<std::mutex> lock(queueMutex);
                clientIdRequest.push(clientId); // Đẩy clientId vào queue
                queueCondition.notify_one(); // Thông báo cho luồng xử lý queue
                std::cout << "Push To Queue " << datamsg.msg << " From " << datamsg.myId << std::endl;
            }
            // Lưu socket của client
            
        }
        else if (clientId != -1 && datamsg.msg == 'I') {
            std::cerr << "Connected To Client " << clientId << std::endl;
            {
                if (cloneUpdate.find(datamsg.myId) == cloneUpdate.end()) {
                    cloneUpdate[datamsg.myId] = std::queue<dataInfo>();
                }
                std::lock_guard<std::mutex> lock(clientSocketsMutex);
                clientSockets[clientId] = clientSocket;
                clientSocketsFile[clientId] = clientSocketFile;
                //lock_guard.unlock();
                //initQueue.push(clientId);
            }


            std::thread firstUpdate(cloneFirstTime, clientId, clientSocket, clientSocketFile);
            firstUpdate.detach();

        }
        else if (clientId != -1 && (datamsg.msg == 'F' || datamsg.msg=='f' || datamsg.msg=='D')) {
            oneClientRequest.push(datamsg);
            std::wcerr << "Push To Request Queue " << datamsg.msg << " From " << datamsg.myId << " " <<datamsg.directoryInfo<< std::endl;
           
            //if (datamsg.msg == 'D') {
            //    done = true;
            //    clearQueue(oneClientRequest);
            //}
            printQueue(oneClientRequest);
        }   
        else if(clientId != -1 && (datamsg.msg =='n' || datamsg.msg == 'N' || datamsg.msg=='d' || datamsg.msg=='l')) {

            cloneUpdate.at(datamsg.myId).push(datamsg);
            std::wcerr << "Push To Request Queue " << datamsg.msg << " From " << datamsg.myId << " " << datamsg.directoryInfo << std::endl;
        }
        else {
            std::cerr << "Error Received from ID: " << datamsg.myId << " & msg: " << datamsg.msg << std::endl;
            int id=-2;
            //for (int i = 1; i <= numberOfClient; i++) {
            //    if (clientSockets[i] == clientSocket) {
            //        id = i;
            //        break;
            //    }
            //}
            std::cerr << "Error receiving data or client disconnected from client "<< id << std::endl;//// lỗi
            //clientOnline[clientId-1] = false;
            clientSockets.erase(id);
            clientSocketsFile.erase(id);
            return;// chưa tự động đóng luồng
        }
    }

    // Xóa socket của client khi ngắt kết nối
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex);
        clientSockets.erase(clientId);
    }

    // Đóng kết nối với client
    closesocket(clientSocket);
}

void processQueue() {// xu ly nhieu client
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        queueCondition.wait(lock, [] { return !clientIdRequest.empty() || busy; });// ko rỗng hoặc  bận

        if (busy) {
            continue; // Nếu server đang bận, bỏ qua xử lý
        }
        // Đặt trạng thái bận rộn
        busy = true;

        // Lấy yêu cầu từ queue
        int clientId = clientIdRequest.front();
        clientIdRequest.pop();
        lock.unlock();
        currentId = clientId;
        // Xử lý yêu cầu từ client
        std::cout << "Processing request from client ID: " << clientId << std::endl;

        // Tìm socket của client từ map
        SOCKET clientSocket;
        SOCKET clientSocketFile;
        {
            std::lock_guard<std::mutex> lock(clientSocketsMutex);
            auto it = clientSockets.find(clientId);
            auto it2 = clientSocketsFile.find(clientId);
            if (it == clientSockets.end()) {
                std::cerr << "Client socket not found for client ID: " << clientId << std::endl;
                busy = false;
                continue;
            }
            if (it2 == clientSocketsFile.end()) {
                std::cerr << "Client socket file not found for client ID: " << clientId << std::endl;
                busy = false;
                continue;
            }
            clientSocket = it->second;
            clientSocketFile = it2->second;
        }

        //tìm socket file của client từ map

        // Gửi yêu cầu đến client để gửi dữ liệu
        dataInfo msgdata;
        msgdata.myId = -1;
        msgdata.msg = 'R';
        msgdata.fileSize = 0;
        msgdata.fileName = L"NULL";
        msgdata.directoryInfo = L"NULL";

        if (!sentDataFuncion(clientSocket, msgdata)) {
            std::cerr << "Failed to send data to client." << std::endl;
        }
        else {
            std::cerr << "Sent to client: " << msgdata.msg << std::endl;
        }

        //xử lý data
        HandleClientUpdate(clientSocket, clientSocketFile);
        cloneToOtherClient(clientId);
        writeMapToTxt(myMap);
        currentId = -1;
        // Sau khi hoàn thành xử lý, đặt trạng thái không bận rộn
        busy = false;
        std::cerr << "Done Processing request from client " << clientId << std::endl;
    }
}

int init() {
    WSADATA wsaData;
    struct sockaddr_in serverAddr, clientAddr, serverAddrFile, clientAddrFile;
    int clientAddrSize = sizeof(clientAddr);
    int clientAddrFileSize = sizeof(clientAddrFile);
    char buffer[1024] = { 0 };
    // Khởi tạo Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    // Tạo socket cho dataInfo
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    // Tạo socket cho file
    serverSocketFile = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocketFile == INVALID_SOCKET) {
        std::cerr << "Socket File creation failed." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Cấu hình địa chỉ server cho dataInfo
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(54000); // Cổng 54000 cho dataInfo

    // Cấu hình địa chỉ server cho file
    serverAddrFile.sin_family = AF_INET;
    serverAddrFile.sin_addr.s_addr = INADDR_ANY;
    serverAddrFile.sin_port = htons(55000); // Cổng 55000 cho file

    // Liên kết socket với địa chỉ và cổng (dataInfo)
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed for dataInfo." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Liên kết socket với địa chỉ và cổng (file)
    if (bind(serverSocketFile, (struct sockaddr*)&serverAddrFile, sizeof(serverAddrFile)) == SOCKET_ERROR) {
        std::cerr << "Bind failed for File." << std::endl;
        closesocket(serverSocket);
        closesocket(serverSocketFile);
        WSACleanup();
        return 1;
    }

    // Lắng nghe kết nối (dataInfo)
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed for dataInfo." << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Lắng nghe kết nối (file)
    if (listen(serverSocketFile, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed for file." << std::endl;
        closesocket(serverSocketFile);
        WSACleanup();
        return 1;
    }
    makeDataMap(SERVER_FOLDER);// tạo Map

    std::cout << "Server is waiting for client connections..." << std::endl;

    std::vector<std::thread> threads;

    // Bắt đầu luồng xử lý queue
    std::thread queueThread(processQueue); // thread 1
    threads.push_back(std::move(queueThread));

    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        clientSocketFile = accept(serverSocketFile, (struct sockaddr*)&clientAddrFile, &clientAddrFileSize);
        if (clientSocket == INVALID_SOCKET || clientSocketFile == INVALID_SOCKET) {
            std::cerr << "Accept failed." << std::endl;
            closesocket(serverSocketFile);
            closesocket(serverSocket);
            WSACleanup();
            return 1;
        }
        std::cout << "Client connected." << std::endl;

        // Tạo một luồng mới để xử lý client
        threads.push_back(std::thread(handleClient, clientSocket, clientSocketFile));
    }

    // Đợi tất cả các luồng hoàn tất
    for (auto& thread : threads) {
        thread.join();
    }

    // Đóng socket và giải phóng Winsock
    closesocket(serverSocket);
    closesocket(serverSocketFile);
    WSACleanup();
    return 0;
}


int main() {
    init();
    return 0;
}
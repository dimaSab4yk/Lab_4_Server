#include <winsock2.h>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")
using namespace std;
using namespace std::chrono;

const int PORT = 8080;

void mirrorRow(vector<vector<int>>& matrix, int i, int n)
{
    for (int j = n / 2; j < n; ++j)
    {
        matrix[i][n - j - 1] = matrix[i][j];
    }
}

void mirrorMatrix(vector<vector<int>>& matrix, int n, int numThreads)
{
    vector<thread> threads;

    for (int i = 0; i < n; ++i)
    {
        threads.emplace_back(mirrorRow, ref(matrix), i, n);

        if (threads.size() >= numThreads)
        {
            for (auto& t : threads)
            {
                t.join();
            }
            threads.clear();
        }
    }

    for (auto& t : threads)
    {
        t.join();
    }
}

int recvInt(SOCKET sock)
{
    int netVal;
    recv(sock, (char*)&netVal, sizeof(netVal), 0);

    return ntohl(netVal);
}

void sendInt(SOCKET sock, int value)
{
    int netVal = htonl(value);
    send(sock, (char*)&netVal, sizeof(netVal), 0);
}

void sendDouble(SOCKET sock, double value)
{
    send(sock, (char*)&value, sizeof(value), 0);
}

void handleClient(SOCKET clientSocket)
{
    char command[16] = {};

    recv(clientSocket, command, sizeof(command), 0);

    if (strcmp(command, "HELLO") == 0)
    {
        cout << "Received: HELLO\n";
        const char* ack = "ACK_HELLO";
        send(clientSocket, ack, 16, 0);
    }

    recv(clientSocket, command, sizeof(command), 0);

    if (strcmp(command, "SEND_DATA") == 0)
    {
        cout << "Receiving data...\n";
        int n = recvInt(clientSocket);
        int numThreads = recvInt(clientSocket);

        vector<vector<int>> matrix(n, vector<int>(n));

        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                matrix[i][j] = recvInt(clientSocket);
            }                
        }            

        recv(clientSocket, command, sizeof(command), 0);

        if (strcmp(command, "START_COMPUTATION") == 0)
        {
            auto start = high_resolution_clock::now();
            mirrorMatrix(matrix, n, numThreads);
            auto end = high_resolution_clock::now();
            double seconds = duration_cast<nanoseconds>(end - start).count() * 1e-9;

            recv(clientSocket, command, sizeof(command), 0);

            if (strcmp(command, "GET_STATUS") == 0)
            {
                const char* status = "READY";
                send(clientSocket, status, 16, 0);
            }

            recv(clientSocket, command, sizeof(command), 0);

            if (strcmp(command, "GET_RESULT") == 0)
            {
                sendInt(clientSocket, (int)(seconds * 1e9));

                for (int i = 0; i < n; ++i)
                {
                    for (int j = 0; j < n; ++j)
                    {
                        sendInt(clientSocket, matrix[i][j]);
                    }
                }                            
            }
        }
    }

    closesocket(clientSocket);
    cout << "Client disconnected." << endl;
}


int main()
{
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (wsaResult != 0) 
    {
        cerr << "WSAStartup failed with error: " << wsaResult << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed!" << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Binding failed!" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        cerr << "Listening failed!" << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Server is waiting for clients..." << endl;

    while (true)
    {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET)
        {
            cerr << "Accept failed!" << endl;
            continue;
        }

        thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
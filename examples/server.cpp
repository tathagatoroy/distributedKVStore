#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <coroutine>
#include <iostream>
#include <string>
#include <thread>
#include <vector> // Required for storing client tasks

#include "Logger.h"

#pragma comment(lib, "Ws2_32.lib")

/*
How Coroutines Work Here:
- Task: The return type of our coroutine. It's a handle to the asynchronous operation.
- promise_type: A struct nested inside Task that defines the coroutine's behavior.
  - get_return_object(): Creates the Task object that is returned to the caller.
  - initial_suspend(): Decides if the coroutine runs immediately or suspends. `suspend_never` means it runs right away.
  - final_suspend(): Decides what happens when the coroutine finishes. `suspend_never` means it cleans up automatically.
  - unhandled_exception(): Catches any exceptions not handled within the coroutine.
*/

struct Task {
    struct promise_type {
        Task get_return_object() {
            LOG("Creating coroutine return object.");
            return Task { std::coroutine_handle<promise_type>::from_promise(*this) };
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> h;
    Task(std::coroutine_handle<promise_type> h) : h(h) {}
    ~Task() {
        if (h) {
            LOG("Destroying coroutine handle.");
            h.destroy();
        }
    }
    // Make Task movable but not copyable
    Task(Task&& other) noexcept : h(other.h) { other.h = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (h) h.destroy();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
};

// An "awaitable" struct for checking when a socket is ready to be read from.
struct socketReadAwaitable {
    SOCKET sock;
    std::coroutine_handle<> handleToResume;

    // This static function runs on a new thread to wait for the socket to be readable.
    static void waitThread(socketReadAwaitable self) {
        LOG("Starting I/O wait thread to check for readability.");
        fd_set rfds; // a fixed size buffer of bit arrays. 1 means that index is included
        FD_ZERO(&rfds); // set all to zero
        FD_SET(self.sock, &rfds); // set the bit corresponding self.sock to 1)

        // select() blocks until the socket has data to be read.
        // The first argument is ignored on Windows.
        if (select(0, &rfds, nullptr, nullptr, nullptr) > 0) {
            LOG("Socket is readable. Resuming coroutine.");
            self.handleToResume.resume();
        } else {
            LOG("select() failed or timed out.");
        }
        LOG("I/O wait thread finished.");
    }

    // await_ready: Called first. Returning false forces the coroutine to suspend.
    bool await_ready() noexcept {
        LOG("Checking if socket is ready to read (await_ready). Returning false to force suspension.");
        return false;
    }

    // await_suspend: Called if await_ready returns false. This is where the async operation is started.
    void await_suspend(std::coroutine_handle<> h) {
        LOG("Suspending coroutine (await_suspend). Creating a new thread for select().");
        handleToResume = h;
        std::thread(waitThread, *this).detach(); // Detach the thread to let it run independently.
    }

    // await_resume: Called when the coroutine is resumed.
    void await_resume() noexcept {
        LOG("Coroutine resumed (await_resume).");
    }
};

// Coroutine to handle a single client's connection.
Task handleClient(SOCKET clientSock) {
    LOG("New client handler coroutine started for socket: " + std::to_string(clientSock));
    char buffer[1024];
    while (true) {
        LOG("Waiting for data from client (co_await).");
        co_await socketReadAwaitable{ clientSock };
        LOG("Resumed. Now calling recv() to read data.");
        int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) {
            if (bytes == 0) {
                LOG("Client with socket " + std::to_string(clientSock) + " disconnected gracefully.");
                std::cout << "[SERVER]: Client with socket " << clientSock << " disconnected." << std::endl;
            } else {
                LOG("recv() failed for socket " + std::to_string(clientSock) + ". Error: " + std::to_string(WSAGetLastError()));
                std::cerr << "[SERVER]: recv failed for client " << clientSock << std::endl;
            }
            closesocket(clientSock);
            LOG("Socket closed. Terminating client handler coroutine.");
            co_return;
        }

        buffer[bytes] = '\0';
        std::string received_msg(buffer);
        LOG("Received " + std::to_string(bytes) + " bytes: " + received_msg);
        std::cout << "[SERVER]: Received from " << clientSock << " -> " << received_msg;
    }
}

// The main coroutine for the server.
Task serverMain() {
    LOG("Server main coroutine started.");
    
    // This vector will store the Task objects for each client.
    // This is the CRITICAL FIX: it prevents the coroutines from being destroyed prematurely.
    static std::vector<Task> client_tasks;

    WSADATA wsaData;
    LOG("Initializing Winsock.");
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    LOG("Creating listening socket.");
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all available network interfaces.
    serverAddr.sin_port = htons(5555);       // Listen on port 5555.

    LOG("Binding socket to port 5555.");
    bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    LOG("Putting socket in listening mode.");
    listen(listenSock, SOMAXCONN); // SOMAXCONN is a standard backlog size.

    std::cout << "[SERVER] listening on port 5555..." << std::endl;
    LOG("Server setup complete. Now accepting connections.");

    while (true) {
        // The accept() call is blocking. It will pause execution here until a client connects.
        SOCKET clientSock = accept(listenSock, nullptr, nullptr);
        if (clientSock != INVALID_SOCKET) {
            LOG("Accepted new connection. Client socket: " + std::to_string(clientSock));
            std::cout << "[SERVER]: New client connected with socket " << clientSock << std::endl;
            // Launch a new coroutine to handle this client and store its Task object.
            client_tasks.push_back(handleClient(clientSock));
        } else {
            LOG("accept() failed. Error: " + std::to_string(WSAGetLastError()));
        }
    }

    LOG("Closing listening socket.");
    closesocket(listenSock);
    LOG("Cleaning up Winsock.");
    WSACleanup();
    LOG("Server main coroutine finished.");
}

// Entry point
int main() {
    Logger::init("network_log.txt");
    LOG("Application starting.");
    auto s = serverMain();
    LOG("serverMain coroutine has been called. Entering infinite loop to keep process alive.");
    // This loop keeps the main thread alive so the server can continue accepting connections.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    return 0;
}
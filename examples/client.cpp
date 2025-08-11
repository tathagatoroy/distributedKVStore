#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <coroutine>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <future>

#include "Logger.h"

#pragma comment(lib, "Ws2_32.lib")

// Fixed Task/Promise structure
struct Task {
    struct promise_type {
        Task get_return_object() {
            LOG("Creating coroutine return object.");
            return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; } // FIX: Changed to suspend_always
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    // Move constructor
    Task(Task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    
    // Move assignment
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) {
                handle.destroy();
            }
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }
    
    // Delete copy operations
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    ~Task() {
        if (handle) {
            LOG("Destroying coroutine handle.");
            handle.destroy();
        }
    }
    
    // Method to check if coroutine is done
    bool done() const {
        return handle && handle.done();
    }
};

// socketWriteAwaitable struct (unchanged)
struct socketWriteAwaitable {
    SOCKET sock;
    std::coroutine_handle<> handleToResume;
    
    static void waitThread(socketWriteAwaitable self) {
        LOG("Starting I/O wait thread to check for writability.");
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(self.sock, &wfds);
        if (select(0, nullptr, &wfds, nullptr, nullptr) > 0) {
            LOG("Socket is writable. Resuming coroutine.");
            self.handleToResume.resume();
        } else {
            LOG("select() failed or timed out.");
        }
        LOG("I/O wait thread finished.");
    }
    
    bool await_ready() noexcept {
        LOG("Checking if socket is ready to write (await_ready). Returning false to force suspension.");
        return false;
    }
    
    void await_suspend(std::coroutine_handle<> h) {
        LOG("Suspending coroutine (await_suspend). Creating a new thread for select().");
        handleToResume = h;
        std::thread(waitThread, *this).detach();
    }
    
    void await_resume() noexcept {
        LOG("Coroutine resumed (await_resume).");
    }
};

Task clientMain(std::promise<void> completion_promise) {
    LOG("Client main coroutine started.");
    WSADATA wsaData;
    LOG("Initializing Winsock.");
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LOG("Socket creation failed.");
        closesocket(sock);
        WSACleanup();
        completion_promise.set_value();
        co_return;
    }

    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);

    LOG("Connecting to server 127.0.0.1:5555.");
    auto ret = connect(sock, (sockaddr*)&serv, sizeof(serv));
    if (ret == -1) {
        LOG("Connection failed. Error: " + std::to_string(WSAGetLastError()));
        std::cout << "[CLIENT] : connection refused " << std::endl;
        closesocket(sock);
        WSACleanup();
        completion_promise.set_value();
        co_return;
    }

    LOG("Connected to server successfully.");
    std::cout << "[CLIENT] : Connected To Server" << std::endl;

    for (int i = 0; i < 5; i++) {
        LOG("Loop " + std::to_string(i) + ": Waiting for socket to be writable (co_await).");
        co_await socketWriteAwaitable{ sock };
        LOG("Resumed. Now calling send().");

        std::string mes = "key " + std::to_string(i) + " = value " + std::to_string(i) + "\n";
        send(sock, mes.c_str(), mes.size(), 0);
        LOG("Sent: " + mes);
        std::cout << "[CLIENT] Sent : " << mes;

        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    
    LOG("Closing socket.");
    closesocket(sock);
    LOG("Cleaning up Winsock.");
    WSACleanup();
    LOG("Client main coroutine finished.");
    
    LOG("Calling completion promise");
    completion_promise.set_value();
}

int main() {
    std::ofstream ofs("network_log.txt", std::ofstream::trunc);
    ofs.close();
    
    Logger::init("network_log.txt");
    LOG("Application starting.");
    
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    auto c = clientMain(std::move(completion_promise));
    
    LOG("clientMain coroutine has been called. Main thread will now wait for completion signal.");

    completion_future.wait();
    
    LOG("Completion signal received. Waiting for coroutine to complete.");
    
    // FIX: Wait for coroutine to actually finish before destroying Task
    while (!c.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    LOG("Coroutine completed. Exiting application.");
    return 0;
}
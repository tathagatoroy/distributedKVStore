#ifndef NETWORK_H
#define NETWORK_H


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <queue>
#include <functional>
#include <vector>
#include <atomic>
#include <coroutine>
#include <cstring>


#include "utils.h"
#include "logger.h"


/*
some constants being defined
*/
constexpr uint32_t MAX_BUFFER_SIZE = 1024;

struct message {
    uint32_t messageSize;
    uint32_t fromNodeID;
    uint32_t toNodeID;
    char message[MAX_BUFFER_SIZE];
};
struct readData {
    struct promise_type {
        // creates the task object return to the caller
        readData get_return_object() {
            return readData { std::coroutine_handle<promise_type>::from_promise(*this)};

        }

        // intial suspend decideds if coroutine runs immediately or suspends.  `suspend_never` means it runs right away.
        std::suspend_never intial_suspend() noexcept {return {};}
        std::suspend_never final_suspend() noexcept {return {};}
        void return_void() {}
        void unhandled_exception() {std::terminate();}
    };
    std::coroutine_handle<promise_type> h;
    readData(std::coroutine_handle<promise_type> h) : h(h) {};
    ~readData() {
        if(h) {
            h.destroy();
        }
    }
    // move allowed not copy 
    readData(readData&& other) noexcept : h(other.h) { other.h = nullptr;}
    readData& operator=(readData&& other) noexcept {
        if(this != &other) {
            if(h) h.destroy();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }
    readData(const readData&) = delete;
    readData& operator=(const readData&) = delete;

    
};

struct socketReadAwaitable {
    SOCKET sock;
    std::coroutine_handle<> handleToResume;

    // This static function runs on a new thread to wait for the socket to be readable.
    static void waitThread(socketReadAwaitable self) {
        fd_set rfds; // a fixed size buffer of bit arrays. 1 means that index is included
        FD_ZERO(&rfds); // set all to zero
        FD_SET(self.sock, &rfds); // set the bit corresponding self.sock to 1)

        // select blocks till time interval for a file descriptor to be ready 
        if(select(0, &rfds, nullptr, nullptr, nullptr) > 0){
            auto formattedLog = Logger::
            LOG("Socket ")
        }

    }



}



#endif // NETWORK_H
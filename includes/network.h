
// file with classes which handles 
// messages to and from a server in async mode
#ifndef NETWORK_H
#define NETWORK_H


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#include<coroutine>
#include<system_error> // handles system error
#include<span>
#include<iostream>
#include<vector>
#include<thread>
#include<string>
#include<format>

// windows socket programming library 
#include<windows.h> // general windows api
#include<winsock2.h> // windows socket api 
#include<ws2tcpip.h> // additive to winsock, can handle ipv6
#include<mswsock.h> // microsoft specific extention to winsock

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

// forward declaration 
class ioService;
class tcpSocket;

// setup winsock 
class winSockSetter {
public:
	winSockSetter() {
		// initialises the winsock library. Populates the struct WSADATA with 
		// library specific details
		WSADATA wsaData;
		// resouce allocation and initialisations for network call
		// needed before API usage
		// 2,2 means version 2.2
		int result = WSAStartup(MAKEWORD(2,2), &wsaData);

		if(!result != 0) {
			throw std::runtime_error("WSAStartup Failed");
		}
	}
	~winSockSetter(){
		WSACleanup();
	}
}

// read result type 
struct readResult {
	std:: error_code error;
	size_t bytesRead;
};

// accept result type 
struct acceptResult {
	std::error_code error;
	std::unique_ptr<tcpSocket> nodeSocket;
};

enum class ioOpType {
	READ,
	WRITE,
	ACCEPT
};

// manages context of overalapped io reads and writes 
// in a network
// from https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
/*
typedef struct _OVERLAPPED {
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union {
    struct {
      DWORD Offset;
      DWORD OffsetHigh;
    } DUMMYSTRUCTNAME;
    PVOID Pointer;
  } DUMMYUNIONNAME;
  HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;
*/

struct ioContext{
	OVERLAPPED overlap; // windows data structure use to manage async io 
	// updates overlap when io is completed

	std::coroutine_handle<> continueation; // function which runs when waiting for io
	ioOpType operationType;

	// for read/write 
	std::span<char> buffer;
	size_t bytesTransfered;

	//for accept operation
	SOCKET socket; // stores the client socket address
	char acceptBuffer[1024]; // buffer for accept address info

	DWORD lastError; // last error code from the io 

	ioContext(ioOpType ioType) : overlapped{} , // default initialisation of all struct fields
								 continueation {}, // null handle
								 operationType(ioType), 
								 buffer{}, // 0 sized memory location nullptr
								 bytesTransfered(0),  
								 socket(INVALID_SOCKET),
								 acceptBuffer{}, // char array of 0
								 lastError(0) {}




};


class tcpSocket {
private:
	SOCKET socketHandle_;
	ioService& ioService_; // a& is a reference not a pointer
	// .variable is allowed. object must exist and cannot be null.
	// no reassignment is possible
	// no pointer arithmetic allowed.
	// c++ practice. use reference whenever I can
public:
	tcpSocket(ioService& ioService_, SOCKET socket = INVALID_SOCKET);
	~tcpSocket();

	// non copyable and non movable
	// system resources like sockets, fileSystems should be non copyable
	tcpSocket(const tcpSocket&) = delete; // copy constructor deleted
	tcpSocket& operator=(const tcpSocket&) = delete; // copy assignment delete
	//gurantees no exception in case of movement

	tcpSocket(tcpSocket&& other) noexcept; // move constructor
	tcpSocket& operator=(tcpSocket&& other) noexcept; // move assignment


	SOCKET nativeHandle() const { return socketHandle_; }
	ioService getIOService() {return ioService_;}


	bool bindAndListen(const std::string& address, int port);
	void close();


};

// I/O Completion Port service
class ioService {
private:
	HANDLE iocpHandle_ ; // handle to a I/O completion Port
	// HANDLE is defined in windows.h
	std::std::vector<std::thread> workerThreads_;
	bool running_;
public:
	ioService(size_t threadCount = std::thread::hardware_concurrency()) : iocpHandle_(INVALID_HANDLE_VALUE), running_(false) {

	}
}
#endif // NETWORK_H
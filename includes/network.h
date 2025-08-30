
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
	SOCKET acceptSocket; // stores the client socket address
	char acceptBuffer[1024]; // buffer for accept address info

	DWORD lastError; // last error code from the io 

	ioContext(ioOpType ioType) : overlap{} , // default initialisation of all struct fields
								 continueation {}, // null handle
								 operationType(ioType), 
								 buffer{}, // 0 sized memory location nullptr
								 bytesTransfered(0),  
								 acceptSocket(INVALID_SOCKET),
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
// creat I/O completion handle and the associated threadpool
// associate the iocp handle with a socket
// reads data using WSARecv (allowing for overlapped sockets)
// and notification of IOCP port
class ioService {
private:
	HANDLE iocpHandle_ ; // handle to a I/O completion Port
	// HANDLE is defined in windows.h
	std::std::vector<std::thread> workerThreads_;
	bool running_;
public:
	ioService(size_t threadCount = std::thread::hardware_concurrency()) : iocpHandle_(INVALID_HANDLE_VALUE), running_(false) {

	// HANDLE WINAPI CreateIoCompletionPort(
	//   _In_     HANDLE    FileHandle,
	//   _In_opt_ HANDLE    ExistingCompletionPort,
	//   _In_     ULONG_PTR CompletionKey,
	//   _In_     DWORD     NumberOfConcurrentThreads If this parameter is zero, the system allows as many concurrently running threads as there are processors in the system.
	// );
		// does create and/or associate(if existing port is passed instead of INVALID)
		// windows mechanism to implement non-blocking async IO
		//When you initiate an overlapped I/O operation, the hardware DMA controller takes over the actual data transfer, freeing the CPU to do other work.
		iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)
		if(iocpHandle_ == NULL) throw std::runtime_error("Failed to create IOCP");
		start_workers(threadCount);


	}
	~ioService(){
		stop();
	}
	void associateSocket(SOCKET socket) {
		HANDLE result = CreateIoCompletionPort((HANDLE) socket, iocpHandle_, (ULONG_PTR) socket, 0);
		if(result == NULL) throw std::runtime_error("Failed to associate socket with IOCP");

	}
	void postSend(SOCKET socket, ioContext* context, char *sendBuffer, ULONG bufferLen){
		DWORD flags = 0;
		WSABUF wsaBuf;
		wsaBuf.buf = sendBuffer;
		wsaBuf.len = bufferLen;
		// 
		/*
		int WSAAPI WSASend(
		  [in]  SOCKET                             s,
		  [in]  LPWSABUF                           lpBuffers,
		  [in]  DWORD                              dwBufferCount,
		  [out] LPDWORD                            lpNumberOfBytesSent,
		  [in]  DWORD                              dwFlags,
		  [in]  LPWSAOVERLAPPED                    lpOverlapped,
		  [in]  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
			);
		*/
		// LPWORD is null in case of overlapped io 
		// completion routine is null as we are using IOCP and not callbacks
		int result = WSASend(socket, &wsaBuf, 1, nullptr, &context->overlap, nullptr);
		if(result == SOCKET_ERROR){
			DWORD error = WSAGetLastError();
			if(error != WSA_IO_PENDING){
				context->lastError = error;
				context->bytesTransfered = 0;
				// send manually failure request
				PostQueuedCompletionStatus(iocpHandle_,0, (ULONG_PTR) socket, &context->overlap);
			}
		}
	}
	void postRead(SOCKET socket, ioContext* context) {
		DWORD flags = 0;
		WSABUF wsaBuf;
		wsaBuf.buf = context->buffer.data();
		wsaBuf.len = static_cast<ULONG> (context->buffer.size());
		// int WSAAPI WSARecv(
		//   [in]      SOCKET                             s,
		//   [in, out] LPWSABUF                           lpBuffers,
		//   [in]      DWORD                              dwBufferCount,
		//   [out]     LPDWORD                            lpNumberOfBytesRecvd,
		//   [in, out] LPDWORD                            lpFlags,
		//   [in]      LPWSAOVERLAPPED                    lpOverlapped,
		//   [in]      LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
		// );
		// &wsaBuf -> array of wsaBuf
		// number of &wsaBuf 
		//A pointer to flags used to modify the behavior of the WSARecv function call. For more information, see the Remarks section.

		int result = WSARecv(socket, &wsaBuf, 1, nullptr, &flags, &context->overlap, nullptr);
		if(result == SOCKET_ERROR){
			DWORD error = WSAGetLastError();
			if(error != WSA_IO_PENDING) {
				context->lastError = error;
				context->bytesTransfered = 0;
				// BOOL WINAPI PostQueuedCompletionStatus(
				//   _In_     HANDLE       CompletionPort,
				//   _In_     DWORD        dwNumberOfBytesTransferred,
				//   _In_     ULONG_PTR    dwCompletionKey,
				//   _In_opt_ LPOVERLAPPED lpOverlapped
				// );
				// in case there is actual failure
				// worker threads doesn't recieve completion event notification
				// so failure is lost 
				// manually tell the post read failed
				PostQueuedCompletionStatus(iocpHandle_,0, (ULONG_PTR)socket, &context->overlap);

			}
		}
	}

	void postAccept(SOCKET listenSocket, ioContext* context){
		context->acceptSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		// AF_INET -> IPV4
		// SOCK_STREAM -> IPV4 + TCP
		// IPPROTO_TCP -> TCP

		if(context->acceptSocket== INVALID_SOCKET){
			context = lastError = WSAGetLastError();
			PostQueuedCompletionStatus(iocpHandle_, 0, (ULONG_PTR)listenSocket, &context->overlap)
			return;
		}
		DWORD bytesReceived;
		// BOOL AcceptEx(
		//     SOCKET sListenSocket : a socket bound to a local address and listening to a connection
		//     SOCKET sAcceptSocket : precreated unbounded socket which will connect to the client
		//     PVOID lpOutputBuffer :A buffer that serves dual purposes:Receives the first chunk of data sent by the client (if any. Stores local and remote address information
		//     DWORD dwReceiveDataLength : 0 means accept the connection dont' wait for data 
		//     DWORD dwLocalAddressLength : pointer to the address info + padding
		//     DWORD dwRemoteAddressLength,
		//     LPDWORD lpdwBytesReceived : actual number of bytes recieved
		//     LPOVERLAPPED lpOverlapped
		//     );
		BOOL result = AcceptEx(listenSocket, context->acceptSocket, context->acceptBuffer, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytesReceived, &context->overlap);
		if(!result){
			DWORD error = WSAGetLastError();
			if(error != WSA_IO_PENDING){
				context->lastError = error;
				closesocket(context->acceptSocket);
				context->acceptSocket = INVALID_SOCKET;
				PostQueuedCompletionStatus(iocpHandle_, 0, (ULONG_PTR)listenSocket, &context->overlap);

			}
		}

	}
	void run(){
		// + 1 as main thread is also counted
		std::cout<< "ioService running with " <<workerThreads_.size() + 1 << std::endl;
		workerLoop();

	}
	void stop() {
		running_ = false;
		for(size_t i = 0; i < workerThreads_.size(); i++) PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
		for(auto& thread :: workerThreads_){
			if(thread.joinable()) {
				thread.join();
			}
		}

		if (iocpHandle_ != INVALID_HANDLE_VALUE) CloseHandle(iocpHandle_);

	}
private:
	void start_workers(size_t threadCount){
		running_ = true ;
		for(size_t i = 0; i < threadCount - 1; i++) {// main is also working
			workerThreads_.emplace_back([this,i]() {
				std::cout <<" worker Thread " << i << "started" << std::endl;
				workerLoop();
			});
		}
	}

	void workerLoop() {
		while(running_){
			DWORD bytesTransfered;
			ULONG_PTR completionKey;
			OVERLAPPED* overlap;
			// TODO use  GetQueuedCompletionStatusEx
			// 1000 is timeout. Returns false after that
			BOOL result = GetQueuedCompletionStatus(iocpHandle_, bytesReceived, &completionKey, &overlap, 1000);
			if(!result){
				DWORD error = GetLastError(); // threads last error 
				if(error == WAIT_TIMEOUT) continue; // restart wait when timeout
				if(overlap == nullptr) break; // stop sends a postCompletionRequest with overlap nullptr ending the workerloop
			}
			if(overlap) {
				ioContext* context = reinterpret_cast<ioContext*>overlap; // this is fine as the first member variable of context is overlap. Doesn't affect other variables.
				context->bytesTransfered = bytesTransfered;
				context->lastError = result ? 0: GetLastError();
				completionReady(context);
			}
		}
	}

	void completionReady(ioContext* context){
		if(context->continueation) context->continueation.resume();
		
	}
};


// tcp socket implementation
// create a socket if already not a socket
// connect socket with IOCP if already not bind
// connect IOCP class with the socket
tcpSocket::tcpSocket(ioService& ioService_, SOCKET socket)
	: socketHandle_(socket), ioService_(ioService_) {
		if(socketHandle_ == INVALID_SOCKET) {
			// create a new IPV TCP socket 
			socketHandle_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(socketHandle_ == INVALID_SOCKET) throw std::runtime_error("Failed to create socket");
			// associate socket with IOCP
			if(socketHandle_ != INVALID_SOCKET) ioService_.associateSocket(socketHandle_);

		}

	}

tcpSocket::~tcpSocket() {
	close();
}

// copy is not allowed on resource. Move is allowed
// move constructor
tcpSocket::tcpSocket(tcpSocket&& other) noexcept 
	: socketHandle_(std::exchange(other.socketHandle_, INVALID_SOCKET)),
		ioService_(other.ioService_) {}

tcpSocket& tcpSocket::operator=(tcpSocket&& other) noexcept {
	if(this != other) {
		close();
		socketHandle_ = std::exchange(other.socketHandle_, INVALID_SOCKET);

	}
	return this*;
}

bool tcpSocket::bindAndListen(const std::string& address, int port) {
	sockaddr_in addr {} // empty intialise sockaddr_in structure 
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	// convert address IP to byte encoding addr
	inet_pton(AF_INET, address.c_str(), &addr.sin_addr);
	if(bind(socketHandle_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		// bind error 
		std::cout<<"Error in binding for ADDRESS " << address <<" PORT : " << port << std::endl;
		return false;
	}
	if(listen(socketHandle_, SOMAXCONN) == SOCKET_ERROR) {
		// server maintains a list of queue for incoming connection requestion. 
		// somaxcon controls size of the queue
		std::cout<< "Error in listening to socket for ADDRESS :" << address <<" PORT : " << port << std::endl;
		return false;
	}
	return true;
}

void tcpSocket::close() {
	if(socketHandle_ != INVALID_SOCKET) _{
		closesocket(socketHandle_);
		socketHandle_ = INVALID_SOCKET;
	}
}

// awaitable async read
// contains tcp socket , buffer and context 
class readAwaitable{
	tcpSocket& socket_;
	std::span<char> buffer_;
	std::unique_ptr<ioContext> context_;

public:
	readAwaitable(tcpSocket& socket, std::span<char> buffer)
		: socket_(socket),
		: buffer_(buffer),
		context_(std::make_unique<ioContext>(ioType::READ)) {
			context_->buffer = buffer_;
		}

		// this essentially calls await suspend to pass back suspend this coroutine 
		// and pass back the control to the caller
		bool await_ready() const noexcept { return false;}
		bool await_suspend(std::coroutine_handle<> continueation) {
			context_->continueation = continueation;
			// get io service return iocp service 
			// post read submits read request 
			// native_handle returns the socket 
			// .get() raw pointer underneath unique pointer 
			socket_.getIOService().postRead(socket_.nativeHandle(), context_.get());


		}

	readResult await_resume() const noexcept {
		readResult result;
		// result buffer already set
		if(context_->lastError == 0) {
			result.error = std::error_code{};
			// no error 
			result.bytesRead = context_->bytesTransfered;

		}
		else {
			result.error = std::error_code(context_->lastError, std::system_category());
			result.bytesRead = 0;
		}

		
	}

class AcceptAwaitable {
private:
	tcpSocket& listenSocket_;
	std::unique_ptr<ioContext> context_;

public:
	AcceptAwaitable(tcpSocket& socket) 
		: listenSocket_(socket),
			context_(std::make_unique<ioContext>(ioType::ACCEPT)) {}

	bool await_ready() const noexcept {return false;}
	bool await_suspend(std::coroutine_handle<> continueation) {
		context.continueation = continueation;
		socket_.getIOService().postAccept(listenSocket_.nativeHandle(), context_.get());

	}
	acceptResult await_resume() {
		acceptResult result;
		if(context_.lastError == 0 && context_->acceptSocket != INVALID_SOCKET) {
			result.error = std:: error_code{};
			result.nodeSocket = std::make_unique<tcpSocket>(listenSocket_.getIOService(), context_->acceptSocket);
			// transfer ownership
			context_->acceptSocket = INVALID_SOCKET;
		}
		else {
			result.error = std::error_code(context_->lastError, std::system_category());
			result.nodeSocket = nullptr;

		}
	}


}

// async functions that can be used sequentially 
readResult asyncRead(tcpSocket& socket, std::span<char> buffer) {
	return readAwaitable(socket, buffer);

}

acceptResult asyncAccept(tcpSocket& listenSocket) {
	return AcceptAwaitable(listenSocket);
}

#endif // NETWORK_H
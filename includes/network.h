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
#include<atomic> // Claude suggestion: Added for thread-safe running_ flag
#include<memory> // Claude suggestion: Added for smart pointer management
#include<utility> // Claude suggestion: Added for std::exchange

// windows socket programming library 
#include<winsock2.h> // windows socket api 
#include<windows.h> // general windows api
#include<ws2tcpip.h> // additive to winsock, can handle ipv6
#include<mswsock.h> // microsoft specific extention to winsock

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

// forward declaration 
class ioService;
class tcpSocket;

// Centralized error handling for network operations
class NetworkError : public std::system_error {
public:
    NetworkError(int error_code, const std::string& message) 
        : std::system_error(error_code, std::system_category(), message) {}
    
    NetworkError(std::error_code ec, const std::string& message)
        : std::system_error(ec, message) {}
};

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

		if(result != 0) { 
			throw NetworkError(result, "WSAStartup Failed"); // Using NetworkError for consistency
		}
	}
	~winSockSetter(){
		WSACleanup();
	}
}; 
// RAII wrapper for SOCKET handles to ensure proper cleanup
class SocketHandle {
private:
    SOCKET socket_;
    
public:
    SocketHandle(SOCKET s = INVALID_SOCKET) : socket_(s) {}
    
    ~SocketHandle() {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
    }
    
    // Non-copyable but movable
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    
    SocketHandle(SocketHandle&& other) noexcept 
        : socket_(std::exchange(other.socket_, INVALID_SOCKET)) {}
    
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            if (socket_ != INVALID_SOCKET) {
                closesocket(socket_);
            }
            socket_ = std::exchange(other.socket_, INVALID_SOCKET);
        }
        return *this;
    }
    
    SOCKET get() const { return socket_; }
    SOCKET release() { return std::exchange(socket_, INVALID_SOCKET); }
    void reset(SOCKET s = INVALID_SOCKET) {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
        }
        socket_ = s;
    }
    
    operator bool() const { return socket_ != INVALID_SOCKET; }
};

// read result type 
struct readResult {
	std::error_code error;
	size_t bytesRead;
};

//writeResult - Claude suggestion: Fixed typo in struct name
struct writeResult {
	std::error_code error;
	size_t bytesWritten;
}; // Claude suggestion: Added missing semicolon

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

struct ioContext{
	OVERLAPPED overlap; // windows data structure use to manage async io 
	// updates overlap when io is completed

	std::coroutine_handle<> continuation; // Claude suggestion: Fixed typo - was 'continueation'
	ioOpType operationType;

	// for read/write 
	std::span<char> buffer;
	size_t bytesTransfered;

	//for accept operation
	SOCKET acceptSocket; // stores the client socket address
	char acceptBuffer[1024]; // buffer for accept address info

	DWORD lastError; // last error code from the io 

	ioContext(ioOpType ioType) : overlap{}, // default initialisation of all struct fields
								 continuation{}, // null handle
								 operationType(ioType), 
								 buffer{}, // 0 sized memory location nullptr
								 bytesTransfered(0),  
								 acceptSocket(INVALID_SOCKET),
								 acceptBuffer{}, // char array of 0
								 lastError(0) {}
};

// coroutine promise type for async operations
struct AsyncTask {
    struct promise_type {
        AsyncTask get_return_object() {
            return AsyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_void() {}
        
        void unhandled_exception() {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                std::cerr << "Unhandled exception in coroutine: " << e.what() << std::endl;
            }
        }
    };
    
    using handle_type = std::coroutine_handle<promise_type>;
    
    AsyncTask(handle_type h) : handle_(h) {}
    ~AsyncTask() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // Move only
    AsyncTask(AsyncTask&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    AsyncTask& operator=(AsyncTask&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    AsyncTask(const AsyncTask&) = delete;
    AsyncTask& operator=(const AsyncTask&) = delete;
    
private:
    handle_type handle_;
};


class tcpSocket {
private:
	SocketHandle socketHandle_;
	ioService& ioService_; // a& is a reference not a pointer
	// .variable is allowed. object must exist and cannot be null.
	// no reassignment is possible
	// no pointer arithmetic allowed.
	// c++ practice. use reference whenever I can
public:
  // Constructor
	// tcp socket implementation
	// create a socket if already not a socket
	// connect socket with IOCP if already not bind
	// connect IOCP class with the socket
 	tcpSocket(ioService& ioService, SOCKET socket = INVALID_SOCKET)
      : socketHandle_(socket), ioService_(ioService) {
      if (!socketHandle_) {
          SOCKET newSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED); // preferred to socket because of overlap
;
      if (newSocket == INVALID_SOCKET) {
      	throw NetworkError(WSAGetLastError(), "Failed to create socket");
          
      }
      socketHandle_.reset(newSocket);
      ioService_.associateSocket(socketHandle_.get());
  }	
  ~tcpSocket() = default; // the RAII class handles it.


	// non copyable bit  movable
	// system resources like sockets, fileSystems should be non copyable
	tcpSocket(const tcpSocket&) = delete; // copy constructor deleted
	tcpSocket& operator=(const tcpSocket&) = delete; // copy assignment delete
	//gurantees no exception in case of movement

  tcpSocket(tcpSocket&& other) noexcept = default;
  tcpSocket& operator=(tcpSocket&& other) noexcept = default;


  SOCKET nativeHandle() const { return socketHandle_.get(); }
	ioService& getIOService() {return ioService_;} // return type - returns reference not value


	bool bindAndListen(const std::string& address, int port);
  void close() { socketHandle_.reset(); } // Or rely on destructor
};


// I/O Completion Port service
// creat I/O completion handle and the associated threadpool
// associate the iocp handle with a socket
// reads data using WSARecv (allowing for overlapped sockets)
// and notification of IOCP port
//TODO 
// 1.current each await type creates a new context. which is not efficient. 
// Instead create a pool of context with mutexes and reuse with each call.
class ioService {
private:
	HANDLE iocpHandle_ ; // handle to a I/O completion Port
	// HANDLE is defined in windows.h
	std::vector<std::thread> workerThreads_; // Claude suggestion: Fixed double std:: namespace
	std::atomic<bool> running_; // Made atomic for thread safety
public:
	ioService(size_t threadCount = std::thread::hardware_concurrency()) : iocpHandle_(INVALID_HANDLE_VALUE), running_(false) {

		// does create and/or associate(if existing port is passed instead of INVALID)
		// windows mechanism to implement non-blocking async IO
		//When you initiate an overlapped I/O operation, the hardware DMA controller takes over the actual data transfer, freeing the CPU to do other work.
		// sometimes hardware_concurrency can be zero 
		if (threadCount == 0) threadCount = 1;

		iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); 
		if(iocpHandle_ == NULL) throw NetworkError(GetLastError(), "Failed to create IOCP"); // Using NetworkError with system error
		start_workers(threadCount);
	}
	~ioService(){
		stop();
	}
	void associateSocket(SOCKET socket) {
		HANDLE result = CreateIoCompletionPort((HANDLE) socket, iocpHandle_, (ULONG_PTR) socket, 0);
		if(result == NULL) throw NetworkError(GetLastError(), "Failed to associate socket with IOCP");
	}
	void postWrite(SOCKET socket, ioContext* context, char *sendBuffer, ULONG bufferLen){
		DWORD flags = 0;
		WSABUF wsaBuf;
		wsaBuf.buf = sendBuffer;
		wsaBuf.len = bufferLen;

		// completion routine is null as we are using IOCP and not callbacks
		int result = WSASend(socket, &wsaBuf, 1, nullptr, flags, &context->overlap, nullptr); 
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

		int result = WSARecv(socket, &wsaBuf, 1, nullptr, &flags, &context->overlap, nullptr);
		if(result == SOCKET_ERROR){
			DWORD error = WSAGetLastError();
			if(error != WSA_IO_PENDING) {
				context->lastError = error;
				context->bytesTransfered = 0;
				// in case there is actual failure
				// worker threads doesn't recieve completion event notification
				// so failure is lost 
				// manually tell the post read failed
				PostQueuedCompletionStatus(iocpHandle_,0, (ULONG_PTR)socket, &context->overlap);

			}
		}
	}

	void postAccept(SOCKET listenSocket, ioContext* context){
		context->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
		// AF_INET -> IPV4
		// SOCK_STREAM -> IPV4 + TCP
		// IPPROTO_TCP -> TCP

		if(context->acceptSocket== INVALID_SOCKET){
			context->lastError = WSAGetLastError(); 
			PostQueuedCompletionStatus(iocpHandle_, 0, (ULONG_PTR)listenSocket, &context->overlap); 
			return;
		}
		DWORD bytesReceived = 0;

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
		std::cout<< "ioService running with " <<workerThreads_.size() << " threads" << std::endl;
		// workerLoop();

	}
	void stop() {
		running_ = false;
		// Wake up all worker threads to check running_ flag
		for(size_t i = 0; i < workerThreads_.size(); i++) PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
		for(auto& thread : workerThreads_){ 
			if(thread.joinable()) {
				thread.join();
			}
		}

		if (iocpHandle_ != INVALID_HANDLE_VALUE) CloseHandle(iocpHandle_);

	}
private:
	void start_workers(size_t threadCount){
		running_ = true ;
		for(size_t i = 0; i < threadCount; i++) {// main is also working
			workerThreads_.emplace_back([this,i]() {
				std::cout <<" worker Thread " << i << " started" << std::endl; 
				workerLoop();
			});
		}
	}

	void workerLoop() {
		while(running_){
			DWORD bytesTransfered; // Claude suggestion: Fixed variable name - was bytesReceived
			ULONG_PTR completionKey;
			OVERLAPPED* overlap;
			// TODO use  GetQueuedCompletionStatusEx
			// 1000 is timeout. Returns false after that
			BOOL result = GetQueuedCompletionStatus(iocpHandle_, &bytesTransfered, &completionKey, &overlap, 1000); // Claude suggestion: Fixed variable name
			if(!result){
				DWORD error = GetLastError(); // threads last error 
				if(error == WAIT_TIMEOUT) continue; // restart wait when timeout
				if(overlap == nullptr) break; // stop sends a postCompletionRequest with overlap nullptr ending the workerloop
			}
			if(overlap) {
				// note this overlap is populated by the same address as the overlap 
				// struct which was called when calling WSASEND/REC
				// hence the memory space has the same struct alignment 
				// &context = &overlap as this overlap is the first member of the context
				// so this context points to the same context.
				// which contains socket.
				ioContext* context = reinterpret_cast<ioContext*>(overlap); 
				context->bytesTransfered = bytesTransfered;
				context->lastError = result ? 0: GetLastError();
				completionReady(context);
			}
		}
	}

	void completionReady(ioContext* context){
		if(context->continuation) context->continuation.resume(); 
	}
};







bool tcpSocket::bindAndListen(const std::string& address, int port) {
    // ReuseAddr 
		// otherwise OS will block the address for a while
    BOOL opt = TRUE;
    setsockopt(socketHandle_.get(), SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{}; 
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
        throw NetworkError(WSAEINVAL, std::format("Invalid IPv4 address: {}", address));
    }
    if (bind(socketHandle_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        throw NetworkError(WSAGetLastError(), std::format("bind failed on {}:{}", address, port));
    }
    if (listen(socketHandle_.get(), SOMAXCONN) == SOCKET_ERROR) {
        throw NetworkError(WSAGetLastError(), std::format("listen failed on {}:{}", address, port));
    }
    return true;
}



// awaitable async write 
// contains tcp socket, buffer and context 
class writeAwaitable{
	tcpSocket& socket_;
	std::span<char> buffer_;
	std::unique_ptr<ioContext> context_;
public:
	writeAwaitable(tcpSocket& socket, std::span<char> buffer)
		: socket_(socket),
		  buffer_(buffer), 
		  context_(std::make_unique<ioContext>(ioOpType::WRITE)) {
		  // make this const so we dont modify 
			context_->buffer = std::span<char>(const_cast<char*>(buffer.data()),buffer.size());
		}
	// await_ready checks if it is already written so no need to suspend. But that is not the case 
	bool await_ready() const noexcept {return false;}
	// if await_ready returns false, we call await_suspend which prepares the coroutine to suspend
	void await_suspend(std::coroutine_handle<> continuation) { 
		context_->continuation = continuation; 
		char* buffer = context_->buffer.data();
		ULONG bufferLen = static_cast<ULONG>(context_->buffer.size());

		socket_.getIOService().postWrite(socket_.nativeHandle(), context_.get(), buffer, bufferLen); 
		// await_suspend returns void for fire-and-forget semantics
	}
	writeResult await_resume() const noexcept {
		writeResult result;
		if(context_->lastError == 0) { 
			result.error = std::error_code{}; 
			result.bytesWritten = context_->bytesTransfered; 
		}
		else {
			result.error = std::error_code(context_->lastError, std::system_category());
			result.bytesWritten = 0; 
		}
		return result;
	}
}; 

// awaitable async read
// contains tcp socket , buffer and context 
class readAwaitable{
	tcpSocket& socket_;
	std::span<char> buffer_;
	std::unique_ptr<ioContext> context_;

public:
	readAwaitable(tcpSocket& socket, std::span<char> buffer)
		: socket_(socket),
		  buffer_(buffer), 
		  context_(std::make_unique<ioContext>(ioOpType::READ)) { 
			context_->buffer = buffer_;
		}

		// this essentially calls await suspend to pass back suspend this coroutine 
		// and pass back the control to the caller
		bool await_ready() const noexcept { return false;}
		void await_suspend(std::coroutine_handle<> continuation) {
			context_->continuation = continuation; // Claude suggestion: Fixed typo
			// get io service return iocp service 
			// post read submits read request 
			// native_handle returns the socket 
			// .get() raw pointer underneath unique pointer 
			socket_.getIOService().postRead(socket_.nativeHandle(), context_.get());
			// await_suspend returns void for fire-and-forget semantics
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
		return result;
	}
}; 

class AcceptAwaitable {
private:
	tcpSocket& listenSocket_;
	std::unique_ptr<ioContext> context_;

public:
	AcceptAwaitable(tcpSocket& socket) 
		: listenSocket_(socket),
		  context_(std::make_unique<ioContext>(ioOpType::ACCEPT)) {} 
	bool await_ready() const noexcept {return false;}
	void await_suspend(std::coroutine_handle<> continuation) {
		//type to void and fixed parameter name
		context_->continuation = continuation; 
		listenSocket_.getIOService().postAccept(listenSocket_.nativeHandle(), context_.get()); 
		// await_suspend returns void for fire-and-forget semantics
	}
	acceptResult await_resume() {
		acceptResult result;
		if(context_->lastError == 0 && context_->acceptSocket != INVALID_SOCKET) {
			SOCKET ls = listenSocket_.nativeHandle(); // get the listening socket 
			//AcceptEx returns an accepted socket that is not fully associated with the listening socket for some socket-layer behaviors. SO_UPDATE_ACCEPT_CONTEXT fixes that so standard socket queries and options behave correctly.
			int sockResult = setsockopt(context_->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<const char*>(&ls), sizeof(ls));

			if(sockResult != 0) {
				throw NetworkError(WSAGetLastError(), std::format("Failed in setsockopt"));

			}

			result.error = std::error_code{};
			result.nodeSocket = std::make_unique<tcpSocket>(listenSocket_.getIOService(), context_->acceptSocket);
			// transfer ownership
			context_->acceptSocket = INVALID_SOCKET;
		}
		else {
			result.error = std::error_code(context_->lastError, std::system_category());
			result.nodeSocket = nullptr;
		}
		return result;
	}
}; 

// async functions that can be used sequentially 
// These functions return awaitable objects that can be co_awaited
readAwaitable asyncRead(tcpSocket& socket, std::span<char> buffer) {
	return readAwaitable(socket, buffer);
}

AcceptAwaitable asyncAccept(tcpSocket& listenSocket) {
	return AcceptAwaitable(listenSocket);
}

writeAwaitable asyncWrite(tcpSocket& socket, std::span<char> buffer) {
	return writeAwaitable(socket, buffer);
}

#endif // _WIN32
#endif // NETWORK_H
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<winsock2.h>
#include<ws2tcpip.h>
#include<coroutine>
#include<iostream>
#include<string>
#include<thread>
#include<chrono>
#include "Logger.h"

//directive to link winsock library in windows. No need to set it in cmakeList
#pragma comment(lib, "Ws2_32.lib")

/*How the Compiler Uses promise_type
Creation: When the coroutine is first called, the compiler automatically:
Allocates memory for the coroutine frame (which holds the promise object).
Constructs the promise_type object.
Calls get_return_object() to create the Task object, which is then returned to the caller.
Calls initial_suspend() to determine if the coroutine should run immediately or suspend at the start.
Completion: When the coroutine reaches a co_return statement or the end of its body, the compiler automatically:
Calls return_void() (or return_value() if a value is returned) on the promise object.
Calls final_suspend() to determine what happens after the coroutine has finished.
Exception Handling: If an unhandled exception occurs inside the coroutine, the compiler automatically catches it and calls unhandled_exception() on the promise object.
*/

// wrapper for a coroutine handle
// FD_ZERO(fd_set *set): Clears all bits in the fd_set, effectively making it empty.
// FD_SET(int fd, fd_set *set): Adds a file descriptor fd to the fd_set.
// FD_CLR(int fd, fd_set *set): Removes a file descriptor fd from the fd_set.
// FD_ISSET(int fd, fd_set *set): Checks if a file descriptor fd is present in the fd_set.
struct Task {
    struct promise_type {
        // when coroutine is first created
        Task get_return_object(){
            //coroutine handle is non-owning handle to coroutine frame. // resume, destroy and query the state
            //promise type define coroutine and outside world's interaction like suspend. resume etc
            LOG("Creating coroutine return object.");
            return Task {std:: coroutine_handle<promise_type>::from_promise(*this)};
        }
        //std::suspend_never initial_suspend()
        /// This method dictates the coroutine's initial state. std::suspend_never means the coroutine will start
        // executing immediately and won't suspend at the beginning.
        std::suspend_never initial_suspend() noexcept {return {};};
        // what happens when the coroutine finishes executing.
        std::suspend_never final_suspend() noexcept {return {};}
        //    // when the coroutine finishes with a return; statement
        void return_void() {}
        void unhandled_exception() {std::terminate();}
    };
    std::coroutine_handle<promise_type> handle;
    Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    //resource management, as it ensures the memory allocated for the coroutine's frame is properly deallocated, preventing memory leaks.
    ~Task() {
        if(handle) {
            LOG("Destroying coroutine handle.");
            handle.destroy();
        }
    }
};

// awaitable type struct
// wrapper around getting ready to check socket
struct socketWriteAwaitable{
    //windows specfic type for Sockets
    SOCKET  sock;
    // wll be used to store a reference to the coroutine that co_await this object so that we can resume it later
    std::coroutine_handle<> handleToResume;

    // member of the class not any instance
    // this is passed to a thread
    // hence need static function
    // a non-static function has access to a self
    // thread() doesn't know how to pass this.
    static void waitThread(socketWriteAwaitable self){
        LOG("Starting I/O wait thread to check for writability.");
        // file data structure to control filedescriptor for sockets
        // ideall should be pointer of fdsets
        fd_set wfds;
        // thread will stay in loop till socket is writable
        while(true){
            // clear wfds set, making sure we start with zero socket
            FD_ZERO(&wfds);
            // add socket (self.sock) to the wfds set.
            FD_SET(self.sock, &wfds);
            // TODO : use poll or epoll as select can only monitor filedescripters cocurrently less 1024
            // check if any of wfds is ready to be written
            // when select returns all wfd is removed wfds which are not ready to be written to
            // TO CHECK : first argument should not be zero
            // first argument is ignored on windows
            // last nullptr will ensure select will block indefinately till write ready
            if(select(0, nullptr, &wfds, nullptr, nullptr) > 0){
                LOG("Socket is writable. Resuming coroutine.");
                self.handleToResume.resume();
                break;
            }
        }
        LOG("I/O wait thread finished.");
    }
    //  called by the compiler to check if the coroutine should suspend or not.
    // Returning false here forces the coroutine to always suspend and proceed to await_suspend.
    // This is a design choice to ensure that the coroutine always yields control and starts the asynchronous wait process.
    bool await_ready() noexcept {
        LOG("Checking if socket is ready to write (await_ready). Returning false to force suspension.");
        return false;
    }
    // passing a copy of this socketWriteAwaitable object to the waitThread function
    // This new thread will block on select until the socket is ready. detach() separates the thread from the main thread, allowing it to run independently.
    //The function doesn't have an explicit return statement,
    // which is a common practice when the suspension is guaranteed. On Windows, it returns void which the compiler converts to bool,
    // but a return value of false is generally expected here to indicate suspension.
    void await_suspend(std::coroutine_handle<> h){
        LOG("Suspending coroutine (await_suspend). Creating a new thread for select().");
        handleToResume = h;
        std::thread(waitThread, *this).detach();
    }
    // will happen after coroutine is resumed.
    void await_resume() noexcept {
        LOG("Coroutine resumed (await_resume).");
    };

};


// has a coroutine co-await it is identified as coroutine
Task clientMain() {
    LOG("Client main coroutine started.");
    // struct to hold information about the Windows Sockets (Winsock) implementation.
    WSADATA wsaData;
    // Initializes the Winsock library. MAKEWORD(2,2) requests version 2.2 of Winsock.
    //This function must be called successfully before any other socket functions can be used.
    LOG("Initializing Winsock.");
    WSAStartup(MAKEWORD(2,2), &wsaData);
    // af_inet -> ipv4
    // SOCK_STREAM -> tcp
    // return a fd which contains the endpoint
    // #include <sys/socket.h>
    // int socket(int domain, int type, int protocol);
    // from https://man7.org/linux/man-pages/man2/socket.2.html
    // The protocol specifies a particular protocol to be used with the
    //    socket.  Normally only a single protocol exists to support a
    //    particular socket type within a given protocol family, in which
    //    case protocol can be specified as 0.  However, it is possible that
    //    many protocols may exist, in which case a particular protocol must
    //    be specified in this manner.  The protocol number to use is
    //    specific to the “communication domain” in which communication is
    //    to take place; see protocols(5).  See getprotoent(3) on how to map
    //    protocol name strings to protocol numbers.
    LOG("Creating socket.");
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    // Declares a structure serv to hold the server's address information (IP address and port).
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(5555); // Sets the server's port to 5555. htons ("host to network short") converts the number into network byte order,
    //which is required for network communication.
    //  Converts the string IP address "127.0.0.1" (localhost, the same machine) into its binary format and stores it in serv.sin_addr.
    inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr);
    // struct sockaddr {
    // unsigned short sa_family;  // Address family (e.g., AF_INET for IPv4)
    // char           sa_data[14]; // 14 bytes for the actual address data
    // };
    //     struct sockaddr_in {
    //     short            sin_family;   // Should be AF_INET
    //     unsigned short   sin_port;     // Port number
    //     struct in_addr   sin_addr;     // IP address
    //     char             sin_zero[8];  // Padding to make it the same size as sockaddr
    // };\
    // In short, the cast is a promise to the compiler that your sockaddr_in pointer
    //can be treated like a generic sockaddr pointer because their memory layouts are
    //compatible for determining the underlying protocol.
    LOG("Connecting to server 127.0.0.1:5555.");
    // can timeout 0 is success, 1 is failure
    auto ret = connect(sock, (sockaddr*) &serv, sizeof(serv));
    if(ret == -1) {
        LOG("Connection failed. Error: " + std::to_string(WSAGetLastError()));
        std:: cout << "[CLIENT] : connection refused " << std::endl;
        co_return;
    }

    LOG("Connected to server successfully.");
    std::cout<< "[CLIENT] : Connected To Server" << std::endl;

    for(int i = 0; i < 5; i++) {
        //The coroutine pauses here and gives control back to the caller (the main function).
        // It will only resume when the socket (sock) is ready to be written to without blocking.
        //This readiness check is handled by the socketWriteAwaitable struct,
        // which starts a separate thread to monitor the socket using select().
        //When select() indicates the socket is writable, that thread resumes this coroutine.
        LOG("Loop " + std::to_string(i) + ": Waiting for socket to be writable (co_await).");
        co_await socketWriteAwaitable {sock};
        LOG("Resumed. Now calling send().");
        std::string mes = "key " + std::to_string(i) + " = value " + std::to_string(i) + "\n";
        send(sock, mes.c_str(), mes.size(), 0);
        LOG("Sent: " + mes);
        std::cout << "[CLIENT] Sent : " << mes ;
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    // Closes the socket and terminates the connection.
    LOG("Closing socket.");
    closesocket(sock);
    //  Releases the resources allocated by the WSAStartup call. It's the counterpart to WSAStartup.
    LOG("Cleaning up Winsock.");
    WSACleanup();
    LOG("Client main coroutine finished.");
}

int main() {
    // Clear log file on client start for cleaner testing sessions.
    std::ofstream ofs("network_log.txt", std::ofstream::trunc);
    ofs.close();
    
    Logger::init("network_log.txt");
    LOG("Application starting.");
    auto c = clientMain();
    LOG("clientMain coroutine has been called. Main thread will now sleep to allow it to run.");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    LOG("Main thread finished sleeping. Exiting application.");
}
#include<network.h>
#include<chrono>
#include<future>
#include<mutex>
#include<iostream> 

const int BASE_PORT = 4000;
const int SERVER_COUNT = 5;

// coroutine acceptLoop 
AsyncTask acceptLoop(tcpSocket& listenSock, int id, std::vector<std::shared_ptr<tcpSocket>>& peers){
    while(true) {
        auto acceptRes = co_await asyncAccept(listenSock);
        if(acceptRes.error) {
            std::cout << "SERVER " << id << "] Accept Error " << acceptRes.error.message() << std::endl;
            continue;
        }
        std::cout << "[SERVER " << id << "] incoming connection Accepted " << std::endl;
        auto sock = std::move(acceptRes.nodeSocket);
        peers.push_back(std::shared_ptr<tcpSocket>(std::move(sock)));
        
        // 
        auto messagePool = new char[1024];
        std::span<char> buffer(messagePool  , 1024);
        while(true) {
            auto res = co_await asyncRead(*peers.back(), std::span<char>(buffer.data(), buffer.size()));
            if(res.error || res.bytesRead == 0){
                std::cout << "[Server " << id << "] peer disconnected " << std::endl;
                break;
            }
            std::string msg(buffer.data(), res.bytesRead);
            std::cout << "[Server" << id <<" ] Recieved " << msg << std::endl;

        }


    }
}

// Coroutine : connect to peer and send messages periodically 
AsyncTask connectAndChat(ioService& ios, int id , int peerId, std::vector<std::shared_ptr<tcpSocket>>& peers) {
    tcpSocket sock(ios); // creates a new socket in constructor 
    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(BASE_PORT + peerId);
    inet_pton(AF_INET, "127.0.0.1", &peerAddr.sin_addr);

    // keep trying to connect 
    while(connect(sock.nativeHandle(), (sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR) {
        std::cerr << "[Server " << id << "trying to connect to peer " << peerId  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[Server " << id << " ] Connected to Peer " << peerId << std::endl;
    auto peerSock = std::make_shared<tcpSocket>(std::move(sock));

    peers.push_back(peerSock);
    int counter = 0;
    while(true){
        std::string msg = "Hello from Server "  + std::to_string(id) + " msg# " + std::to_string(id) ;
        auto res = co_await asyncWrite(*peerSock, std::span<char>((char*) msg.data(), msg.size()));
        if(res.error) {
            std::cerr << "[Server " << id << "] Write Error to peer " << peerId << " : " << res.error.message() << std::endl;
            break;

        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }



}

int main(int argc, char* argv[]) {
    std::cout << "testNetwork starting..." << std::endl;
    std::cout.flush();
    
    
    if(argc < 2) { 
        std::cerr << "Usage: server.exe <id> " << std::endl;
        std::cerr.flush();
        return 1;
    }
    int id = std::stoi(argv[1]);
    std::cout << "Server ID: " << id << std::endl;
    std::cout.flush();
    try {
        winSockSetter wsa;
        ioService ios(4);

        //listen Socket
        tcpSocket listenSock(ios);
        listenSock.bindAndListen("127.0.0.1", BASE_PORT + id);
        std::cout << "[Server " << id  << "] Listening on port " << BASE_PORT + id << std::endl;
        std::cout.flush(); // Force output to appear immediately

        std::vector<std::shared_ptr<tcpSocket>> peers;
        
        // Start accept loop coroutine (don't await - let it run in background)
        auto acceptTask = acceptLoop(listenSock, id, peers);

        // connect to all higher ids only 
        std::vector<AsyncTask> connectTasks;
        for(int peerId = 0; peerId < SERVER_COUNT ; peerId++){
            if(peerId == id) continue ;
            if(peerId > id) {
                std::cout << "[Server " << id << "] Connecting to peer " << peerId << std::endl;
                std::cout.flush();
                connectTasks.push_back(connectAndChat(ios, id, peerId, peers));
            }
        }

        std::cout << "[Server " << id << "] Starting IO service..." << std::endl;
        std::cout.flush();
        ios.run();
        std::cin.get();
        ios.stop();

    } catch (const std::exception& e) {
        std::cerr << "[Server " << id << "] Exception : " << e.what() << std::endl;
    }

}
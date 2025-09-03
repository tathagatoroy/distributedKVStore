#include<network.h>
#include<chrono>
#include<future>
#include<mutex>
#include<iostream> 
#include<queue>

const int BASE_PORT = 4000;
const int SERVER_COUNT = 5;


namespace node{


    struct logEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        int peerId;
        std::string logLevel;

        logEntry(int id, const std::string& msg, const std::string& level = "INFO") : peerId(id), message(msg), logLevel(level), timestamp(std::chrono::system_clock::now()) {}
    };

    
    const char* tag = "NODE";
    // lock free Single Producer Single Consumer queue for high performance 
    template<typename T, size_t size>
    class lockFreeSPSCQueue{
        private:
            // alignas(64) to ensure 64 byte alignment for atomic operations
            // Prevents false sharing: In multi-threaded code, 
            //when different threads access different variables that happen to be on the same cache line, it causes cache invalidation. This alignment prevents that.
            struct alignas(64) alignedItem {
                std::atomic<bool> ready{false};
                T item;
            };
            // circular buffer for items idx. Write and Read idx facilitate queue dequeu in FCFS without memory reallocation upon overflow/delete
            alignedItem buffer_[size];
            alignas(64) std::atomic<size_t> writeIndex_{0};
            alignas(64) std::atomic<size_t> readIndex_{0};


        public:
            bool tryPush(T&& item) {
                size_t writeIdx, nextWriteIdx;
                do {
                    writeIdx = writeIndex_.load(std::memory_order_relaxed);  
                    nextWriteIdx = (writeIdx + 1) % size;

                    if(nextWriteIdx == readIndex_.load(std::memory_order_relaxed)){ 
                        std::cout << "Task Queue is full" << std::endl;
                        return false;
                    }
                } while(!writeIndex_.compare_exchange_weak(writeIdx, nextWriteIdx, std::memory_order_relaxed));

                buffer_[writeIdx].item = std::move(item);
                buffer_[writeIdx].ready.store(true, std::memory_order_release);  
                return true;
            }

            bool tryPop(T& item){
                size_t readIdx, nextReadIdx;
                do {
                    readIdx = readIndex_.load(std::memory_order_relaxed);
                    nextReadIdx = (readIdx + 1) % size;
                    if(readIdx == writeIndex_.load(std::memory_order_relaxed)){
                        std::cout << "queue is empty" << std::endl;
                        return false;
                    }

                    if(!buffer_[readIdx].ready.load(std::memory_order_acquire)){
                        std::cout << "producer hasn't produced this item yet" << std::endl;  
                        return false;
                    }

                } while(!readIndex_.compare_exchange_weak(readIdx, nextReadIdx, std::memory_order_relaxed));
                item = std::move(buffer_[readIdx].item);
                buffer_[readIdx].ready.store(false, std::memory_order_release);
                return true;
            }

            bool empty() const {
                const size_t readIdx = readIndex_.load(std::memory_order_relaxed);
                return !buffer_[readIDx].ready.load(std::memory_order_acquire);
            }
    };

    class AsyncLogger{
        private:
            static constexpr size_t QUEUE_SIZE = 1000;
            static constexpr size_t BATCH_SIZE = 64;

            struct threadSafeQueue {
                std::queue<logEntry> taskQueue;
                mutable std::mutex mutex;
                std::condition_variable condition;

                void push(logEntry&& entry) {
                    std::lock_guard<std::mutex> lock(mutex);
                    taskQueue.push(std::move(entry));
                    condition.notify_one();
                }

            }
    };

    struct rawMessage{
        uint32_t peerId;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        std::string message;
        rawMessage(uint32_t peerId, std::chrono::time_point<std::chrono::steady_clock> timestamp, char* buffer, uint32_t numBytes)
        : peerId(peerId), timestamp(timestamp), message(buffer, numBytes) {}
    }
    static const numPeers = 5;
    class node {
        private:
            std::vector<std::shared_ptr<tcpSocket>>& peers;
            std::vector<rawMessage> messages;
            
    }
}
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
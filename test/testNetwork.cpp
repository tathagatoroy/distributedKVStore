#include<network.h>
#include<chrono>
#include<future>
#include<mutex>
#include<iostream> 
#include<queue>
#include<fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include<utils.h>
#include<bufferQueue.h>
#include<config.h>


/*
logEntry : struct to store message 
lockFreeSPSCQueue :  struct lock free Single Producer Single Consumer queue for high performance . producer thread pushes message to the queue to be written by worker threads
logFileManager : class to handle log file 
asyncLogger : logger which controls worker threads which uses a threadsafe queue to log the messages to logfile using filemanager

*/

    




namespace node{
    // tag for log messages
    const char* tag = "NODE";


    // structure to hold log entries
    struct logEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string message;
        int peerId;
        std::string logLevel;

        logEntry(int id, const std::string& msg, const std::string& level = "INFO") : peerId(id), message(msg), logLevel(level), timestamp(std::chrono::system_clock::now()) {}
        std::string serialize(){
            std::ostringstream oss;
            auto timeVal = timePointToString(timestamp);
            oss << "[" << timeVal << "][" << logLevel << "][Node" << std::to_string(peerId) << "] : " << message << std::endl;
            return oss.str();

        }
    };

    



    class logFileManager {
        private:
            std::string logFileName_;
            std::ofstream logFile_;
            std::mutex fileMutex_;
            size_t currentFileSize_{0};
            size_t maxFileSize_;
        
        public:
            logFileManager(const std::string& filename, size_t maxFileSizeMB = 100) : 
                logFileName_(filename), maxFileSize_(maxFileSizeMB = 100) {
                    /*
                    std::unique_lock<std::mutex> lock(fileMutex_); 
                    lock not needed as threads are not created yet
                    */ 
                    logFile_.open(logFileName_, std::ios::app);
                    if(!logFile_) {
                        throw std::runtime_error("Failed to open log file");
                    }
                }
            // no destructor needed as all vars are stack allocated


            void log(const std::string& message) {
                std::unique_lock<std::mutex> lock(fileMutex_);

                // TODO 1. properly file handling in case of close
                // This check is important to prevent writing to a closed file stream, which can lead to errors.
                if (!logFile_.is_open()) {
                    // You can handle this case by logging an error to the console or
                    // trying to reopen the file, but for now, we'll simply return.
                    return; 
                }
                // auto now = std::chrono::system_clock::now();
                // // Convert to time_t object 
                // auto timeTNow = std::chrono::system_clock::to_time_t(now);
                // // Get milliseconds
                // auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
                // // Convert to tm struct for formatting
                // std::tm timeinfo;
                // // This is specific to Windows
                // // On Windows, localtime_s is the safe, thread-safe version of localtime.
                // // It's part of the C Runtime Library and is the recommended function to use.
                // localtime_s(&timeinfo, &timeTNow);
                // std::stringstream ss;
                // ss << std::put_time(&timeinfo, "%y-%m-%d %H:%M:%S"); // Changed %b to %m for month number
                // ss << "::" << std::setfill('0') << std::setw(3) << ms.count();
                // logFile_ << "[" << ss.str() << "] " << message << std::endl;
                logFile_.write(message.c_str(), message.size());
                logFile_.flush();
                currentFileSize_ += message.size();

                // TODO : handle log file size being exceeded
                // if(currentFileSize_ > maxFileSize_) return 
                
            }
            void closeFile(){
                std::unique_lock<std::mutex> lock(fileMutex_);
                logFile_.close();
            }

    };
    // high performance async logger using lock free SPSC queue
    class asyncLogger {
        private :
            // constexpr means value needs to be known at comp time while const can be initialised latter



            threadSafeQueue<logEntry> logQueue_;
            std::vector<std::thread> workerThreads_;
            std::atomic<bool> running_{true};

            //file manager 
            logFileManager fileManager;

            // performance metrics
            std::atomic<uint64_t> totalLogsProcessed_{0};
            std::atomic<uint64_t> droppedLogs_{0};
        public:
            asyncLogger(const std::string& filename, size_t numThreads = 2, size_t maxFileSizeMB = 100)
            :fileManager(filename, maxFileSizeMB) 
            {
                for(size_t i = 0; i < numThreads; ++i) {
                    workerThreads_.emplace_back([this, i] () {
                        workerLoop(i);
                    });
                }
            
                std::cout<< "AsyncLogger started with " << numThreads << " threads " << std::endl;
            }
            ~asyncLogger() {
                shutdown();
            }

            void log(int peerID, const std::string& message, const std::string& level = "INFO"){
                if(!running_.load(std::memory_order_relaxed)) {
                    std::cout << "no threads are running so no logging possible" << std::endl;
                }
                try {
                    logQueue_.push(logEntry(peerID, message, level));
                } catch(...) {
                    droppedLogs_.fetch_add(1, std::memory_order_relaxed);
                }
            }
            // specialized logging methods 
            void logError(int peerID, const std::string& message) {
                log(peerID, message, "ERROR");

            }
                        // specialized logging methods 
            void logWarn(int peerID, const std::string& message) {
                log(peerID, message, "WARNING");
                
            }
                        // specialized logging methods 
            void logInfo(int peerID, const std::string& message) {
                log(peerID, message, "INFO");
                
            }


            // get methods for performance 
            uint64_t getTotalLogsProcessed() const {
                return totalLogsProcessed_.load(std::memory_order_relaxed);

            }
            uint64_t getDroppedLogs() const {
                return droppedLogs_.load(std::memory_order_relaxed);

            }
            size_t getQueueSize() const {
                // threadsafe queue handles the race condition
                return logQueue_.size();
            }

            void shutdown() {
                if(!running_.exchange(false)){
                    return; // already shutdown
                }
                std::cout << "Shutting Down logger" <<std::endl;

                // wait for all threads to finish 
                for(auto& thread : workerThreads_){
                    if(thread.joinable()) thread.join();
                }

                // process the remaining logs 
                std::vector<logEntry> remainingLogs;
                while(logQueue_.tryPopBatch(remainingLogs, BATCH_SIZE)) {
                    writeBatch(remainingLogs);
                }
                fileManager.closeFile();

                // present stats 
                std::cout << "AsyncLogger is shut down. Total Logs " << getTotalLogsProcessed() << " Dropped Logs " << getDroppedLogs << std::endl;




            }
        private:
            void workerLoop(size_t threadID){
                std::vector<logEntry> batch;
                batch.reserve(BATCH_SIZE);

                std::cout << "worker thread id : " << threadID << "started " << std::endl;
                while(running_.load(std::memory_order_relaxed)){
                    try{
                        // try to get the batch if it exist 
                        if(logQueue_.tryPopBatch(batch, BATCH_SIZE)) writeBatch(batch);
                        else {
                            logQueue_.waitPopBatch(batch, BATCH_SIZE);
                            if(!batch.empty()) writeBatch(batch);
                        }
                    } catch(const std:: exception& e) {
                        std::cout << "Logger Thread[" << threadID << "] error: " << e.what() << std::endl;
                    }
                
                }
                std::cout << "Logging Worker Thread [" << threadID << "] is being terminated" << std::endl;
            }

            void writeBatch(std::vector<logEntry>& batch){
                if(!batch.size()) return;
                // faster to format the batch in one go rather than using repeated writes
                std::ostringstream buffer;
                for(auto& log : batch){
                    buffer << log.serialize() << "\n";

                }
                fileManager.log(buffer.str());
                totalLogsProcessed_.fetch_add(batch.size(), std::memory_order_relaxed);


            }


    };

    class netWorkManager {
        private:
            std::shared_ptr<asyncLogger> logger_;
            std::vector<std::shared_ptr<tcpSocket>>& peers;

        public:
            netWorkManager(int nodeID)
                : logger_(std::make_shared<asyncLogger>("network_" + std::to_string(nodeID) + ".log")) {}
    }
    
}



//     struct rawMessage{
//         uint32_t peerId;
//         std::chrono::time_point<std::chrono::steady_clock> timestamp;
//         std::string message;
//         rawMessage(uint32_t peerId, std::chrono::time_point<std::chrono::steady_clock> timestamp, char* buffer, uint32_t numBytes)
//         : peerId(peerId), timestamp(timestamp), message(buffer, numBytes) {}
//     }
//     static const numPeers = 5;
//     class node {
//         private:
//             std::vector<std::shared_ptr<tcpSocket>>& peers;
//             std::vector<rawMessage> messages;
            
//     }
// }
// // coroutine acceptLoop 
// AsyncTask acceptLoop(tcpSocket& listenSock, int id, std::vector<std::shared_ptr<tcpSocket>>& peers){
//     while(true) {
//         auto acceptRes = co_await asyncAccept(listenSock);
//         if(acceptRes.error) {
//             std::cout << "SERVER " << id << "] Accept Error " << acceptRes.error.message() << std::endl;
//             continue;
//         }
//         std::cout << "[SERVER " << id << "] incoming connection Accepted " << std::endl;
//         auto sock = std::move(acceptRes.nodeSocket);
//         peers.push_back(std::shared_ptr<tcpSocket>(std::move(sock)));

        
//         // 
//         auto messagePool = new char[1024];
//         std::span<char> buffer(messagePool  , 1024);
//         while(true) {
//             auto res = co_await asyncRead(*peers.back(), std::span<char>(buffer.data(), buffer.size()));
//             if(res.error || res.bytesRead == 0){
//                 std::cout << "[Server " << id << "] peer disconnected " << std::endl;
//                 break;
//             }
//             std::string msg(buffer.data(), res.bytesRead);
//             std::cout << "[Server" << id <<" ] Recieved " << msg << std::endl;

//         }


//     }
// }

// // Coroutine : connect to peer and send messages periodically 
// AsyncTask connectAndChat(ioService& ios, int id , int peerId, std::vector<std::shared_ptr<tcpSocket>>& peers) {
//     tcpSocket sock(ios); // creates a new socket in constructor 
//     sockaddr_in peerAddr{};
//     peerAddr.sin_family = AF_INET;
//     peerAddr.sin_port = htons(BASE_PORT + peerId);
//     inet_pton(AF_INET, "127.0.0.1", &peerAddr.sin_addr);

//     // keep trying to connect 
//     while(connect(sock.nativeHandle(), (sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR) {
//         std::cerr << "[Server " << id << "trying to connect to peer " << peerId  << std::endl;
//         std::this_thread::sleep_for(std::chrono::seconds(1));
//     }
//     std::cout << "[Server " << id << " ] Connected to Peer " << peerId << std::endl;
//     auto peerSock = std::make_shared<tcpSocket>(std::move(sock));

//     peers.push_back(peerSock);
//     int counter = 0;
//     while(true){
//         std::string msg = "Hello from Server "  + std::to_string(id) + " msg# " + std::to_string(id) ;
//         auto res = co_await asyncWrite(*peerSock, std::span<char>((char*) msg.data(), msg.size()));
//         if(res.error) {
//             std::cerr << "[Server " << id << "] Write Error to peer " << peerId << " : " << res.error.message() << std::endl;
//             break;

//         }
//         std::this_thread::sleep_for(std::chrono::seconds(3));
//     }



// }

// int main(int argc, char* argv[]) {
//     std::cout << "testNetwork starting..." << std::endl;
//     std::cout.flush();
    
    
//     if(argc < 2) { 
//         std::cerr << "Usage: server.exe <id> " << std::endl;
//         std::cerr.flush();
//         return 1;
//     }
//     int id = std::stoi(argv[1]);
//     std::cout << "Server ID: " << id << std::endl;
//     std::cout.flush();
//     try {
//         winSockSetter wsa;
//         ioService ios(4);

//         //listen Socket
//         tcpSocket listenSock(ios);
//         listenSock.bindAndListen("127.0.0.1", BASE_PORT + id);
//         std::cout << "[Server " << id  << "] Listening on port " << BASE_PORT + id << std::endl;
//         std::cout.flush(); // Force output to appear immediately

//         std::vector<std::shared_ptr<tcpSocket>> peers;
        
//         // Start accept loop coroutine (don't await - let it run in background)
//         auto acceptTask = acceptLoop(listenSock, id, peers);

//         // connect to all higher ids only 
//         std::vector<AsyncTask> connectTasks;
//         for(int peerId = 0; peerId < SERVER_COUNT ; peerId++){
//             if(peerId == id) continue ;
//             if(peerId > id) {
//                 std::cout << "[Server " << id << "] Connecting to peer " << peerId << std::endl;
//                 std::cout.flush();
//                 connectTasks.push_back(connectAndChat(ios, id, peerId, peers));
//             }
//         }

//         std::cout << "[Server " << id << "] Starting IO service..." << std::endl;
//         std::cout.flush();
//         ios.run();
//         std::cin.get();
//         ios.stop();

//     } catch (const std::exception& e) {
//         std::cerr << "[Server " << id << "] Exception : " << e.what() << std::endl;
//     }

// }
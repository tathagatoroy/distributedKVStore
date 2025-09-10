#ifndef BUFFERQ_H
#define BUFFERQ_H


#include<chrono>
#include<future>
#include<mutex>
#include<iostream> 
#include<queue>
#include<fstream>
#include <string>
#include <sstream>
#include <iomanip>

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
                // if write index is at the same place as read idx then queue is full
                if(nextWriteIdx == readIndex_.load(std::memory_order_relaxed)){ 
                    std::cout << "Task Queue is full" << std::endl;
                    return false;
                }
                // else keep on checking as long as no one has modified WriteIdx while we do this check
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


// multiple producer multiple consumer queue with traditional locking
template<typename T, size_t size>
struct threadSafeQueue {
    std::queue<T> queue;
    mutable std::mutex mutex; // allows it to be modified inside const member function
    std::condition_variable condition; // waiting on a varib

    void push(T&& entry) {// passing a rvalue
        std::lock_guard<std::mutex> lock(mutex); // lock is release once of out scope
        queue.push(std::move(entry));
        condition.notify_one();
    }

    // batch popping 
    bool tryPopBatch(std::vector<T>& batch, size_t maxSize){
        std::unique_lock<std::mutex> lock(mutex); // creates a lock on mutext
        if(queue.empty()){
            // queue is empty 
            return false;
        }
        // clear buffer 
        batch.clear();
        batch.reserve(maxSize);

        while(!queue.empty() && batch.size() < maxSize) {
            batch.push_back(std::move(queue.front()));
            queue.pop();
        }
        return !batch.empty();
    }

    // this waits till queue is not empty 
    // while trypopwatch doesn't , only till I acquire it 
    void waitPopBatch(std::vector<T>& batch, size_t maxSize){
        std::unique_lock<std::mutex> lock(mutex);
        // lock must be held by thread before wait is called 
        // wait atomically release the locks and put it to sleep
        // [this] { return !queue.empty(); } is lambda expression which is the second argument
        // [this] allows lambda to acquire access member object like queue 
        // !queue.empty() is true , queue not empty thread doesn't need to wait and continues execution
        // condition is busy waiting 
        condition.wait(lock, [this] { return !queue.empty(); }); 
        batch.clear();
        batch.reserve(maxSize);

        while(!queue.empty() && batch.size() < maxSize){
            batch.push_back(std::move(queue.front()));
            queue.pop();
        }

    }

    size_t size() const{
        std::lock_guard<std::mutex> lock(mutex);
        return queue.size();
    }
};


#endif
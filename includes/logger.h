#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <thread>
#include <sstream>
#include <chrono>
#include <iomanip>

// Macro to simplify logging calls. It automatically captures the function name.
#define LOG(message) Logger::log(__func__, message)

class Logger {
public:
    // Initializes the logger and opens the log file.
    static void init(const std::string& filename) {
        // Open file in append mode.
        log_file.open(filename, std::ios_base::app);
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }

    // Logs a message with a timestamp, thread ID, function name, and the message.
    static void log(const std::string& function_name, const std::string& message) {
        if (!log_file.is_open()) return;

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        // Use a stringstream to format the thread ID.
        std::stringstream ss;
        ss << std::this_thread::get_id();

        // Lock the mutex to ensure thread-safe writes to the file.
        std::lock_guard<std::mutex> guard(log_mutex);
        log_file << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
                 << '.' << std::setfill('0') << std::setw(3) << ms.count()
                 << " [Thread: " << ss.str() << "]"
                 << " [" << function_name << "] "
                 << message << std::endl;
    }

private:
    // static members are shared across all instances of the class.
    static std::ofstream log_file;
    static std::mutex log_mutex;
};

// Initialize static members.
std::ofstream Logger::log_file;
std::mutex Logger::log_mutex;

#endif // LOGGER_H
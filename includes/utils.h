#ifndef UTILS_H
#define UTILS_H

#include<iostream>
#include <cstdarg>
#include <cstdio>
#include<string>
#include <chrono>
#include<iomanip>
#include<ctime>


inline std::string formatString(const char* tag, const char* fmt, ...){
    char buffer[1024];
    // type to use var arguments
    va_list args;
    // start initialise extra arguments starting from fmt
    va_start(args, fmt);
    // safer than printf for formatting 
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    // clean up the variable list. Crucial to do this
    va_end(args);
    std::string result = std::string("DEBUG : [") + tag + "] " + buffer;
    return result;
}
inline void logD(const char* tag, const char* fmt, ...){
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::cout << "DEBUG : [" << tag << "] " << buffer << std::endl;

}

inline void logI(const char* tag, const char* fmt, ...){
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    std::cout << "INFO : [" << tag << "] " << buffer << std::endl;

}

std::string timePointToString(const std::chrono::system_clock::time_point& time_point) {
    // Convert time_point to a time_t object
    // The time_t object represents the time in seconds since the epoch
    std::time_t time_t_value = std::chrono::system_clock::to_time_t(time_point);

    // Convert time_t to a tm struct (for local time)
    // tm is a structure that holds calendar time components (year, month, day, etc.)
    std::tm tm_struct = *std::localtime(&time_t_value);

    // Use a stringstream to format the time into a string
    std::ostringstream oss;
    oss << std::put_time(&tm_struct, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

#endif
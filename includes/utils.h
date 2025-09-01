#ifndef UTILS_H
#define SERIALIZE_H

#include<iostream>
#include <cstdarg>
#include <cstdio>
#include<string>


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


#endif
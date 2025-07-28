#ifndef UTILS_H
#define SERIALIZE_H

#include<iostream>
#include <cstdarg>
#include <cstdio>

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
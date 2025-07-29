// reference : https://github.com/resetius/miniraft-cpp/blob/master/miniraft/messages.h
#ifndef MESSAGES_H
#define MESSAGES_H

#include<chrono>
#include<memory>
#include<vector>
#include<assert.h>
#include<stdint.h>
#include<concepts>
#include <string.h>
#include <typeinfo>

namespace messages {
    static const char* logTag = "MESSAGES";
    
    enum class eMessageType : uint32_t {
        NONE = 0,
        LOG_ENTRY = 1,
        REQUEST_VOTE_REQUEST = 2,
        REQUEST_VOTE_RESPONSE = 3,
        APPEND_ENTRIES_REQUEST = 4,
        APPEND_ENTRIES_RESPONSE = 5,
        INSTALL_SNAPSHOT_REQUEST = 6, // Todo
        INSTALL_SNAPSHOT_RESPONSE = 7, // Todo
        COMMAND_REQUEST = 8,
        COMMAND_RESPONSE = 9
    };

    struct tMessage {
        static constexpr eMessageType MessageType = eMessageType::NONE; // type of the message
        uint32_t type ;
        uint32_t len;
        // char value[0] ; // flexible array member, used for variable length messages
        // for flexible array member, the size is determined at runtime
        // so the size of the message is sizeof(tMessage) + len
        // improve cache locality and contiguity preferred for network I/O
        // 8 bytes at the start for type and len
        // followed by the value of variable length
        // for example, if len = 4, then the size of the message is 12 bytes
        // if len = 0, then the size of the message is 8 bytes

    };
    static_assert(sizeof(tMessage) == 8, "Size of tMessage must be 8 bytes");

    // define log entry message
    struct tLogEntry : public tMessage {
        static constexpr eMessageType MessageType = eMessageType::LOG_ENTRY;
        enum eFlags {
            eNone = 0, 
            eStub = 1
        };
        uint64_t term = 1;
        uint64_t flags = 0;
        char data[0];
    };

    struct tmessageEx : public tMessage {
        uint32_t src = 0;
        uint32_t dst = 0;
        uint64_t term = 0;
        uint64_t seqNo = 0;
        // char data[0]; // flexible array member


    };

    static_assert(sizeof(tmessageEx) == sizeof(tMessage) + 24, "Size of tMessageEx must be 24 bytes more than tMessage");

    struct tRequestVoteRequest : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::REQUEST_VOTE_REQUEST;
        uint64_t lastLogIndex = 0;
        uint64_t lastLogTerm = 0;
        uint32_t candidateId = 0; // id of the candidate
        uint32_t padding = 0;
        char data[0]; // flexible array member for variable length data
    };

    static_assert(sizeof(tRequestVoteRequest) == sizeof(tmessageEx) + 24, "Size of tRequestVoteRequest must be 24 bytes more than tmessageEx");

    struct tRequestVoteResponse : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::REQUEST_VOTE_RESPONSE;
        uint32_t voteGranted; // 1 if vote granted, 0 otherwise
        uint32_t padding = 0;
        char data[0]; // flexible array member for variable length data
    };
    static_assert(sizeof(tRequestVoteResponse) == sizeof(tmessageEx) + 8, "Size of tRequestVoteResponse must be 8 bytes more than tmessageEx");

    struct tAppendEntriesRequest : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::APPEND_ENTRIES_REQUEST;
        uint64_t prevLogIndex = 0; // index of the previous log entry
        uint64_t prevLogTerm = 0; // term of the previous log entry
        uint64_t leaderCommit = 0; // index of the last committed log entry
        uint32_t leaderId = 0; // id of the leader
        uint32_t entriesCount = 0; // number of log entries in the request
        char data[0]; // flexible array member for variable length data
    };

    static_assert(sizeof(tAppendEntriesRequest) == sizeof(tmessageEx) + 32, "Size of tAppendEntriesRequest must be 32 bytes more than tmessageEx");

    struct tAppendEntriesResponse : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::APPEND_ENTRIES_RESPONSE;
        uint64_t matchIndex = 0; // index of the last log entry that was successfully appended
        uint32_t success = 0;
        uint32_t padding = 0;
        char data[0]; // flexible array member for variable length data
    };

    static_assert(sizeof(tAppendEntriesResponse) == sizeof(tmessageEx) + 16, "Size of tAppendEntriesResponse must be 16 bytes more than tmessageEx");


    struct tCommandRequest : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::COMMAND_REQUEST;
        enum eFlags {
            eNone = 0,
            eWrite = 1,
            eStale = 2,
            eConsistent = 4
        };
        uint32_t flags = eNone; // flags for the command
        uint32_t cookie = 0; // cookie for the command
        char data[0]; // flexible array member for variable length data
    };
    static_assert(sizeof(tCommandRequest) == sizeof(tmessageEx) + 8, "Size of tCommandRequest must be 8 bytes more than tmessageEx");

    struct tCommandResponse : public tmessageEx {
        static constexpr eMessageType MessageType = eMessageType::COMMAND_RESPONSE;
        uint64_t index = 0;
        uint32_t cookie = 0;
        uint32_t errorCode = 0;
        char data[0]; // flexible array member for variable length data
    };

    static_assert(sizeof(tCommandResponse) == sizeof(tmessageEx) + 16, "Size of tCommandResponse must be 16 bytes more than tmessageEx");

    // TODO : load this constatnt from a config file
    struct tTimeOut {
        static constexpr std::chrono::milliseconds election = std::chrono::milliseconds(5000); // 5 seconds
        static constexpr std::chrono::milliseconds heartbeat = std::chrono::milliseconds(1000); // 1 second
        static constexpr std::chrono::milliseconds rpc = std::chrono::milliseconds(10000); // 10 seconds
    };

    // T is a derived class of tMessage
    template<typename T>
    requires std::derived_from<T, tMessage>
    struct tMessageHolder {
        T* message;
        std::shared_ptr<char []> rawData; // smart pointer to a raw binary data of the message
        uint32_t payloadSize; // size of the payload
        std::shared_ptr<tMessageHolder<tMessage>[]> payloads; // array of payloads

        // default constructor empty
        tMessageHolder() : message(nullptr) {}

        // paramaterized constructor
        template<typename U> 
        requires std::derived_from<U,T> 
        TMessageHolder(U* u, const std::shared_ptr<char[]>& rawdata, uint32_t payloadsize = 0, const std::shared_ptr<tMessageHolder<tMessage>[]>& payload = {})
        : message(u)
        , rawData(rawdata)
        , payloadSize(payloadsize)
        , payloads(payload) {}

        // copy constructor 
        template<typeName U> 
        requires std::derived_from<U,T> 
        tMessageHolder(const tMessageHolder<U>& other)
        : message(other.message)
        , rawData(other.rawData)
        , payloadSize(other.payLoadSize)
        , payloads(other.payloads) {}

        void initPayload(uint32_t size) {
            payloadSize = size;
            payloads = std:shared_ptr<tMessageHolder<tMessage>[]> ( new tMessageHolder<tMessage>[size]);
        }
        T* operator->() {
            return message;
        }
        const T* operator->() const {
            return message
        }

        operator bool() const {
            !!message;
        }
        bool isEx() const {
            return ( 2 <= mes->type && mes->type <= 5);
        }
    };

}

#endif

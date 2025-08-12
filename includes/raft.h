// reference : https://dzone.com/articles/implementation-of-the-raft-consensus-algorithm-usi
#ifndef RAFT_H
#define RAFT_H

#include<iostream>
#include<cstdint>
#include<vector>
#include<chrono>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include "messages.h"

namespace raft{
    static const char* logTag = "RAFT";


    struct TState {
        uint64_t term;
        uint32_t votedFor;
        std::vector<tMessageHolder<tLogEntry>> log;


    };
    enum class nodeState{
        follower,
        candidate,
        leader
    };

    inline const char* nodeStateToString(nodeState state){
        switch(state){
            case nodeState::follower: return "Follower";
            case nodeState::candidate: return "Candidate";
            case nodeState::leader: return "Leader";
            default: return "State Unknown";
        }
    }
    using tTime = std::chrono::time_point<std::chrono::steady_clock>;

    struct tVolateTileState {
        uint64_t commitIndex = 0; // last log entry that was committed for L,F
        uint64_t lastApplied = 0; // last log entry that was applied to the state Machine for L, F lastApplied <= lastCommitIndex
        std::unordered_map<uint32_t, uint64_t> nextIndex; // next log entry to be sent to a peer by L
        std::unordered_map<uint32_t, uint64_t> matchIndex; // last log entry that discovered a match by L
        std::unordered_set<uint32_t> votes; // ids of peers who voted for this peer
        std::unordered_map<uint32_t, tTime> heartBeatDuel; // L heartbeats of the nodes 
        std::unordered_map<uint32_t, tTime> rpcDue; // leader timeouts
        tTime electionDue ; // F follower timeouts



    };

    // represent the abstract struct
    // allows multiple definition of the node which must define the interface
    // makes it flexible
    // virtual destructor is needed for base class destructor
    struct iNode{
        virtual ~iNode() = default;
        virtual void send(tMessageHolder<tMessage> message) = 0; // stores message in a internal buffer
        virtual void drain() = 0;
        
    };

    class tRaft {
        public:
            tRaft(uint32_t node, const std::unordered_map<uint32_t, std::shared_ptr<iNode>>& nodes);
            void process(tTime now, tMessageHolder<tMessage> message, const std::shared_ptr<iNode>& replyTo = {});
            void processTimeOut(tTime now);

    }

}

#endif 
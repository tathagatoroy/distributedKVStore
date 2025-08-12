#ifndef NODES_H
#define NODES_H

#include<iostream>
#include<cstdint>
#include<vector>
#include<chrono>
#include<map>
#include<unordered_map>
#include<unordered_set>
#include "messages.h"

namespace nodes{
    static const char* logTag = "NODES";
    // each server nodes can either of the three state
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
    struct tVolatileState {
        uint32_t lastCommitIndex = 0; // last index committed to log 
        uint32_t lastAppliedToState = 0; // highest index present in state 

        tVolatileState() = default;

        // Copy operations (compiler-generated is fine for simple structs)
        tVolatileState(const tVolatileState&) = default;
        tVolatileState& operator=(const tVolatileState&) = default;
        
        // Move operations (compiler-generated is fine for simple structs)
        tVolatileState(tVolatileState&&) = default;
        tVolatileState& operator=(tVolatileState&&) = default;
    };

    struct tVolatileLeaderState {
        // next Index for each server, index of the next log entry 
        //to send to that server (initialized to leader last log index + 1)
        std::vector<uint32_t> nextIndex;

        //for each server, index of highest log entry known to be replicated on server
        // (initialized to 0, increases monotonically)
        std::vector<uint32_t> matchIndex;

        tVolatileLeaderState(uint32_t numServers, uint32_t leaderLastLogIndex)
        : nextIndex(numServers, leaderLastLogIndex + 1) , matchIndex(numServers, 0) {

        }



    };
    struct nodeAddr {
        std::string address;
        uint16_t port;

        nodeAddr(std::string address, uint16_t port) : address(address) , port(port) {}
        
    };

    class node{
        private:
            nodeState state;
            std::unique_ptr<tVolatileLeaderState> leaderState; 
            std::unique_ptr<tVolatileState> volatileState; 
            std::unique_ptr<nodeAddr> ip4Address;
            uint32_t currentTerm;
            uint32_t votedFor;
            std::vector<std::string> logs;
            uint32_t numServers;
            uint32_t nodeID;

        public:
            node(nodeState state, uint32_t numServers, uint32_t nodeID, std::string address, uint16_t port) 
            : state(state)
            , currentTerm(0)
            , votedFor(-1)
            , logs()
            , nodeID(nodeID)
            , numServers(numServers) {

                volatileState = std::make_unique<tVolatileState>();
                ip4Address = std::make_unique<nodeAddr>(address, port);
                if(state == nodeState::leader) {
                    leaderState = std::make_unique<tVolatileLeaderState>(numServers, volatileState->lastAppliedToState);
                }
            }

        // no destructor needed, smart pointers handle cleanup

        public:



    };
}



#endif
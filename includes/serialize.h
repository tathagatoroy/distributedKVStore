#ifndef SERIALIZE_H
#define SERIALIZE_H

#include<iostream>
#include<string>
#include<sstream>
#include<typeinfo>
#include<utility>
#include <stdexcept>
#include <variant>
#include "utils.h"

const char* logTag = "SERIALIZE";


namespace Serialize {
    template <typename T>
    // const so it is not modified.
    // T& is efficient passing
    std::string serialize(const T& value){
        std::string typeName = typeid(T).name();
        std::string valueStr;
        
        if constexpr (std::is_same_v<T, char>) {
            valueStr = const_cast<char&>(value); // const_cast to remove constness
        }
        else if constexpr(std::is_same_v<T, bool>){
            if(value) valueStr = "True";
            else valueStr = "False";
        }
        else if constexpr (std:: is_arithmetic_v<T>) valueStr = std::to_string(value);
        // for string types 
        else if constexpr (std::is_same_v<T, std::string>) {
            typeName = "string";
            valueStr = value;
        }
        // else if constexpr (std::is_same_v<T, const char *>) valueStr = value;
        else valueStr = "unsupported Types";

        std::ostringstream oss;
        oss << R"({"type":")" <<typeName<<R"(", "value":")"<<valueStr<<R"("})";
        return oss.str();



    }
}

// for now supporting string , int , long long, float, double, char, bool , string 
namespace Deserialize{


    using AnyType = std::variant<int, long long, float, double, char, bool, std::string>;
    std::pair<std::string, std::string> parse(std::string result){
        auto typePos = result.find("\"type\":\""); // not found returns "std::string::npos"
        auto valuePos = result.find("\"value\":\"");

        std::string type, val;
        if(typePos != std::string::npos){
            typePos += 8;
            auto typeEnd = result.find("\"", typePos);
            auto substr = result.substr(typePos, typeEnd - typePos);
            if(typeEnd != std::string::npos){
                type = result.substr(typePos, typeEnd - typePos);
            }
            else{
                std::ostringstream errorMessage;
                errorMessage << "Error : Couldn't parse "<<result;
                std::runtime_error(errorMessage.str());
            }
        }
        else{
            std::ostringstream errorMessage;
            errorMessage << "Error : Couldn't parse "<<result;
            std::runtime_error(errorMessage.str());

        }
        if(valuePos != std::string::npos){
            valuePos += 9;
            auto valueEnd = result.find("\"", valuePos);
            if(valueEnd != std::string::npos){
                val = result.substr(valuePos, valueEnd - valuePos);
            }
            else{
                std::ostringstream errorMessage;
                errorMessage << "Error : Couldn't parse "<<result;
                std::runtime_error(errorMessage.str());
            }
        }
        else{
            std::ostringstream errorMessage;
            errorMessage << "Error : Couldn't parse "<<result;
            std::runtime_error(errorMessage.str());

        }
        return make_pair(type, val);
    }

    AnyType deserialize(std::string output){
        auto res = parse(output);
        auto type = res.first;
        auto value = res.second;
        std::cout<<type<<" "<<value<<std::endl;

        if(type == "int"){
            return std::stoi(value);
        }
        else if(type == "long long"){
            return std::stoll(value);
        }
        else if(type == "float"){
            return std::stof(value);
        }
        else if(type == "double"){
            return std::stod(value);
        }
        else if(type == "char"){
            return value[0]; // as value is a string, we take the first character
        }
        else if(type == "bool"){
            return (value == "True");
        }
        else if(type == "string"){
            return value;
        }
        else{
            throw std::runtime_error("Unsupported type: " + type);
        }
    }

}

#endif 
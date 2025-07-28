#include<iostream>
#include<serialize.h>
#include<string>
#include <cassert>


using namespace std;

int main(){
    int intSample = 5;
    float floatSample = 7.99;
    char charSample = 'c';
    string stringSample = "hello";
    bool boolSample = true;
    double doubleSample = 3.14;

    string intRes = Serialize::serialize(intSample);
    cout<<"Value of input int sample is: "<<intSample<<endl;
    cout<<"After serialization, the value is: "<<intRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    string floatRes = Serialize::serialize(floatSample);
    cout<<"Value of input float sample is: "<<floatSample<<endl;
    cout<<"After serialization, the value is: "<<floatRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    string charRes = Serialize::serialize(charSample);
    cout<<"Value of input char sample is: "<<charSample<<endl;
    cout<<"After serialization, the value is: "<<charRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    string stringRes = Serialize::serialize(stringSample);
    cout<<"Value of input string sample is: "<<stringSample<<endl;
    cout<<"After serialization, the value is: "<<stringRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    string boolRes = Serialize::serialize(boolSample);
    cout<<"Value of input bool sample is: "<<boolSample<<endl;
    cout<<"After serialization, the value is: "<<boolRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    string doubleRes = Serialize::serialize(doubleSample);
    cout<<"Value of input double sample is: "<<doubleSample<<endl;
    cout<<"After serialization, the value is: "<<doubleRes<<endl;
    cout<<endl;
    cout<<"---------------------------------------------------------------------------"<<endl;
    cout<<"Testing deserialization"<<endl;

    auto intResult = Deserialize::deserialize(intRes);
    if (std::holds_alternative<int>(intResult)) {
        int value = std::get<int>(intResult);
        assert(value == intSample);
        cout<<"Deserialized int value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for int"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    auto floatResult = Deserialize::deserialize(floatRes);
    if (std::holds_alternative<float>(floatResult)) {
        float value = std::get<float>(floatResult);
        assert(value == floatSample);
        cout<<"Deserialized float value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for float"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    auto charResult = Deserialize::deserialize(charRes);
    if (std::holds_alternative<char>(charResult)) {
        char value = std::get<char>(charResult);
        assert(value == charSample);
        cout<<"Deserialized char value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for char"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    auto stringResult = Deserialize::deserialize(stringRes);
    if (std::holds_alternative<std::string>(stringResult)) {
        string value = std::get<std::string>(stringResult);
        assert(value == stringSample);
        cout<<"Deserialized string value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for string"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    auto boolResult = Deserialize::deserialize(boolRes);
    if (std::holds_alternative<bool>(boolResult)) {
        bool value = std::get<bool>(boolResult);
        assert(value == boolSample);
        cout<<"Deserialized bool value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for bool"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    auto doubleResult = Deserialize::deserialize(doubleRes);
    if (std::holds_alternative<double>(doubleResult)) {
        double value = std::get<double>(doubleResult);
        assert(value == doubleSample);
        cout<<"Deserialized double value: "<<value<<endl;
    }
    else {
        cout<<"Deserialization failed for double"<<endl;
    }
    cout<<"---------------------------------------------------------------------------"<<endl;
    cout<<"All tests passed!"<<endl;
    return 0;




}
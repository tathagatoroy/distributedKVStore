#!/bin/bash 

cd build
cmake ..
cd ..
cmake -S . -B build
cmake --build build 


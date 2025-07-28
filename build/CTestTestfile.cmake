# CMake generated Testfile for 
# Source directory: C:/Users/91833/Tathagato/distributedKVStore
# Build directory: C:/Users/91833/Tathagato/distributedKVStore/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(runSerializeTest "./build/testExecutables/serialize")
  set_tests_properties(runSerializeTest PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;20;add_test;C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(runSerializeTest "./build/testExecutables/serialize")
  set_tests_properties(runSerializeTest PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;20;add_test;C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(runSerializeTest "./build/testExecutables/serialize")
  set_tests_properties(runSerializeTest PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;20;add_test;C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(runSerializeTest "./build/testExecutables/serialize")
  set_tests_properties(runSerializeTest PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;20;add_test;C:/Users/91833/Tathagato/distributedKVStore/CMakeLists.txt;0;")
else()
  add_test(runSerializeTest NOT_AVAILABLE)
endif()

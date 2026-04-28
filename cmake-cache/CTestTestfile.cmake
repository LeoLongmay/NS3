# CMake generated Testfile for 
# Source directory: /home/master01/CC_Exp
# Build directory: /home/master01/CC_Exp/cmake-cache
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ctest-stdlib_pch_exec "ns3.39-stdlib_pch_exec-debug")
set_tests_properties(ctest-stdlib_pch_exec PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/cmake-cache/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1501;set_runtime_outputdirectory;/home/master01/CC_Exp/CMakeLists.txt;149;process_options;/home/master01/CC_Exp/CMakeLists.txt;0;")
subdirs("src")
subdirs("examples")
subdirs("scratch")
subdirs("utils")

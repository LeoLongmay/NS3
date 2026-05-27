# CMake generated Testfile for 
# Source directory: /home/master01/CC_Exp/src/tap-bridge
# Build directory: /home/master01/CC_Exp/cmake-cache/src/tap-bridge
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ctest-tap-creator "ns3.39-tap-creator-optimized")
set_tests_properties(ctest-tap-creator PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/src/tap-bridge/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1659;set_runtime_outputdirectory;/home/master01/CC_Exp/src/tap-bridge/CMakeLists.txt;40;build_exec;/home/master01/CC_Exp/src/tap-bridge/CMakeLists.txt;0;")
subdirs("examples")

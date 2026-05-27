# CMake generated Testfile for 
# Source directory: /home/master01/CC_Exp/utils
# Build directory: /home/master01/CC_Exp/cmake-cache/utils
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(ctest-test-runner "ns3.39-test-runner-optimized")
set_tests_properties(ctest-test-runner PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/utils/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/utils/CMakeLists.txt;46;set_runtime_outputdirectory;/home/master01/CC_Exp/utils/CMakeLists.txt;0;")
add_test(ctest-bench-scheduler "ns3.39-bench-scheduler-optimized")
set_tests_properties(ctest-bench-scheduler PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/utils/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1659;set_runtime_outputdirectory;/home/master01/CC_Exp/utils/CMakeLists.txt;53;build_exec;/home/master01/CC_Exp/utils/CMakeLists.txt;0;")
add_test(ctest-bench-packets "ns3.39-bench-packets-optimized")
set_tests_properties(ctest-bench-packets PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/utils/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1659;set_runtime_outputdirectory;/home/master01/CC_Exp/utils/CMakeLists.txt;61;build_exec;/home/master01/CC_Exp/utils/CMakeLists.txt;0;")
add_test(ctest-print-introspected-doxygen "ns3.39-print-introspected-doxygen-optimized")
set_tests_properties(ctest-print-introspected-doxygen PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/utils/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1659;set_runtime_outputdirectory;/home/master01/CC_Exp/utils/CMakeLists.txt;68;build_exec;/home/master01/CC_Exp/utils/CMakeLists.txt;0;")
add_test(ctest-perf-io "ns3.39-perf-io-optimized")
set_tests_properties(ctest-perf-io PROPERTIES  WORKING_DIRECTORY "/home/master01/CC_Exp/build/utils/perf/" _BACKTRACE_TRIPLES "/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1584;add_test;/home/master01/CC_Exp/build-support/macros-and-definitions.cmake;1659;set_runtime_outputdirectory;/home/master01/CC_Exp/utils/CMakeLists.txt;77;build_exec;/home/master01/CC_Exp/utils/CMakeLists.txt;0;")

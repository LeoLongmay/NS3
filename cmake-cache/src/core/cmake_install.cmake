# Install script for directory: /home/master01/CC_Exp/src/core

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so"
         RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/master01/CC_Exp/build/lib/libns3.39-core-debug.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so"
         OLD_RPATH "/home/master01/CC_Exp/build/lib:::::::"
         NEW_RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/home/master01/CC_Exp/src/core/model/int64x64-128.h"
    "/home/master01/CC_Exp/src/core/model/example-as-test.h"
    "/home/master01/CC_Exp/src/core/helper/csv-reader.h"
    "/home/master01/CC_Exp/src/core/helper/event-garbage-collector.h"
    "/home/master01/CC_Exp/src/core/helper/random-variable-stream-helper.h"
    "/home/master01/CC_Exp/src/core/model/abort.h"
    "/home/master01/CC_Exp/src/core/model/ascii-file.h"
    "/home/master01/CC_Exp/src/core/model/ascii-test.h"
    "/home/master01/CC_Exp/src/core/model/assert.h"
    "/home/master01/CC_Exp/src/core/model/attribute-accessor-helper.h"
    "/home/master01/CC_Exp/src/core/model/attribute-construction-list.h"
    "/home/master01/CC_Exp/src/core/model/attribute-container.h"
    "/home/master01/CC_Exp/src/core/model/attribute-helper.h"
    "/home/master01/CC_Exp/src/core/model/attribute.h"
    "/home/master01/CC_Exp/src/core/model/boolean.h"
    "/home/master01/CC_Exp/src/core/model/breakpoint.h"
    "/home/master01/CC_Exp/src/core/model/build-profile.h"
    "/home/master01/CC_Exp/src/core/model/calendar-scheduler.h"
    "/home/master01/CC_Exp/src/core/model/callback.h"
    "/home/master01/CC_Exp/src/core/model/command-line.h"
    "/home/master01/CC_Exp/src/core/model/config.h"
    "/home/master01/CC_Exp/src/core/model/default-deleter.h"
    "/home/master01/CC_Exp/src/core/model/default-simulator-impl.h"
    "/home/master01/CC_Exp/src/core/model/deprecated.h"
    "/home/master01/CC_Exp/src/core/model/des-metrics.h"
    "/home/master01/CC_Exp/src/core/model/double.h"
    "/home/master01/CC_Exp/src/core/model/enum.h"
    "/home/master01/CC_Exp/src/core/model/event-id.h"
    "/home/master01/CC_Exp/src/core/model/event-impl.h"
    "/home/master01/CC_Exp/src/core/model/fatal-error.h"
    "/home/master01/CC_Exp/src/core/model/fatal-impl.h"
    "/home/master01/CC_Exp/src/core/model/fd-reader.h"
    "/home/master01/CC_Exp/src/core/model/environment-variable.h"
    "/home/master01/CC_Exp/src/core/model/global-value.h"
    "/home/master01/CC_Exp/src/core/model/hash-fnv.h"
    "/home/master01/CC_Exp/src/core/model/hash-function.h"
    "/home/master01/CC_Exp/src/core/model/hash-murmur3.h"
    "/home/master01/CC_Exp/src/core/model/hash.h"
    "/home/master01/CC_Exp/src/core/model/heap-scheduler.h"
    "/home/master01/CC_Exp/src/core/model/int-to-type.h"
    "/home/master01/CC_Exp/src/core/model/int64x64-double.h"
    "/home/master01/CC_Exp/src/core/model/int64x64.h"
    "/home/master01/CC_Exp/src/core/model/integer.h"
    "/home/master01/CC_Exp/src/core/model/length.h"
    "/home/master01/CC_Exp/src/core/model/list-scheduler.h"
    "/home/master01/CC_Exp/src/core/model/log-macros-disabled.h"
    "/home/master01/CC_Exp/src/core/model/log-macros-enabled.h"
    "/home/master01/CC_Exp/src/core/model/log.h"
    "/home/master01/CC_Exp/src/core/model/make-event.h"
    "/home/master01/CC_Exp/src/core/model/map-scheduler.h"
    "/home/master01/CC_Exp/src/core/model/math.h"
    "/home/master01/CC_Exp/src/core/model/names.h"
    "/home/master01/CC_Exp/src/core/model/node-printer.h"
    "/home/master01/CC_Exp/src/core/model/nstime.h"
    "/home/master01/CC_Exp/src/core/model/object-base.h"
    "/home/master01/CC_Exp/src/core/model/object-factory.h"
    "/home/master01/CC_Exp/src/core/model/object-map.h"
    "/home/master01/CC_Exp/src/core/model/object-ptr-container.h"
    "/home/master01/CC_Exp/src/core/model/object-vector.h"
    "/home/master01/CC_Exp/src/core/model/object.h"
    "/home/master01/CC_Exp/src/core/model/pair.h"
    "/home/master01/CC_Exp/src/core/model/pointer.h"
    "/home/master01/CC_Exp/src/core/model/priority-queue-scheduler.h"
    "/home/master01/CC_Exp/src/core/model/ptr.h"
    "/home/master01/CC_Exp/src/core/model/random-variable-stream.h"
    "/home/master01/CC_Exp/src/core/model/rng-seed-manager.h"
    "/home/master01/CC_Exp/src/core/model/rng-stream.h"
    "/home/master01/CC_Exp/src/core/model/scheduler.h"
    "/home/master01/CC_Exp/src/core/model/show-progress.h"
    "/home/master01/CC_Exp/src/core/model/simple-ref-count.h"
    "/home/master01/CC_Exp/src/core/model/simulation-singleton.h"
    "/home/master01/CC_Exp/src/core/model/simulator-impl.h"
    "/home/master01/CC_Exp/src/core/model/simulator.h"
    "/home/master01/CC_Exp/src/core/model/singleton.h"
    "/home/master01/CC_Exp/src/core/model/string.h"
    "/home/master01/CC_Exp/src/core/model/synchronizer.h"
    "/home/master01/CC_Exp/src/core/model/system-path.h"
    "/home/master01/CC_Exp/src/core/model/system-wall-clock-ms.h"
    "/home/master01/CC_Exp/src/core/model/system-wall-clock-timestamp.h"
    "/home/master01/CC_Exp/src/core/model/test.h"
    "/home/master01/CC_Exp/src/core/model/time-printer.h"
    "/home/master01/CC_Exp/src/core/model/timer-impl.h"
    "/home/master01/CC_Exp/src/core/model/timer.h"
    "/home/master01/CC_Exp/src/core/model/trace-source-accessor.h"
    "/home/master01/CC_Exp/src/core/model/traced-callback.h"
    "/home/master01/CC_Exp/src/core/model/traced-value.h"
    "/home/master01/CC_Exp/src/core/model/trickle-timer.h"
    "/home/master01/CC_Exp/src/core/model/tuple.h"
    "/home/master01/CC_Exp/src/core/model/type-id.h"
    "/home/master01/CC_Exp/src/core/model/type-name.h"
    "/home/master01/CC_Exp/src/core/model/type-traits.h"
    "/home/master01/CC_Exp/src/core/model/uinteger.h"
    "/home/master01/CC_Exp/src/core/model/unused.h"
    "/home/master01/CC_Exp/src/core/model/valgrind.h"
    "/home/master01/CC_Exp/src/core/model/vector.h"
    "/home/master01/CC_Exp/src/core/model/warnings.h"
    "/home/master01/CC_Exp/src/core/model/watchdog.h"
    "/home/master01/CC_Exp/src/core/model/realtime-simulator-impl.h"
    "/home/master01/CC_Exp/src/core/model/wall-clock-synchronizer.h"
    "/home/master01/CC_Exp/src/core/model/val-array.h"
    "/home/master01/CC_Exp/src/core/model/matrix-array.h"
    "/home/master01/CC_Exp/src/core/model/random-variable.h"
    "/home/master01/CC_Exp/build/include/ns3/config-store-config.h"
    "/home/master01/CC_Exp/build/include/ns3/core-config.h"
    "/home/master01/CC_Exp/build/include/ns3/core-module.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/master01/CC_Exp/cmake-cache/src/core/examples/cmake_install.cmake")

endif()


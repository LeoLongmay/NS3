# Install script for directory: /home/leo/PowerTCP-RAW/ns-3.39/src/core

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

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so"
         RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/leo/PowerTCP-RAW/ns-3.39/build/lib/libns3.39-core-debug.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so"
         OLD_RPATH "/home/leo/PowerTCP-RAW/ns-3.39/build/lib:"
         NEW_RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-core-debug.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/int64x64-128.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/example-as-test.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/helper/csv-reader.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/helper/event-garbage-collector.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/helper/random-variable-stream-helper.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/abort.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/ascii-file.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/ascii-test.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/assert.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/attribute-accessor-helper.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/attribute-construction-list.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/attribute-container.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/attribute-helper.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/attribute.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/boolean.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/breakpoint.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/build-profile.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/calendar-scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/callback.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/command-line.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/config.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/default-deleter.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/default-simulator-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/deprecated.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/des-metrics.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/double.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/enum.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/event-id.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/event-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/fatal-error.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/fatal-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/fd-reader.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/environment-variable.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/global-value.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/hash-fnv.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/hash-function.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/hash-murmur3.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/hash.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/heap-scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/int-to-type.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/int64x64-double.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/int64x64.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/integer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/length.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/list-scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/log-macros-disabled.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/log-macros-enabled.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/log.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/make-event.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/map-scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/math.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/names.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/node-printer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/nstime.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object-base.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object-factory.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object-map.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object-ptr-container.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object-vector.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/object.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/pair.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/pointer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/priority-queue-scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/ptr.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/random-variable-stream.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/rng-seed-manager.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/rng-stream.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/scheduler.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/show-progress.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/simple-ref-count.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/simulation-singleton.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/simulator-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/simulator.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/singleton.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/string.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/synchronizer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/system-path.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/system-wall-clock-ms.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/system-wall-clock-timestamp.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/test.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/time-printer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/timer-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/timer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/trace-source-accessor.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/traced-callback.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/traced-value.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/trickle-timer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/tuple.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/type-id.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/type-name.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/type-traits.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/uinteger.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/unused.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/valgrind.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/vector.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/warnings.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/watchdog.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/realtime-simulator-impl.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/wall-clock-synchronizer.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/val-array.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/matrix-array.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/src/core/model/random-variable.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/build/include/ns3/config-store-config.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/build/include/ns3/core-config.h"
    "/home/leo/PowerTCP-RAW/ns-3.39/build/include/ns3/core-module.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/leo/PowerTCP-RAW/ns-3.39/cmake-cache/src/core/examples/cmake_install.cmake")

endif()


# Install script for directory: /home/master01/CC_Exp/src/wifi

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
    set(CMAKE_INSTALL_CONFIG_NAME "release")
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
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so"
         RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/master01/CC_Exp/build/lib/libns3.39-wifi-optimized.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so"
         OLD_RPATH "/home/master01/CC_Exp/build/lib:::::::"
         NEW_RPATH "/usr/local/lib:$ORIGIN/:$ORIGIN/../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libns3.39-wifi-optimized.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ns3" TYPE FILE FILES
    "/home/master01/CC_Exp/src/wifi/helper/athstats-helper.h"
    "/home/master01/CC_Exp/src/wifi/helper/spectrum-wifi-helper.h"
    "/home/master01/CC_Exp/src/wifi/helper/wifi-helper.h"
    "/home/master01/CC_Exp/src/wifi/helper/wifi-mac-helper.h"
    "/home/master01/CC_Exp/src/wifi/helper/wifi-radio-energy-model-helper.h"
    "/home/master01/CC_Exp/src/wifi/helper/yans-wifi-helper.h"
    "/home/master01/CC_Exp/src/wifi/model/adhoc-wifi-mac.h"
    "/home/master01/CC_Exp/src/wifi/model/ampdu-subframe-header.h"
    "/home/master01/CC_Exp/src/wifi/model/ampdu-tag.h"
    "/home/master01/CC_Exp/src/wifi/model/amsdu-subframe-header.h"
    "/home/master01/CC_Exp/src/wifi/model/ap-wifi-mac.h"
    "/home/master01/CC_Exp/src/wifi/model/block-ack-agreement.h"
    "/home/master01/CC_Exp/src/wifi/model/block-ack-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/block-ack-type.h"
    "/home/master01/CC_Exp/src/wifi/model/block-ack-window.h"
    "/home/master01/CC_Exp/src/wifi/model/capability-information.h"
    "/home/master01/CC_Exp/src/wifi/model/channel-access-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/ctrl-headers.h"
    "/home/master01/CC_Exp/src/wifi/model/edca-parameter-set.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/default-emlsr-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-capabilities.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-configuration.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-operation.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/tid-to-link-mapping-element.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/eht-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/emlsr-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/eht/multi-link-element.h"
    "/home/master01/CC_Exp/src/wifi/model/error-rate-model.h"
    "/home/master01/CC_Exp/src/wifi/model/extended-capabilities.h"
    "/home/master01/CC_Exp/src/wifi/model/fcfs-wifi-queue-scheduler.h"
    "/home/master01/CC_Exp/src/wifi/model/frame-capture-model.h"
    "/home/master01/CC_Exp/src/wifi/model/frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/he/constant-obss-pd-algorithm.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-capabilities.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-configuration.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-operation.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/he/he-ru.h"
    "/home/master01/CC_Exp/src/wifi/model/he/mu-edca-parameter-set.h"
    "/home/master01/CC_Exp/src/wifi/model/he/mu-snr-tag.h"
    "/home/master01/CC_Exp/src/wifi/model/he/multi-user-scheduler.h"
    "/home/master01/CC_Exp/src/wifi/model/he/obss-pd-algorithm.h"
    "/home/master01/CC_Exp/src/wifi/model/he/rr-multi-user-scheduler.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-capabilities.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-configuration.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-operation.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/ht/ht-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/interference-helper.h"
    "/home/master01/CC_Exp/src/wifi/model/mac-rx-middle.h"
    "/home/master01/CC_Exp/src/wifi/model/mac-tx-middle.h"
    "/home/master01/CC_Exp/src/wifi/model/mgt-headers.h"
    "/home/master01/CC_Exp/src/wifi/model/mpdu-aggregator.h"
    "/home/master01/CC_Exp/src/wifi/model/msdu-aggregator.h"
    "/home/master01/CC_Exp/src/wifi/model/nist-error-rate-model.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/dsss-error-rate-model.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/dsss-parameter-set.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/dsss-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/dsss-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/erp-information.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/erp-ofdm-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/erp-ofdm-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/ofdm-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/non-ht/ofdm-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/non-inheritance.h"
    "/home/master01/CC_Exp/src/wifi/model/originator-block-ack-agreement.h"
    "/home/master01/CC_Exp/src/wifi/model/phy-entity.h"
    "/home/master01/CC_Exp/src/wifi/model/preamble-detection-model.h"
    "/home/master01/CC_Exp/src/wifi/model/qos-frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/qos-txop.h"
    "/home/master01/CC_Exp/src/wifi/model/qos-utils.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/aarf-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/aarfcd-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/amrr-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/aparf-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/arf-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/cara-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/constant-rate-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/ideal-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/minstrel-ht-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/minstrel-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/onoe-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/parf-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/rraa-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/rrpaa-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/rate-control/thompson-sampling-wifi-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/recipient-block-ack-agreement.h"
    "/home/master01/CC_Exp/src/wifi/model/reduced-neighbor-report.h"
    "/home/master01/CC_Exp/src/wifi/model/reference/error-rate-tables.h"
    "/home/master01/CC_Exp/src/wifi/model/simple-frame-capture-model.h"
    "/home/master01/CC_Exp/src/wifi/model/snr-tag.h"
    "/home/master01/CC_Exp/src/wifi/model/spectrum-wifi-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/ssid.h"
    "/home/master01/CC_Exp/src/wifi/model/sta-wifi-mac.h"
    "/home/master01/CC_Exp/src/wifi/model/status-code.h"
    "/home/master01/CC_Exp/src/wifi/model/supported-rates.h"
    "/home/master01/CC_Exp/src/wifi/model/table-based-error-rate-model.h"
    "/home/master01/CC_Exp/src/wifi/model/threshold-preamble-detection-model.h"
    "/home/master01/CC_Exp/src/wifi/model/txop.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-capabilities.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-configuration.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-frame-exchange-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-operation.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/vht/vht-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-ack-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-acknowledgment.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-assoc-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-bandwidth-filter.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-default-ack-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-default-assoc-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-default-protection-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-information-element.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-header.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-queue-container.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-queue-elem.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-queue-scheduler-impl.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-queue-scheduler.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-queue.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac-trailer.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mac.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mgt-header.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mode.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mpdu-type.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-mpdu.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-net-device.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-band.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-common.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-listener.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-operating-channel.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-state-helper.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy-state.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-phy.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-ppdu.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-protection-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-protection.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-psdu.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-radio-energy-model.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-remote-station-info.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-remote-station-manager.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-spectrum-phy-interface.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-spectrum-signal-parameters.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-standards.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-tx-current-model.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-tx-parameters.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-tx-timer.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-tx-vector.h"
    "/home/master01/CC_Exp/src/wifi/model/wifi-utils.h"
    "/home/master01/CC_Exp/src/wifi/model/yans-error-rate-model.h"
    "/home/master01/CC_Exp/src/wifi/model/yans-wifi-channel.h"
    "/home/master01/CC_Exp/src/wifi/model/yans-wifi-phy.h"
    "/home/master01/CC_Exp/build/include/ns3/wifi-module.h"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/master01/CC_Exp/cmake-cache/src/wifi/examples/cmake_install.cmake")

endif()


cmake_minimum_required (VERSION 3.4.1)
project (webrtc_module)

SET(CMAKE_SKIP_BUILD_RPATH  TRUE)
if(APPLE)
  message("compile for macos")
  SET(CMAKE_CXX_FLAGS " -std=c++11 -msse2  ${CMAKE_CXX_FLAGS} ")
  SET(CMAKE_C_FLAGS " -msse2 ${CMAKE_C_FLAGS}")
  ADD_DEFINITIONS(-DWEBRTC_MAC)
  ADD_DEFINITIONS(-DWEBRTC_POSIX)
elseif(MINGW)
  message("compile for windows via mingw")
  ADD_DEFINITIONS(-DWEBRTC_POSIX)
  SET(CMAKE_CXX_FLAGS " -std=c++11 -msse2  ${CMAKE_CXX_FLAGS} ")
  SET(CMAKE_C_FLAGS " -msse2 ${CMAKE_C_FLAGS}")
else()
  message("compile for regular unix")
  ADD_DEFINITIONS(-DWEBRTC_POSIX)
  SET(CMAKE_CXX_FLAGS " -std=c++11 -msse2  ${CMAKE_CXX_FLAGS} ")
  SET(CMAKE_C_FLAGS " -msse2 ${CMAKE_C_FLAGS}")
endif()
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

include_directories(./webrtc)
include_directories(./webrtc/common_audio/signal_processing/include)
include_directories(./)

set(SOURCES_WEBRTC
        webrtc/common_audio/fft4g.c
        webrtc/common_audio/ring_buffer.c
#
        webrtc/common_audio/signal_processing/randomization_functions.c
        webrtc/common_audio/signal_processing/spl_init.c
        webrtc/common_audio/signal_processing/copy_set_operations.c
        webrtc/common_audio/signal_processing/energy.c
        webrtc/common_audio/signal_processing/division_operations.c
        webrtc/common_audio/signal_processing/spl_sqrt_floor.c
        webrtc/common_audio/signal_processing/min_max_operations.c
        webrtc/common_audio/signal_processing/vector_scaling_operations.c
        webrtc/common_audio/signal_processing/cross_correlation.c
        webrtc/common_audio/signal_processing/complex_bit_reverse.c
        webrtc/common_audio/signal_processing/get_scaling_square.c
        webrtc/common_audio/signal_processing/spl_init.c
        webrtc/common_audio/signal_processing/real_fft.c
        webrtc/common_audio/signal_processing/complex_fft.c
        webrtc/common_audio/signal_processing/downsample_fast.c
#
        webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c
        webrtc/modules/audio_processing/utility/delay_estimator.c
#
        webrtc/modules/audio_processing/aecm/aecm_core.c
        webrtc/modules/audio_processing/aecm/aecm_core_c.c
        webrtc/modules/audio_processing/aecm/aecm_core.h
        webrtc/modules/audio_processing/aecm/aecm_defines.h
        webrtc/modules/audio_processing/aecm/echo_control_mobile.c
        webrtc/modules/audio_processing/aecm/echo_control_mobile.h
#
        webrtc/modules/audio_processing/ns/noise_suppression_x.c
        webrtc/modules/audio_processing/ns/noise_suppression_x.h
        webrtc/modules/audio_processing/ns/nsx_core.c
        webrtc/modules/audio_processing/ns/nsx_core_c.c
        webrtc/modules/audio_processing/ns/nsx_core.h
        webrtc/modules/audio_processing/ns/nsx_defines.h
        webrtc/modules/audio_processing/ns/windows_private.h
    )


add_library(webrtc6 STATIC ${SOURCES_WEBRTC})
target_link_libraries(webrtc6)

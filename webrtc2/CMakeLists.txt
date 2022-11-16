cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_BUILD_TYPE Debug)
include_directories(./)
add_definitions(-DWEBRTC_POSIX)
add_library(
        aec
        STATIC
        webrtc/modules/audio_processing/aec/echo_cancellation.c
        webrtc/common_audio/ring_buffer.c
        webrtc/common_audio/signal_processing/randomization_functions.c
        webrtc/modules/audio_processing/utility/delay_estimator_wrapper.c
        webrtc/modules/audio_processing/utility/delay_estimator.c
        webrtc/modules/audio_processing/aec/aec_core.c
#        webrtc/modules/audio_processing/aec/aec_core_neon.c
        webrtc/modules/audio_processing/aec/aec_rdft.c
#        webrtc/modules/audio_processing/aec/aec_rdft_neon.c
        webrtc/modules/audio_processing/aec/aec_resampler.c
        webrtc/system_wrappers/interface/cpu_features_wrapper.h
    )



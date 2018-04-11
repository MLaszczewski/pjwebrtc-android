cmake_minimum_required(VERSION 3.4.1)
project(pjwebrtc)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-D__ANDROID)
message("ANDROID ABI: ${ANDROID_ABI}")
set(ABI ${ANDROID_ABI})

set(PJ_DIR "/Users/m8/sip/android/pjproject-${ABI}")

include_directories(
    ${PJ_DIR}/pjlib/include
    ${PJ_DIR}/pjlib-util/include
    ${PJ_DIR}/pjmedia/include
    ${PJ_DIR}/pjnath/include)

set(PJ_LIB_SUFIX ${ABI}-unknown-linux-android.a)

if(${ABI} STREQUAL "armeabi-v7a")
  set(PJ_LIB_SUFIX "arm-unknown-linux-androideabi.a")
endif()

set(PJ_LIBS

    "${PJ_DIR}/pjnath/lib/libpjnath-${PJ_LIB_SUFIX}"

    "${PJ_DIR}/pjmedia/lib/libpjmedia-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/pjmedia/lib/libpjmedia-audiodev-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/pjmedia/lib/libpjmedia-videodev-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/pjmedia/lib/libpjmedia-codec-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/pjmedia/lib/libpjsdp-${PJ_LIB_SUFIX}"

    "${PJ_DIR}/third_party/lib/libg7221codec-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libresample-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libwebrtc-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libgsmcodec-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libspeex-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libyuv-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libilbccodec-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/third_party/lib/libsrtp-${PJ_LIB_SUFIX}"

    "${PJ_DIR}/pjlib-util/lib/libpjlib-util-${PJ_LIB_SUFIX}"
    "${PJ_DIR}/pjlib/lib/libpj-${PJ_LIB_SUFIX}"
)

set(OPENSSL_DIR "/Users/m8/sip/android/openssl_for_ios_and_android/output/android/openssl-${ABI}/")

set(OPENSSL_LIBS
    "${OPENSSL_DIR}/lib/libssl.a"
    "${OPENSSL_DIR}/lib/libcrypto.a"
)

set(MAIN_DIR src/main/cpp/)
set(LIB_DIR ${MAIN_DIR}/lib)

aux_source_directory(${LIB_DIR}/promise/src PROMISE_SRC_LIST)
include_directories(${LIB_DIR}/promise/include)

aux_source_directory(${LIB_DIR}/wsxx/src WSXX_SRC_LIST)
include_directories(${LIB_DIR}/wsxx/include)

include_directories(${LIB_DIR}/json)

aux_source_directory(${MAIN_DIR} MAIN_SRC_LIST)

set(SOURCES ${MAIN_SRC_LIST} ${PROMISE_SRC_LIST} ${WSXX_SRC_LIST})

add_library( pjwebrtc SHARED ${SOURCES} )

find_library(log-lib log)

target_link_libraries( # Specifies the target library.
                       pjwebrtc

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} ${PJ_LIBS} ${OPENSSL_LIBS} )



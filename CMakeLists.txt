cmake_minimum_required(VERSION 3.16)
project(manalysis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SOUNDTOUCH REQUIRED soundtouch)
pkg_check_modules(KEYFINDER  REQUIRED libkeyfinder)
pkg_check_modules(EBUR128    REQUIRED libebur128)
pkg_check_modules(AVCODEC    REQUIRED libavcodec)
pkg_check_modules(AVFORMAT   REQUIRED libavformat)
pkg_check_modules(AVUTIL     REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

add_executable(manalysis
    src/main.cpp
    src/AudioDecoder.cpp
    src/BpmAnalyzer.cpp
    src/KeyAnalyzer.cpp
    src/GainAnalyzer.cpp
)

target_include_directories(manalysis PRIVATE
    ${SOUNDTOUCH_INCLUDE_DIRS}
    ${KEYFINDER_INCLUDE_DIRS}
    ${EBUR128_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
)

target_link_libraries(manalysis PRIVATE
    ${SOUNDTOUCH_LIBRARIES}
    ${KEYFINDER_LIBRARIES}
    ${EBUR128_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

target_compile_options(manalysis PRIVATE -Wall -Wextra -O2)

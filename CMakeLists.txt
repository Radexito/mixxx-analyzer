cmake_minimum_required(VERSION 3.16)
project(mixxx-analyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    find_package(GTest REQUIRED)
endif()

if(WIN32)
    # ── Windows: locate deps via CMAKE_PREFIX_PATH (no pkg-config needed) ──────
    # Point cmake: -DCMAKE_PREFIX_PATH="<ffmpeg_root>;C:/vcpkg/installed/x64-windows"
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h REQUIRED)
    find_library(AVCODEC_LIB    avcodec    REQUIRED)
    find_library(AVFORMAT_LIB   avformat   REQUIRED)
    find_library(AVUTIL_LIB     avutil     REQUIRED)
    find_library(SWRESAMPLE_LIB swresample REQUIRED)
    find_path(EBUR128_INCLUDE_DIR ebur128.h REQUIRED)
    find_library(EBUR128_LIB ebur128 REQUIRED)

    set(ANALYSIS_WIN_INCLUDES ${FFMPEG_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
    set(ANALYSIS_LIBS
        qm-dsp
        ${EBUR128_LIB}
        ${AVCODEC_LIB} ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWRESAMPLE_LIB}
    )
else()
    # ── Unix/macOS: use pkg-config ───────────────────────────────────────────────
    if(NOT PkgConfig_FOUND)
        message(FATAL_ERROR "pkg-config not found; required on non-Windows platforms")
    endif()
    pkg_check_modules(EBUR128    REQUIRED IMPORTED_TARGET libebur128)
    pkg_check_modules(AVCODEC    REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVFORMAT   REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVUTIL     REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

    set(ANALYSIS_LIBS
        qm-dsp
        PkgConfig::EBUR128
        PkgConfig::AVCODEC PkgConfig::AVFORMAT PkgConfig::AVUTIL PkgConfig::SWRESAMPLE
    )
endif()

# ── qm-dsp (BPM tempo tracker + key detector, copied from Mixxx) ─────────────
set(QM_DSP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qm-dsp)

add_library(qm-dsp STATIC
    ${QM_DSP_DIR}/dsp/onsets/DetectionFunction.cpp
    ${QM_DSP_DIR}/dsp/onsets/PeakPicking.cpp
    ${QM_DSP_DIR}/dsp/phasevocoder/PhaseVocoder.cpp
    ${QM_DSP_DIR}/dsp/signalconditioning/DFProcess.cpp
    ${QM_DSP_DIR}/dsp/signalconditioning/Filter.cpp
    ${QM_DSP_DIR}/dsp/signalconditioning/FiltFilt.cpp
    ${QM_DSP_DIR}/dsp/signalconditioning/Framer.cpp
    ${QM_DSP_DIR}/dsp/tempotracking/TempoTrackV2.cpp
    ${QM_DSP_DIR}/dsp/transforms/FFT.cpp
    ${QM_DSP_DIR}/dsp/keydetection/GetKeyMode.cpp
    ${QM_DSP_DIR}/dsp/chromagram/Chromagram.cpp
    ${QM_DSP_DIR}/dsp/chromagram/ConstantQ.cpp
    ${QM_DSP_DIR}/dsp/rateconversion/Decimator.cpp
    ${QM_DSP_DIR}/base/Pitch.cpp
    ${QM_DSP_DIR}/maths/MathUtilities.cpp
    ${QM_DSP_DIR}/ext/kissfft/kiss_fft.c
    ${QM_DSP_DIR}/ext/kissfft/tools/kiss_fftr.c
)
target_include_directories(qm-dsp PUBLIC ${QM_DSP_DIR})
target_compile_definitions(qm-dsp PRIVATE kiss_fft_scalar=double)
if(MSVC)
    target_compile_definitions(qm-dsp PRIVATE _USE_MATH_DEFINES NOMINMAX)
    target_compile_options(qm-dsp PRIVATE /W0 /O2)
else()
    target_compile_options(qm-dsp PRIVATE -O2 -w) # suppress qm-dsp warnings
endif()

# ── Shared analysis sources ───────────────────────────────────────────────────
set(ANALYSIS_SOURCES
    src/AudioDecoder.cpp
    src/DownmixAndOverlapHelper.cpp
    src/QmBpmAnalyzer.cpp
    src/QmKeyAnalyzer.cpp
    src/GainAnalyzer.cpp
    src/SilenceAnalyzer.cpp
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(mixxx-analyzer src/main.cpp ${ANALYSIS_SOURCES})
target_link_libraries(mixxx-analyzer PRIVATE ${ANALYSIS_LIBS})
if(WIN32 AND DEFINED ANALYSIS_WIN_INCLUDES)
    target_include_directories(mixxx-analyzer PRIVATE ${ANALYSIS_WIN_INCLUDES})
endif()
if(MSVC)
    target_compile_options(mixxx-analyzer PRIVATE /W3 /O2)
    target_compile_definitions(mixxx-analyzer PRIVATE _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(mixxx-analyzer PRIVATE -Wall -Wextra -O2)
endif()
if(UNIX AND NOT APPLE)
    target_link_options(mixxx-analyzer PRIVATE -static-libstdc++ -static-libgcc)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
if(BUILD_TESTING)
    enable_testing()

    add_executable(mixxx-analyzer-test tests/analysis_test.cpp ${ANALYSIS_SOURCES})
    target_include_directories(mixxx-analyzer-test PRIVATE src
        $<$<BOOL:${WIN32}>:${ANALYSIS_WIN_INCLUDES}>)
    target_link_libraries(mixxx-analyzer-test PRIVATE ${ANALYSIS_LIBS} GTest::gtest GTest::gtest_main)
    target_compile_options(mixxx-analyzer-test PRIVATE -Wall -O2)
    target_compile_definitions(mixxx-analyzer-test PRIVATE
        MANALYSIS_TEST_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/assets")

    add_test(NAME AnalysisTest COMMAND mixxx-analyzer-test)
endif()

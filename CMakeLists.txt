cmake_minimum_required(VERSION 3.16)
project(mixxx-analyzer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
find_package(GTest REQUIRED)

pkg_check_modules(EBUR128    REQUIRED libebur128)
pkg_check_modules(AVCODEC    REQUIRED libavcodec)
pkg_check_modules(AVFORMAT   REQUIRED libavformat)
pkg_check_modules(AVUTIL     REQUIRED libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)

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
target_compile_options(qm-dsp PRIVATE -O2 -w) # suppress qm-dsp warnings

# ── Shared analysis sources ───────────────────────────────────────────────────
set(ANALYSIS_SOURCES
    src/AudioDecoder.cpp
    src/DownmixAndOverlapHelper.cpp
    src/QmBpmAnalyzer.cpp
    src/QmKeyAnalyzer.cpp
    src/GainAnalyzer.cpp
    src/SilenceAnalyzer.cpp
)

set(ANALYSIS_INCLUDE_DIRS
    ${EBUR128_INCLUDE_DIRS}
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
    ${SWRESAMPLE_INCLUDE_DIRS}
)

set(ANALYSIS_LIBS
    qm-dsp
    ${EBUR128_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(mixxx-analyzer src/main.cpp ${ANALYSIS_SOURCES})
target_include_directories(mixxx-analyzer PRIVATE ${ANALYSIS_INCLUDE_DIRS})
target_link_libraries(mixxx-analyzer PRIVATE ${ANALYSIS_LIBS})
target_compile_options(mixxx-analyzer PRIVATE -Wall -Wextra -O2)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(mixxx-analyzer-test tests/analysis_test.cpp ${ANALYSIS_SOURCES})
target_include_directories(mixxx-analyzer-test PRIVATE ${ANALYSIS_INCLUDE_DIRS} src)
target_link_libraries(mixxx-analyzer-test PRIVATE ${ANALYSIS_LIBS} GTest::gtest GTest::gtest_main)
target_compile_options(mixxx-analyzer-test PRIVATE -Wall -O2)
target_compile_definitions(mixxx-analyzer-test PRIVATE
    MANALYSIS_TEST_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/assets")

add_test(NAME AnalysisTest COMMAND mixxx-analyzer-test)

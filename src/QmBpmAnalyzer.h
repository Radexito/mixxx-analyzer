#pragma once

#include <memory>
#include <vector>

#include "DownmixAndOverlapHelper.h"

class DetectionFunction;

// Detects BPM using the Queen Mary tempo tracker — exact port of
// mixxx::AnalyzerQueenMaryBeats (AnalyzerQueenMaryBeats.cpp).
// Returns beat positions in frames; call averageBpm() to convert.
class QmBpmAnalyzer {
  public:
    explicit QmBpmAnalyzer(int sampleRate);
    ~QmBpmAnalyzer();

    // Feed interleaved stereo float32 samples (numFrames * 2 floats).
    void feed(const float* interleavedStereo, int numFrames);

    // Finalises analysis and returns detected BPM (0 if undetected).
    float result();

    // Returns beat positions in seconds (populated after result() is called).
    std::vector<double> beatFramesSecs(int sampleRate) const;

  private:
    int m_sampleRate;
    int m_windowSize;
    int m_stepSizeFrames;

    std::unique_ptr<DetectionFunction> m_pDetectionFunction;
    DownmixAndOverlapHelper m_helper;
    std::vector<double> m_detectionResults;
    std::vector<double> m_beats;       // beat positions in df-increment units
    std::vector<double> m_beatFrames;  // beat positions in frames (set by result())
};

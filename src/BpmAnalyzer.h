#pragma once

#include <memory>

namespace soundtouch {
class BPMDetect;
}

// Estimates BPM from a stream of interleaved float32 stereo samples
// using the SoundTouch BPMDetect algorithm (internally downmixed to mono).
class BpmAnalyzer {
  public:
    explicit BpmAnalyzer(int sampleRate);
    ~BpmAnalyzer();

    // Feed interleaved stereo float samples (numFrames * 2 floats).
    void feed(const float* interleavedStereo, int numFrames);

    // Returns detected BPM. Call after all audio has been fed.
    float result() const;

  private:
    std::unique_ptr<soundtouch::BPMDetect> m_detector;
    int m_sampleRate;
};

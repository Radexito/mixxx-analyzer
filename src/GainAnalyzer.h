#pragma once

#include <ebur128.h>

// Measures integrated loudness and ReplayGain from a stream of
// interleaved float32 stereo samples using libebur128 (EBU R128).
class GainAnalyzer {
  public:
    struct Result {
        double lufs;        // Integrated loudness in LUFS
        double replayGain;  // ReplayGain 2.0 dB value (-18 LUFS reference)
    };

    explicit GainAnalyzer(int sampleRate);
    ~GainAnalyzer();

    // Non-copyable
    GainAnalyzer(const GainAnalyzer&) = delete;
    GainAnalyzer& operator=(const GainAnalyzer&) = delete;

    // Feed interleaved stereo float samples (numFrames * 2 floats).
    void feed(const float* interleavedStereo, int numFrames);

    // Returns measured loudness. Call after all audio has been fed.
    // Returns false if measurement failed (e.g. silence).
    bool result(Result& out) const;

  private:
    ebur128_state* m_state;
};

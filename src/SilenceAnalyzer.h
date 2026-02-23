#pragma once

// Detects the first and last non-silent frame in an audio stream.
// Uses the same -60 dB threshold (0.001f) as Mixxx's AnalyzerSilence.
class SilenceAnalyzer {
  public:
    struct Result {
        double introSecs;  // time of first non-silent sample
        double outroSecs;  // time of last non-silent sample
    };

    explicit SilenceAnalyzer(int sampleRate, int channels);

    // Feed interleaved float samples (numFrames * channels floats).
    void feed(const float* samples, int numFrames);

    // Call after all audio has been fed.
    Result result() const;

  private:
    static constexpr float kThreshold = 0.001f;  // -60 dB

    int m_sampleRate;
    int m_channels;
    long long m_framesProcessed;
    long long m_signalStart;  // -1 = not found yet
    long long m_signalEnd;
};

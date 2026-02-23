#pragma once

#include <memory>
#include <string>

namespace KeyFinder {
class KeyFinder;
class Workspace;
}  // namespace KeyFinder

// Detects musical key from a stream of interleaved float32 stereo samples
// using libkeyfinder.
class KeyAnalyzer {
  public:
    struct Result {
        std::string key;      // e.g. "C major"
        std::string camelot;  // e.g. "8B"
    };

    explicit KeyAnalyzer(int sampleRate);
    ~KeyAnalyzer();

    // Feed interleaved stereo float samples (numFrames * 2 floats).
    void feed(const float* interleavedStereo, int numFrames);

    // Returns detected key. Call after all audio has been fed.
    Result result();

  private:
    std::unique_ptr<KeyFinder::KeyFinder> m_kf;
    std::unique_ptr<KeyFinder::Workspace> m_workspace;
    int m_sampleRate;
};

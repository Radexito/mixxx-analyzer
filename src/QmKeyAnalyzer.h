#pragma once

#include <memory>
#include <string>
#include <vector>

#include "DownmixAndOverlapHelper.h"

class GetKeyMode;

// 1:1 port of Mixxx's AnalyzerQueenMaryKey + calculateGlobalKey logic.
// Uses qm-dsp's GetKeyMode (Chromagram + HPCP + key profile correlation).
class QmKeyAnalyzer {
  public:
    struct Result {
        // Raw key index matching Mixxx's ChromaticKey proto enum:
        //   0=INVALID, 1=C_MAJOR..12=B_MAJOR, 13=C_MINOR..24=B_MINOR
        int chromaticKey{0};
        std::string key;     // e.g. "D minor"
        std::string camelot; // e.g. "7A"
    };

    explicit QmKeyAnalyzer(int sampleRate);
    ~QmKeyAnalyzer();

    void feed(const float* stereoFrames, int numFrames);
    Result result();

  private:
    std::unique_ptr<GetKeyMode> m_pKeyMode;
    DownmixAndOverlapHelper m_helper;
    size_t m_currentFrame{0};
    int m_totalFrames{0};

    // Accumulated key changes: (chromaticKey, startFrame)
    std::vector<std::pair<int, double>> m_keyChanges;
    int m_prevKey{0};
};

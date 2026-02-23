#include "KeyAnalyzer.h"

#include <keyfinder/audiodata.h>
#include <keyfinder/constants.h>
#include <keyfinder/keyfinder.h>
#include <keyfinder/workspace.h>

namespace {

struct KeyInfo {
    const char* name;
    const char* camelot;
};

// Indexed by KeyFinder::key_t enum values (0..23), SILENCE=24
constexpr KeyInfo kKeyTable[] = {
    {"A major",  "11B"}, // A_MAJOR      = 0
    {"A minor",  "8A"},  // A_MINOR      = 1
    {"Bb major", "6B"},  // B_FLAT_MAJOR = 2
    {"Bb minor", "3A"},  // B_FLAT_MINOR = 3
    {"B major",  "1B"},  // B_MAJOR      = 4
    {"B minor",  "10A"}, // B_MINOR      = 5
    {"C major",  "8B"},  // C_MAJOR      = 6
    {"C minor",  "5A"},  // C_MINOR      = 7
    {"Db major", "3B"},  // D_FLAT_MAJOR = 8
    {"Db minor", "12A"}, // D_FLAT_MINOR = 9
    {"D major",  "10B"}, // D_MAJOR      = 10
    {"D minor",  "7A"},  // D_MINOR      = 11
    {"Eb major", "5B"},  // E_FLAT_MAJOR = 12
    {"Eb minor", "2A"},  // E_FLAT_MINOR = 13
    {"E major",  "12B"}, // E_MAJOR      = 14
    {"E minor",  "9A"},  // E_MINOR      = 15
    {"F major",  "7B"},  // F_MAJOR      = 16
    {"F minor",  "4A"},  // F_MINOR      = 17
    {"F# major", "2B"},  // G_FLAT_MAJOR = 18
    {"F# minor", "11A"}, // G_FLAT_MINOR = 19
    {"G major",  "9B"},  // G_MAJOR      = 20
    {"G minor",  "6A"},  // G_MINOR      = 21
    {"Ab major", "4B"},  // A_FLAT_MAJOR = 22
    {"Ab minor", "1A"},  // A_FLAT_MINOR = 23
};

} // namespace

KeyAnalyzer::KeyAnalyzer(int sampleRate)
        : m_kf(std::make_unique<KeyFinder::KeyFinder>()),
          m_workspace(std::make_unique<KeyFinder::Workspace>()),
          m_sampleRate(sampleRate) {
}

KeyAnalyzer::~KeyAnalyzer() = default;

void KeyAnalyzer::feed(const float* interleavedStereo, int numFrames) {
    KeyFinder::AudioData chunk;
    chunk.setChannels(2);
    chunk.setFrameRate(static_cast<unsigned int>(m_sampleRate));
    chunk.addToSampleCount(numFrames * 2);

    for (int f = 0; f < numFrames; ++f) {
        chunk.setSampleByFrame(f, 0, static_cast<double>(interleavedStereo[f * 2]));
        chunk.setSampleByFrame(f, 1, static_cast<double>(interleavedStereo[f * 2 + 1]));
    }

    m_kf->progressiveChromagram(chunk, *m_workspace);
}

KeyAnalyzer::Result KeyAnalyzer::result() {
    m_kf->finalChromagram(*m_workspace);
    KeyFinder::key_t key = m_kf->keyOfChromagram(*m_workspace);

    if (key == KeyFinder::SILENCE || key >= static_cast<int>(std::size(kKeyTable))) {
        return {"Silence / Unknown", "-"};
    }
    return {kKeyTable[key].name, kKeyTable[key].camelot};
}

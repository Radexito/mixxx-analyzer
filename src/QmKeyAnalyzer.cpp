// QmKeyAnalyzer.cpp — 1:1 port of Mixxx's AnalyzerQueenMaryKey + KeyUtils::calculateGlobalKey
//
// Reference Mixxx sources:
//   src/analyzer/plugins/analyzerqueenmarykey.cpp
//   src/track/keyutils.cpp  (calculateGlobalKey)
//   src/track/keyfactory.cpp (makePreferredKeys)
//   src/proto/keys.proto    (ChromaticKey enum)

#include "QmKeyAnalyzer.h"

#include <dsp/keydetection/GetKeyMode.h>

#include <algorithm>
#include <map>
#include <stdexcept>

static constexpr int kTuningFrequencyHz = 440;

// ── Camelot wheel mapping ────────────────────────────────────────────────────
// ChromaticKey enum (from Mixxx's keys.proto):
//   1=C_MAJOR..12=B_MAJOR, 13=C_MINOR..24=B_MINOR
// Camelot wheel (major=B suffix, minor=A suffix):
//   C major=8B, Db=3B, D=10B, Eb=5B, E=12B, F=7B, F#=2B, G=9B, Ab=4B, A=11B, Bb=6B, B=1B
//   C minor=5A, C#=12A, D=7A, Eb=2A, E=9A, F=4A, F#=11A, G=6A, Ab=1A, A=8A, Bb=3A, B=10A

struct KeyInfo {
    const char* name;
    const char* camelot;
};

// Index 0 = INVALID, 1..12 = major, 13..24 = minor
static const KeyInfo kKeyInfo[25] = {
    {"(none)", ""},       //  0 INVALID
    {"C major", "8B"},    //  1
    {"Db major", "3B"},   //  2
    {"D major", "10B"},   //  3
    {"Eb major", "5B"},   //  4
    {"E major", "12B"},   //  5
    {"F major", "7B"},    //  6
    {"F# major", "2B"},   //  7
    {"G major", "9B"},    //  8
    {"Ab major", "4B"},   //  9
    {"A major", "11B"},   // 10
    {"Bb major", "6B"},   // 11
    {"B major", "1B"},    // 12
    {"C minor", "5A"},    // 13
    {"C# minor", "12A"},  // 14
    {"D minor", "7A"},    // 15
    {"Eb minor", "2A"},   // 16
    {"E minor", "9A"},    // 17
    {"F minor", "4A"},    // 18
    {"F# minor", "11A"},  // 19
    {"G minor", "6A"},    // 20
    {"Ab minor", "1A"},   // 21
    {"A minor", "8A"},    // 22
    {"Bb minor", "3A"},   // 23
    {"B minor", "10A"},   // 24
};

// ── Constructor ──────────────────────────────────────────────────────────────

QmKeyAnalyzer::QmKeyAnalyzer(int sampleRate) {
    GetKeyMode::Config cfg(static_cast<double>(sampleRate), kTuningFrequencyHz);
    m_pKeyMode = std::make_unique<GetKeyMode>(cfg);

    const size_t windowSize = static_cast<size_t>(m_pKeyMode->getBlockSize());
    const size_t stepSize = static_cast<size_t>(m_pKeyMode->getHopSize());

    m_helper.initialize(windowSize, stepSize, [this](double* pWindow, size_t) -> bool {
        int key = m_pKeyMode->process(pWindow);
        // key range is 0-24 (0 = no key detected)
        if (key < 0 || key > 24)
            key = 0;
        if (key != m_prevKey) {
            m_keyChanges.push_back({key, static_cast<double>(m_currentFrame)});
            m_prevKey = key;
        }
        return true;
    });
}

QmKeyAnalyzer::~QmKeyAnalyzer() = default;

// ── Feed ─────────────────────────────────────────────────────────────────────

void QmKeyAnalyzer::feed(const float* stereoFrames, int numFrames) {
    m_currentFrame += static_cast<size_t>(numFrames);
    m_totalFrames += numFrames;
    m_helper.processStereoSamples(stereoFrames, numFrames * 2);
}

// ── Result ───────────────────────────────────────────────────────────────────

QmKeyAnalyzer::Result QmKeyAnalyzer::result() {
    m_helper.finalize();
    m_pKeyMode.reset();

    if (m_keyChanges.empty()) {
        return {0, kKeyInfo[0].name, kKeyInfo[0].camelot};
    }

    // calculateGlobalKey: pick key with greatest total frame-duration (Mixxx port)
    int globalKey = 0;
    if (m_keyChanges.size() == 1) {
        globalKey = m_keyChanges[0].first;
    } else {
        std::map<int, double> histogram;
        for (size_t i = 0; i < m_keyChanges.size(); ++i) {
            int k = m_keyChanges[i].first;
            double start = m_keyChanges[i].second;
            double end = (i + 1 < m_keyChanges.size()) ? m_keyChanges[i + 1].second
                                                       : static_cast<double>(m_totalFrames);
            histogram[k] += (end - start);
        }
        double maxDuration = 0;
        for (auto& [k, dur] : histogram) {
            if (dur > maxDuration) {
                maxDuration = dur;
                globalKey = k;
            }
        }
    }

    if (globalKey < 0 || globalKey > 24)
        globalKey = 0;
    return {globalKey, kKeyInfo[globalKey].name, kKeyInfo[globalKey].camelot};
}

#pragma once

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "KeyAnalyzer.h"
#include "QmBpmAnalyzer.h"

struct TrackResult {
    float bpm;
    KeyAnalyzer::Result key;
    GainAnalyzer::Result gain;
    bool gainOk;
};

inline TrackResult analyzeTrack(const std::string& path) {
    std::unique_ptr<QmBpmAnalyzer> bpm;
    std::unique_ptr<KeyAnalyzer> key;
    std::unique_ptr<GainAnalyzer> gain;
    bool initialized = false;

    std::string err;
    bool ok = AudioDecoder::decode(
        path,
        [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
            if (!initialized) {
                bpm = std::make_unique<QmBpmAnalyzer>(info.sampleRate);
                key = std::make_unique<KeyAnalyzer>(info.sampleRate);
                gain = std::make_unique<GainAnalyzer>(info.sampleRate);
                initialized = true;
            }
            bpm->feed(samples, numFrames);
            key->feed(samples, numFrames);
            gain->feed(samples, numFrames);
        },
        err);

    EXPECT_TRUE(ok) << "Decode failed: " << err;
    EXPECT_TRUE(initialized) << "No audio data: " << path;

    TrackResult r{};
    if (ok && initialized) {
        r.bpm = bpm->result();
        r.key = key->result();
        r.gainOk = gain->result(r.gain);
    }
    return r;
}

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "KeyAnalyzer.h"
#include "QmBpmAnalyzer.h"

// ── Helper ─────────────────────────────────────────────────────────────────

struct TrackResult {
    float bpm;
    KeyAnalyzer::Result key;
    GainAnalyzer::Result gain;
    bool gainOk;
};

static TrackResult analyzeTrack(const std::string& path) {
    std::unique_ptr<QmBpmAnalyzer> bpm;
    std::unique_ptr<KeyAnalyzer> key;
    std::unique_ptr<GainAnalyzer> gain;
    bool initialized = false;

    std::string err;
    bool ok = AudioDecoder::decode(
            path,
            [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
                if (!initialized) {
                    bpm  = std::make_unique<QmBpmAnalyzer>(info.sampleRate);
                    key  = std::make_unique<KeyAnalyzer>(info.sampleRate);
                    gain = std::make_unique<GainAnalyzer>(info.sampleRate);
                    initialized = true;
                }
                bpm->feed(samples, numFrames);
                key->feed(samples, numFrames);
                gain->feed(samples, numFrames);
            },
            err);

    EXPECT_TRUE(ok) << "Decode failed: " << err;
    EXPECT_TRUE(initialized) << "No audio data found in: " << path;

    TrackResult r{};
    if (ok && initialized) {
        r.bpm    = bpm->result();
        r.key    = key->result();
        r.gainOk = gain->result(r.gain);
    }
    return r;
}

// ── Fixtures ────────────────────────────────────────────────────────────────

// Tolerance for BPM comparison (±1.0 BPM)
constexpr float kBpmTolerance = 1.0f;

// ── Tests ────────────────────────────────────────────────────────────────────

TEST(AnalysisTest, BellaCiao_BpmAndKey) {
    const std::string path =
            "/home/radexito/Music/becky-uro/"
            "Bella Ciao (Hard Techno Remix) [InZ44_NC2a4].mp3";

    auto r = analyzeTrack(path);

    // Mixxx reference: 165 BPM, key 7A (D minor)
    EXPECT_NEAR(r.bpm, 165.0f, kBpmTolerance)
            << "BPM mismatch for Bella Ciao. Got: " << r.bpm;
    EXPECT_EQ(r.key.camelot, "7A")
            << "Key mismatch for Bella Ciao. Got: " << r.key.key << " (" << r.key.camelot << ")";
}

TEST(AnalysisTest, Micrograms1200_BpmAndKey) {
    const std::string path =
            "/home/radexito/Music/psytrance/"
            "1200 Micrograms - The Apocalypse.mp3";

    auto r = analyzeTrack(path);

    // Mixxx reference: 147 BPM, key 7A (D minor)
    EXPECT_NEAR(r.bpm, 147.0f, kBpmTolerance)
            << "BPM mismatch for 1200 Micrograms. Got: " << r.bpm;
    EXPECT_EQ(r.key.camelot, "7A")
            << "Key mismatch for 1200 Micrograms. Got: " << r.key.key << " (" << r.key.camelot << ")";
}

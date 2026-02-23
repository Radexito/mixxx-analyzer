#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "QmBpmAnalyzer.h"
#include "QmKeyAnalyzer.h"

// ── Asset directory ──────────────────────────────────────────────────────────
// Resolved relative to this source file at compile time via CMake.
// Can be overridden at runtime with MANALYSIS_TEST_ASSETS env var.

#ifndef MANALYSIS_TEST_ASSETS_DIR
#define MANALYSIS_TEST_ASSETS_DIR ""
#endif

static std::string assetsDir() {
    const char* env = std::getenv("MANALYSIS_TEST_ASSETS");
    if (env && env[0])
        return std::string(env) + "/";
    std::string d = MANALYSIS_TEST_ASSETS_DIR;
    if (!d.empty() && d.back() != '/')
        d += '/';
    return d;
}

// ── Helper ───────────────────────────────────────────────────────────────────

struct TrackResult {
    float bpm;
    QmKeyAnalyzer::Result key;
    GainAnalyzer::Result gain;
    bool gainOk;
};

static TrackResult analyzeTrack(const std::string& path) {
    std::unique_ptr<QmBpmAnalyzer> bpm;
    std::unique_ptr<QmKeyAnalyzer> key;
    std::unique_ptr<GainAnalyzer> gain;
    bool initialized = false;

    std::string err;
    bool ok = AudioDecoder::decode(
        path,
        [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
            if (!initialized) {
                bpm = std::make_unique<QmBpmAnalyzer>(info.sampleRate);
                key = std::make_unique<QmKeyAnalyzer>(info.sampleRate);
                gain = std::make_unique<GainAnalyzer>(info.sampleRate);
                initialized = true;
            }
            bpm->feed(samples, numFrames);
            key->feed(samples, numFrames);
            gain->feed(samples, numFrames);
        },
        err);

    EXPECT_TRUE(ok) << "Decode failed: " << err;
    EXPECT_TRUE(initialized) << "No audio data in: " << path;

    TrackResult r{};
    if (ok && initialized) {
        r.bpm = bpm->result();
        r.key = key->result();
        r.gainOk = gain->result(r.gain);
    }
    return r;
}

// Skips the test if path does not exist on the filesystem.
#define SKIP_IF_MISSING(path)                                                             \
    do {                                                                                  \
        if (!std::filesystem::exists(path)) {                                             \
            GTEST_SKIP() << "Asset not found (run tests/download_assets.sh): " << (path); \
        }                                                                                 \
    } while (0)

constexpr float kBpmTol = 1.0f;

// ── Audionautix tests ────────────────────────────────────────────────────────
// Royalty-free tracks from Audionautix (CC BY 4.0, Jason Shaw).
// Download with:  bash tests/download_assets.sh
//
// TODO: run Mixxx on each file, fill in expected BPM + Camelot, then rename
// DISABLED_Audionautix → Audionautix to enable.

struct AudionautixParams {
    const char* file;
    float bpm;
    const char* camelot;
};

class DISABLED_AudionautixTest : public ::testing::TestWithParam<AudionautixParams> {};

TEST_P(DISABLED_AudionautixTest, BpmAndKey) {
    std::string path = assetsDir() + GetParam().file;
    SKIP_IF_MISSING(path);
    auto r = analyzeTrack(path);
    EXPECT_NEAR(r.bpm, GetParam().bpm, kBpmTol) << GetParam().file << " BPM: got " << r.bpm;
    EXPECT_EQ(r.key.camelot, GetParam().camelot) << GetParam().file << " Key: got " << r.key.key;
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(
        Audionautix,
        DISABLED_AudionautixTest,
        ::testing::Values(
                // TODO: fill in expected values after analyzing with Mixxx
                AudionautixParams{"FallingSky.mp3",       0.0f, "?"},
                AudionautixParams{"LatinHouseBed.mp3",    0.0f, "?"},
                AudionautixParams{"BanjoHop.mp3",         0.0f, "?"},
                AudionautixParams{"BeBop25.mp3",          0.0f, "?"},
                AudionautixParams{"Boom.mp3",             0.0f, "?"},
                AudionautixParams{"NightRave.mp3",        0.0f, "?"},
                AudionautixParams{"Sk8board.mp3",         0.0f, "?"},
                AudionautixParams{"AllGoodInTheWood.mp3", 0.0f, "?"},
                AudionautixParams{"DanceDubber.mp3",      0.0f, "?"},
                AudionautixParams{"DogHouse.mp3",         0.0f, "?"},
                AudionautixParams{"Don'tStop.mp3",        0.0f, "?"}),
        [](const ::testing::TestParamInfo<AudionautixParams>& info) {
            std::string name = info.param.file;
            for (char& c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
            return name;
        });
// clang-format on

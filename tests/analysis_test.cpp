#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "QmBpmAnalyzer.h"
#include "QmKeyAnalyzer.h"

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

struct TrackResult {
    float bpm;
    QmKeyAnalyzer::Result key;
    GainAnalyzer::Result gain;
    bool gainOk;
    AudioDecoder::Tags tags;
    std::vector<double> beatgrid;
};

static TrackResult analyzeTrack(const std::string& path) {
    std::unique_ptr<QmBpmAnalyzer> bpm;
    std::unique_ptr<QmKeyAnalyzer> key;
    std::unique_ptr<GainAnalyzer> gain;
    bool initialized = false;
    int sampleRate = 0;

    std::string err;
    AudioDecoder::Tags tags;
    bool ok = AudioDecoder::decode(
        path,
        [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
            if (!initialized) {
                sampleRate = info.sampleRate;
                bpm = std::make_unique<QmBpmAnalyzer>(sampleRate);
                key = std::make_unique<QmKeyAnalyzer>(sampleRate);
                gain = std::make_unique<GainAnalyzer>(sampleRate);
                initialized = true;
            }
            bpm->feed(samples, numFrames);
            key->feed(samples, numFrames);
            gain->feed(samples, numFrames);
        },
        err, tags);

    EXPECT_TRUE(ok) << "Decode failed: " << err;
    EXPECT_TRUE(initialized) << "No audio data in: " << path;

    TrackResult r{};
    if (ok && initialized) {
        r.bpm = bpm->result();
        r.key = key->result();
        r.gainOk = gain->result(r.gain);
        r.tags = std::move(tags);
        r.beatgrid = bpm->beatFramesSecs();
    }
    return r;
}

#define SKIP_IF_MISSING(path)                                                             \
    do {                                                                                  \
        if (!std::filesystem::exists(path)) {                                             \
            GTEST_SKIP() << "Asset not found (run tests/download_assets.sh): " << (path); \
        }                                                                                 \
    } while (0)

constexpr float kBpmTol = 1.0f;

// Royalty-free tracks from Audionautix (CC BY 4.0, Jason Shaw).
// Download with:  bash tests/download_assets.sh
// Reference values detected with Mixxx.

struct AudionautixParams {
    const char* file;
    float bpm;
    const char* camelot;
};

class AudionautixTest : public ::testing::TestWithParam<AudionautixParams> {};

TEST_P(AudionautixTest, BpmAndKey) {
    std::string path = assetsDir() + GetParam().file;
    SKIP_IF_MISSING(path);
    auto r = analyzeTrack(path);
    EXPECT_NEAR(r.bpm, GetParam().bpm, kBpmTol) << GetParam().file << " BPM: got " << r.bpm;
    EXPECT_EQ(r.key.camelot, GetParam().camelot) << GetParam().file << " Key: got " << r.key.key;
    EXPECT_FALSE(r.beatgrid.empty()) << GetParam().file << " beatgrid should be non-empty";
    if (!r.beatgrid.empty()) {
        EXPECT_GT(r.beatgrid.front(), 0.0) << GetParam().file << " first beat should be > 0s";
    }
}

// clang-format off
INSTANTIATE_TEST_SUITE_P(
        Audionautix,
        AudionautixTest,
        ::testing::Values(
                AudionautixParams{"FallingSky.mp3",       128.0f, "3B"},
                AudionautixParams{"LatinHouseBed.mp3",    130.0f, "11A"},
                AudionautixParams{"BanjoHop.mp3",         130.0f, "5A"},
                AudionautixParams{"BeBop25.mp3",          100.0f, "8B"},
                AudionautixParams{"Boom.mp3",             146.0f, "8A"},
                AudionautixParams{"NightRave.mp3",        138.0f, "7A"},
                AudionautixParams{"Sk8board.mp3",          80.0f, "10B"},
                AudionautixParams{"AllGoodInTheWood.mp3", 120.0f, "6A"},
                AudionautixParams{"DanceDubber.mp3",      140.0f, "8B"},
                AudionautixParams{"DogHouse.mp3",         145.0f, "3B"},
                AudionautixParams{"Don'tStop.mp3",        140.0f, "12A"}),
        [](const ::testing::TestParamInfo<AudionautixParams>& info) {
            std::string name = info.param.file;
            for (char& c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
            return name;
        });
// clang-format on

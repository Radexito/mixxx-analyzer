#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "QmBpmAnalyzer.h"
#include "QmKeyAnalyzer.h"

static const std::string kBeckyUro = "/home/radexito/Music/becky-uro/";

// ── Helper ─────────────────────────────────────────────────────────────────

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
                    bpm  = std::make_unique<QmBpmAnalyzer>(info.sampleRate);
                    key  = std::make_unique<QmKeyAnalyzer>(info.sampleRate);
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

// ── Named tests (existing) ────────────────────────────────────────────────

constexpr float kBpmTol = 1.0f;

TEST(AnalysisTest, BellaCiao_BpmAndKey) {
    auto r = analyzeTrack(kBeckyUro + "Bella Ciao (Hard Techno Remix) [InZ44_NC2a4].mp3");
    EXPECT_NEAR(r.bpm, 165.0f, kBpmTol) << "Got: " << r.bpm;
    EXPECT_EQ(r.key.camelot, "7A")      << "Got: " << r.key.key;
}

TEST(AnalysisTest, Micrograms1200_BpmAndKey) {
    auto r = analyzeTrack("/home/radexito/Music/psytrance/1200 Micrograms - The Apocalypse.mp3");
    EXPECT_NEAR(r.bpm, 147.0f, kBpmTol) << "Got: " << r.bpm;
    EXPECT_EQ(r.key.camelot, "7A")      << "Got: " << r.key.key;
}

// ── Parametrized tests (becky-uro folder) ────────────────────────────────

struct TrackParams {
    const char* file;      // filename relative to kBeckyUro
    float       bpm;
    const char* camelot;
};

class BeckyUroTest : public ::testing::TestWithParam<TrackParams> {};

TEST_P(BeckyUroTest, BpmAndKey) {
    auto p = GetParam();
    auto r = analyzeTrack(kBeckyUro + p.file);
    EXPECT_NEAR(r.bpm, p.bpm, kBpmTol)
            << p.file << " — BPM: got " << r.bpm << ", expected " << p.bpm;
    EXPECT_EQ(r.key.camelot, p.camelot)
            << p.file << " — Key: got " << r.key.key << " (" << r.key.camelot
            << "), expected " << p.camelot;
}

INSTANTIATE_TEST_SUITE_P(
        BeckyUro,
        BeckyUroTest,
        ::testing::Values(
                TrackParams{"BASS IN YOUR FACE [0u0vzOZscmM].mp3",                                       100.0f, "8A"},
                TrackParams{"MONEY ON THE DASH (SPED UP) [0FeqhKTHoaw].mp3",                             150.0f, "7A"},
                TrackParams{"LET'S GET FKD UP [hmdR9lacEkI].mp3",                                        106.7f, "6B"},
                TrackParams{"D-Frek - La La La (HTF REMIX) [-B4mYUbCoIA].mp3",                           100.2f, "11B"},
                TrackParams{"Spongebob [ufpvSvVRhnA].mp3",                                               100.0f, "9B"},
                TrackParams{"Prada [KYcSPar5uTk].mp3",                                                   142.0f, "4A"},
                TrackParams{"I Like To Move It [KpM9mpeLgtU].mp3",                                       150.0f, "5A"},
                TrackParams{"i like the way you kiss me [jVBsM8CM0AE].mp3",                              151.6f, "1B"},
                TrackParams{"Stolen Dance (Hypertechno Mix) [0rJaxjoTl_o].mp3",                          150.0f, "1B"},
                TrackParams{"L'Amour Toujours (Sped Up Hardstyle) [JKfX8-EeitE].mp3",                   169.1f, "2B"},
                TrackParams{"Jiyagi - Mockingbird (Frenchcore) (Videoclip) [l3WwF7HQlVU].mp3",           100.0f, "8A"},
                TrackParams{"Blue Stahli - \xef\xbc\x82Suit Up\xef\xbc\x82 [_DzlYVFU8Oo].mp3",          110.0f, "5A"},
                TrackParams{"Calabria [Dj1TzBJcRec].mp3",                                                148.0f, "3A"},
                TrackParams{"Kernkraft 400 [xkQbISDpsjk].mp3",                                          150.0f, "10A"},
                TrackParams{"Tokyo Drift [ayNKqMoslU4].mp3",                                             150.0f, "2A"},
                TrackParams{"Ilona - Un monde parfait (D-Frek Remix) [t5PmIE8Nn_w].mp3",                103.0f, "11A"},
                TrackParams{"GIVE ME [G5dnoj3c1uA].mp3",                                                 169.0f, "6A"},
                TrackParams{"Say It Right [tC5jAUZ5Wsk].mp3",                                           143.1f, "6A"},
                TrackParams{"Give It To Me (Techno Edit) [7tTg4CuPWjU].mp3",                            150.1f, "1B"},
                TrackParams{"Phatt Bass (Blade) (feat. FEEZZ) [SuFiQH5nmi0].mp3",                       140.0f, "8B"},
                TrackParams{"Kulikitaka (D-Frek & Fortanoiza FRENCHCORE REMIX) [5wjY3q3PuxI].mp3",      195.0f, "2A"},
                TrackParams{"Santiano (D-Frek Frenchcore Remix) [tAuiIP9l_i4].mp3",                     100.0f, "9A"},
                TrackParams{"S&M (HYPERTECHNO Edit) [BjjB9YXTp-A].mp3",                                 142.0f, "4A"},
                TrackParams{"Crusade (Club Edit) [LFQdPZ7qsm8].mp3",                                    150.0f, "2A"},
                TrackParams{"Queen of Kings (Gabry Ponte Remix) [Q8_CbyG0ZuM].mp3",                     135.2f, "9A"},
                TrackParams{"Heroine [aZk3Apw0WH8].mp3",                                                144.0f, "7B"},
                TrackParams{"strangers tekkno [pIoJZQC9tUM].mp3",                                       160.0f, "12A"},
                TrackParams{"The Destroyer of All Things [lDcSBVRRwQ4].mp3",                            128.0f, "8A"},
                TrackParams{"D-Frek & DCX - Flying High [T2PqQH4Y0xI].mp3",                              92.5f, "4B"},
                TrackParams{"Linkin Park - New Divide (Jesse Bloch Techno Remix) [7iw7KVSv8KA].mp3",    148.0f, "4A"},
                TrackParams{"Lullaby [sgH6emB4UdY].mp3",                                                142.0f, "3A"},
                TrackParams{"Blade [nEbbPM_Xl14].mp3",                                                  136.0f, "2A"},
                TrackParams{"HALO [OfNKfQTNRHE].mp3",                                                   150.0f, "1A"},
                TrackParams{"LSD [xw6BPt9f1eU].mp3",                                                    130.0f, "4A"},
                TrackParams{"LET'S GO [0485blukCq0].mp3",                                               129.0f, "1A"},
                TrackParams{"D-Frek - Viens je t'emm\xc3\xa8ne [_tAFhrdIcgE].mp3",                     175.0f, "5A"},
                TrackParams{"BOOM BOOM BOOM BOOM (D-Frek BOOTLEG) [-bvv3LMnh9M].mp3",                    95.0f, "2B"},
                TrackParams{"Derni\xc3\xa8re danse (Techno Mix) [yGFgaPVHSrU].mp3",                     140.0f, "5A"},
                TrackParams{"ART DECO (TEKKNO) [GfWldXRcQgg].mp3",                                      150.0f, "3A"},
                TrackParams{"Feel The Rhythm [biuGVxYowIM].mp3",                                        135.0f, "6B"},
                TrackParams{"Wanna Play\xef\xbc\x9f [9eRTxxTuUxo].mp3",                                 100.0f, "6B"},
                TrackParams{"La mort avec toi (Billx Remix) [P6yEhO4p8Jo].mp3",                         105.0f, "10A"},
                TrackParams{"Cover Up My Face [fld2XDSbK64].mp3",                                       155.0f, "7B"},
                TrackParams{"Le Bask & D-Frek - Six Names [0AfI5hjQB_0].mp3",                           195.0f, "10B"},
                TrackParams{"Techno Syndrome (Mortal Kombat) [Sr1bLLvsbh0].mp3",                        133.0f, "8A"},
                TrackParams{"Linkin Park - In The End (D-Fence Remix) [JR-vXXSO7Ww].mp3",              105.0f, "3A"},
                TrackParams{"Walking On a Dream (Techno Remix) (feat. Zyzz) [EConX2hhuuY].mp3",         150.0f, "3B"},
                TrackParams{"Evil Sailor [Q_QdVLzolXI].mp3",                                            100.0f, "4A"},
                TrackParams{"Toccata [L0P8xj5jFn0].mp3",                                                140.0f, "7A"},
                TrackParams{"Hard Techno Is Not A Crime [0IzslPiJTM0].mp3",                             158.0f, "5A"},
                TrackParams{"Mexico [TaHup9LEBIk].mp3",                                                 105.0f, "2A"},
                TrackParams{"D-Frek & La Teigne - Party Crasher [z3P42pLGJ04].mp3",                     100.0f, "7B"},
                TrackParams{"Nothing Like The Oldschool [gBMd2RNk4ZI].mp3",                             100.0f, "5A"}),
        [](const ::testing::TestParamInfo<TrackParams>& info) {
            // Sanitize filename into a valid GTest name
            std::string name = info.param.file;
            for (char& c : name) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
            }
            // Trim trailing underscores from YouTube ID suffix
            return name.substr(0, 40);
        });


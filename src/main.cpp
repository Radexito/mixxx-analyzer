#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "AudioDecoder.h"
#include "GainAnalyzer.h"
#include "QmBpmAnalyzer.h"
#include "QmKeyAnalyzer.h"
#include "SilenceAnalyzer.h"

namespace {

void printUsage(const char* argv0) {
    std::fprintf(stderr, "Usage: %s <audiofile> [audiofile...]\n", argv0);
    std::fprintf(stderr, "\nAnalyzes audio tracks and outputs BPM, key, and gain.\n");
    std::fprintf(stderr, "\nOutputs tab-separated: file\\tBPM\\tKey\\tCamelot\\tLUFS\\tReplayGain\\tIntro\\tOutro\n");
}

bool analyzeFile(const std::string& path) {
    // We need to know the sample rate before constructing analyzers,
    // so we do a quick probe pass first.
    int sampleRate = 0;
    int channels = 0;
    bool initialized = false;

    std::unique_ptr<QmBpmAnalyzer>   bpm;
    std::unique_ptr<QmKeyAnalyzer>   key;
    std::unique_ptr<GainAnalyzer>    gain;
    std::unique_ptr<SilenceAnalyzer> silence;

    std::string decodeError;
    bool ok = AudioDecoder::decode(
            path,
            [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
                if (!initialized) {
                    sampleRate = info.sampleRate;
                    channels = info.channels;
                    bpm     = std::make_unique<QmBpmAnalyzer>(sampleRate);
                    key     = std::make_unique<QmKeyAnalyzer>(sampleRate);
                    gain    = std::make_unique<GainAnalyzer>(sampleRate);
                    silence = std::make_unique<SilenceAnalyzer>(sampleRate, channels);
                    initialized = true;
                }
                bpm->feed(samples, numFrames);
                key->feed(samples, numFrames);
                gain->feed(samples, numFrames);
                silence->feed(samples, numFrames);
            },
            decodeError);

    if (!ok) {
        std::fprintf(stderr, "Error decoding '%s': %s\n", path.c_str(), decodeError.c_str());
        return false;
    }
    if (!initialized) {
        std::fprintf(stderr, "No audio data in '%s'\n", path.c_str());
        return false;
    }

    float detectedBpm = bpm->result();
    QmKeyAnalyzer::Result detectedKey = key->result();
    GainAnalyzer::Result gainResult{};
    bool gainOk = gain->result(gainResult);
    SilenceAnalyzer::Result silenceResult = silence->result();

    auto fmtTime = [](double secs) -> std::string {
        int m = static_cast<int>(secs) / 60;
        double s = secs - m * 60.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d:%05.2f", m, s);
        return buf;
    };

    // Print header once (detect by checking if first file)
    // We just print the result line
    if (detectedBpm > 0.0f) {
        std::printf("%-50s  BPM: %6.2f  Key: %-10s (%3s)  LUFS: %7.2f  RG: %+.2f dB  Intro: %s  Outro: %s\n",
                path.c_str(),
                detectedBpm,
                detectedKey.key.c_str(),
                detectedKey.camelot.c_str(),
                gainOk ? gainResult.lufs : 0.0,
                gainOk ? gainResult.replayGain : 0.0,
                fmtTime(silenceResult.introSecs).c_str(),
                fmtTime(silenceResult.outroSecs).c_str());
    } else {
        std::printf("%-50s  BPM: (undetected)  Key: %-10s (%3s)  LUFS: %7.2f  RG: %+.2f dB  Intro: %s  Outro: %s\n",
                path.c_str(),
                detectedKey.key.c_str(),
                detectedKey.camelot.c_str(),
                gainOk ? gainResult.lufs : 0.0,
                gainOk ? gainResult.replayGain : 0.0,
                fmtTime(silenceResult.introSecs).c_str(),
                fmtTime(silenceResult.outroSecs).c_str());
    }

    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    bool allOk = true;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (!analyzeFile(argv[i])) {
            allOk = false;
        }
    }
    return allOk ? 0 : 1;
}

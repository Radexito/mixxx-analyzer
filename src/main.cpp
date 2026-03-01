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
    std::fprintf(stderr, "Usage: %s [--json] <audiofile> [audiofile...]\n", argv0);
    std::fprintf(stderr, "\nAnalyzes audio tracks and outputs BPM, key, gain, and intro/outro.\n");
    std::fprintf(stderr, "\n  --json   Output results as a JSON array\n");
}

// Escape a string for embedding in JSON.
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (unsigned char c : s) {
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\t')
            out += "\\t";
        else if (c < 0x20) {
            // Escape remaining control characters as \u00XX
            char buf[7];
            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
            out += buf;
        } else
            out += static_cast<char>(c);
    }
    return out;
}

struct AnalysisResult {
    std::string path;
    float bpm;
    std::string key;
    std::string camelot;
    double lufs;
    double replayGain;
    double introSecs;
    double outroSecs;
    AudioDecoder::Tags tags;
    std::vector<double> beatgrid;
};

bool analyzeFile(const std::string& path, AnalysisResult& out) {
    int sampleRate = 0;
    int channels = 0;
    bool initialized = false;

    std::unique_ptr<QmBpmAnalyzer> bpm;
    std::unique_ptr<QmKeyAnalyzer> key;
    std::unique_ptr<GainAnalyzer> gain;
    std::unique_ptr<SilenceAnalyzer> silence;

    std::string decodeError;
    AudioDecoder::Tags tags;
    bool ok = AudioDecoder::decode(
        path,
        [&](const float* samples, int numFrames, const AudioDecoder::AudioInfo& info) {
            if (!initialized) {
                sampleRate = info.sampleRate;
                channels = info.channels;
                bpm = std::make_unique<QmBpmAnalyzer>(sampleRate);
                key = std::make_unique<QmKeyAnalyzer>(sampleRate);
                gain = std::make_unique<GainAnalyzer>(sampleRate);
                silence = std::make_unique<SilenceAnalyzer>(sampleRate, channels);
                initialized = true;
            }
            bpm->feed(samples, numFrames);
            key->feed(samples, numFrames);
            gain->feed(samples, numFrames);
            silence->feed(samples, numFrames);
        },
        decodeError, tags);

    if (!ok) {
        std::fprintf(stderr, "Error decoding '%s': %s\n", path.c_str(), decodeError.c_str());
        return false;
    }
    if (!initialized) {
        std::fprintf(stderr, "No audio data in '%s'\n", path.c_str());
        return false;
    }

    GainAnalyzer::Result gainResult{};
    bool gainOk = gain->result(gainResult);
    QmKeyAnalyzer::Result detectedKey = key->result();
    SilenceAnalyzer::Result silenceResult = silence->result();

    out.path = path;
    out.bpm = bpm->result();
    out.key = detectedKey.key;
    out.camelot = detectedKey.camelot;
    out.lufs = gainOk ? gainResult.lufs : 0.0;
    out.replayGain = gainOk ? gainResult.replayGain : 0.0;
    out.introSecs = silenceResult.introSecs;
    out.outroSecs = silenceResult.outroSecs;
    out.tags = std::move(tags);
    out.beatgrid = bpm->beatFramesSecs();
    return true;
}

void printHuman(const AnalysisResult& r) {
    auto fmtTime = [](double secs) -> std::string {
        int m = static_cast<int>(secs) / 60;
        double s = secs - m * 60.0;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d:%05.2f", m, s);
        return buf;
    };
    if (r.bpm > 0.0f) {
        std::printf(
            "%-50s  BPM: %6.2f  Key: %-10s (%3s)  LUFS: %7.2f  RG: %+.2f dB  Intro: %s  Outro: "
            "%s\n",
            r.path.c_str(), r.bpm, r.key.c_str(), r.camelot.c_str(), r.lufs, r.replayGain,
            fmtTime(r.introSecs).c_str(), fmtTime(r.outroSecs).c_str());
    } else {
        std::printf(
            "%-50s  BPM: (undetected)  Key: %-10s (%3s)  LUFS: %7.2f  RG: %+.2f dB  Intro: %s  "
            "Outro: %s\n",
            r.path.c_str(), r.key.c_str(), r.camelot.c_str(), r.lufs, r.replayGain,
            fmtTime(r.introSecs).c_str(), fmtTime(r.outroSecs).c_str());
    }
}

void printJson(const std::vector<AnalysisResult>& results) {
    std::printf("[\n");
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        std::printf("  {\n");
        std::printf("    \"file\": \"%s\",\n", jsonEscape(r.path).c_str());
        if (r.bpm > 0.0f)
            std::printf("    \"bpm\": %.2f,\n", r.bpm);
        else
            std::printf("    \"bpm\": null,\n");
        std::printf("    \"key\": \"%s\",\n", jsonEscape(r.key).c_str());
        std::printf("    \"camelot\": \"%s\",\n", jsonEscape(r.camelot).c_str());
        std::printf("    \"lufs\": %.2f,\n", r.lufs);
        std::printf("    \"replayGain\": %.2f,\n", r.replayGain);
        std::printf("    \"introSecs\": %.3f,\n", r.introSecs);
        std::printf("    \"outroSecs\": %.3f,\n", r.outroSecs);
        // Tags as flat fields
        std::printf("    \"title\": \"%s\",\n", jsonEscape(r.tags.title).c_str());
        std::printf("    \"artist\": \"%s\",\n", jsonEscape(r.tags.artist).c_str());
        std::printf("    \"album\": \"%s\",\n", jsonEscape(r.tags.album).c_str());
        std::printf("    \"year\": \"%s\",\n", jsonEscape(r.tags.year).c_str());
        std::printf("    \"genre\": \"%s\",\n", jsonEscape(r.tags.genre).c_str());
        std::printf("    \"label\": \"%s\",\n", jsonEscape(r.tags.label).c_str());
        std::printf("    \"comment\": \"%s\",\n", jsonEscape(r.tags.comment).c_str());
        std::printf("    \"trackNumber\": \"%s\",\n", jsonEscape(r.tags.trackNumber).c_str());
        std::printf("    \"bpmTag\": \"%s\",\n", jsonEscape(r.tags.bpmTag).c_str());
        // Beatgrid as array of seconds
        std::printf("    \"beatgrid\": [");
        for (std::size_t j = 0; j < r.beatgrid.size(); ++j) {
            std::printf("%.6f%s", r.beatgrid[j], (j + 1 < r.beatgrid.size()) ? "," : "");
        }
        std::printf("]\n");
        std::printf("  }%s\n", (i + 1 < results.size()) ? "," : "");
    }
    std::printf("]\n");
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    bool jsonMode = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (std::strcmp(argv[i], "--json") == 0) {
            jsonMode = true;
        } else {
            files.push_back(argv[i]);
        }
    }

    if (files.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    bool allOk = true;
    std::vector<AnalysisResult> results;

    for (const auto& path : files) {
        AnalysisResult r;
        if (analyzeFile(path, r)) {
            if (jsonMode)
                results.push_back(std::move(r));
            else
                printHuman(r);
        } else {
            allOk = false;
        }
    }

    if (jsonMode)
        printJson(results);

    return allOk ? 0 : 1;
}

// qm-dsp headers must come before our own to avoid preprocessor conflicts.
#include <dsp/onsets/DetectionFunction.h>
#include <dsp/tempotracking/TempoTrackV2.h>
#include <maths/MathUtilities.h>

#include "QmBpmAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace {

// Exact constants from mixxx::AnalyzerQueenMaryBeats
constexpr float kStepSecs = 0.01161f;
constexpr int kMaximumBinSizeHz = 50;

DFConfig makeDetectionFunctionConfig(int stepSizeFrames, int windowSize) {
    DFConfig config;
    config.DFType = DF_COMPLEXSD;
    config.stepSize = stepSizeFrames;
    config.frameLength = windowSize;
    config.dbRise = 3;
    config.adaptiveWhitening = false;
    config.whiteningRelaxCoeff = -1;
    config.whiteningFloor = -1;
    return config;
}

// ── BeatUtils port ──────────────────────────────────────────────────────────
// Direct port of mixxx::BeatUtils::retrieveConstRegions and makeConstBpm
// (src/track/beatutils.cpp), using plain std::vector<double> for frame
// positions instead of QVector<FramePos>. Logic and constants are identical.

constexpr double kMaxSecsPhaseError    = 0.025;
constexpr double kMaxSecsPhaseErrorSum = 0.1;
constexpr int kMaxOutliersCount        = 1;
constexpr int kMinRegionBeatCount      = 16;

struct ConstRegion {
    double firstBeat;  // first beat position in frames
    double beatLength; // average beat length in frames
};

std::vector<ConstRegion> retrieveConstRegions(
        const std::vector<double>& beatFrames, int sampleRate) {
    if ((int)beatFrames.size() < 2) return {};

    const double maxPhaseError    = kMaxSecsPhaseError * sampleRate;
    const double maxPhaseErrorSum = kMaxSecsPhaseErrorSum * sampleRate;

    int leftIndex  = 0;
    int rightIndex = (int)beatFrames.size() - 1;
    std::vector<ConstRegion> regions;

    while (leftIndex < (int)beatFrames.size() - 1) {
        double meanBeatLength = (beatFrames[rightIndex] - beatFrames[leftIndex]) /
                (rightIndex - leftIndex);
        int outliersCount = 0;
        double ironedBeat = beatFrames[leftIndex];
        double phaseErrorSum = 0;
        int i = leftIndex + 1;

        for (; i <= rightIndex; ++i) {
            ironedBeat += meanBeatLength;
            double phaseError = ironedBeat - beatFrames[i];
            phaseErrorSum += phaseError;
            if (std::fabs(phaseError) > maxPhaseError) {
                ++outliersCount;
                if (outliersCount > kMaxOutliersCount || i == leftIndex + 1) break;
            }
            if (std::fabs(phaseErrorSum) > maxPhaseErrorSum) break;
        }

        if (i > rightIndex) {
            // Verify that border beats don't bend meanBeatLength unfavorably.
            double regionBorderError = 0;
            if (rightIndex > leftIndex + 2) {
                double firstLen = beatFrames[leftIndex + 1] - beatFrames[leftIndex];
                double lastLen  = beatFrames[rightIndex]   - beatFrames[rightIndex - 1];
                regionBorderError = std::fabs(firstLen + lastLen - 2 * meanBeatLength);
            }
            if (regionBorderError < maxPhaseError / 2) {
                regions.push_back({beatFrames[leftIndex], meanBeatLength});
                leftIndex  = rightIndex;
                rightIndex = (int)beatFrames.size() - 1;
                continue;
            }
        }
        --rightIndex;
    }
    // Sentinel region marking the end.
    regions.push_back({beatFrames.back(), 0.0});
    return regions;
}

// Port of BeatUtils::roundBpmWithinRange / trySnap.
std::optional<double> trySnap(double minBpm, double center, double maxBpm, double fraction) {
    double snap = std::round(center * fraction) / fraction;
    if (snap > minBpm && snap < maxBpm) return snap;
    return std::nullopt;
}

double roundBpmWithinRange(double minBpm, double center, double maxBpm) {
    if (auto s = trySnap(minBpm, center, maxBpm, 1.0)) return *s;
    if (center < 85.0) {
        if (auto s = trySnap(minBpm, center, maxBpm, 2.0)) return *s;
    }
    if (center > 127.0) {
        if (auto s = trySnap(minBpm, center, maxBpm, 2.0 / 3.0)) return *s;
    }
    if (auto s = trySnap(minBpm, center, maxBpm, 3.0))  return *s;
    if (auto s = trySnap(minBpm, center, maxBpm, 12.0)) return *s;
    return center;
}

// Port of BeatUtils::makeConstBpm (dominant region → rounded BPM).
// Exact translation of mixxx/src/track/beatutils.cpp BeatUtils::makeConstBpm.
double makeConstBpm(const std::vector<ConstRegion>& regions, int sampleRate) {
    if (regions.size() < 2) return 0.0;

    // ── Step 1: Find the longest constant region ────────────────────────────
    int midRegionIndex = 0;
    double longestRegionLength = 0.0;
    double longestRegionBeatLength = 0.0;

    for (int i = 0; i < (int)regions.size() - 1; ++i) {
        double length = regions[i + 1].firstBeat - regions[i].firstBeat;
        if (length > longestRegionLength) {
            longestRegionLength    = length;
            longestRegionBeatLength = regions[i].beatLength;
            midRegionIndex         = i;
        }
    }
    if (longestRegionLength == 0.0) return 0.0;

    int longestRegionNumberOfBeats =
            static_cast<int>(longestRegionLength / longestRegionBeatLength + 0.5);
    double longestRegionBeatLengthMin = longestRegionBeatLength -
            (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;
    double longestRegionBeatLengthMax = longestRegionBeatLength +
            (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;

    int startRegionIndex = midRegionIndex;

    // ── Step 2: Extend backward to the earliest compatible region ───────────
    for (int i = 0; i < midRegionIndex; ++i) {
        const double length =
                regions[i + 1].firstBeat - regions[i].firstBeat;
        const int numberOfBeats =
                static_cast<int>(length / regions[i].beatLength + 0.5);
        if (numberOfBeats < kMinRegionBeatCount) continue;

        const double thisMin = regions[i].beatLength -
                (kMaxSecsPhaseError * sampleRate) / numberOfBeats;
        const double thisMax = regions[i].beatLength +
                (kMaxSecsPhaseError * sampleRate) / numberOfBeats;

        if (longestRegionBeatLength <= thisMin || longestRegionBeatLength >= thisMax)
            continue;

        // Combined span: region i → end of midRegion
        const double newLength =
                regions[midRegionIndex + 1].firstBeat - regions[i].firstBeat;
        const double beatLenMin = std::max(longestRegionBeatLengthMin, thisMin);
        const double beatLenMax = std::min(longestRegionBeatLengthMax, thisMax);
        const int maxBeats = static_cast<int>(std::round(newLength / beatLenMin));
        const int minBeats = static_cast<int>(std::round(newLength / beatLenMax));
        if (minBeats != maxBeats) continue; // ambiguous beat count

        const int nb = minBeats;
        const double newBeatLength = newLength / nb;
        if (newBeatLength > longestRegionBeatLengthMin &&
                newBeatLength < longestRegionBeatLengthMax) {
            longestRegionLength        = newLength;
            longestRegionBeatLength    = newBeatLength;
            longestRegionNumberOfBeats = nb;
            longestRegionBeatLengthMin = longestRegionBeatLength -
                    (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;
            longestRegionBeatLengthMax = longestRegionBeatLength +
                    (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;
            startRegionIndex = i;
            break;
        }
    }

    // ── Step 3: Extend forward to the latest compatible region ──────────────
    for (int i = (int)regions.size() - 2; i > midRegionIndex; --i) {
        const double length =
                regions[i + 1].firstBeat - regions[i].firstBeat;
        const int numberOfBeats =
                static_cast<int>(length / regions[i].beatLength + 0.5);
        if (numberOfBeats < kMinRegionBeatCount) continue;

        const double thisMin = regions[i].beatLength -
                (kMaxSecsPhaseError * sampleRate) / numberOfBeats;
        const double thisMax = regions[i].beatLength +
                (kMaxSecsPhaseError * sampleRate) / numberOfBeats;

        if (longestRegionBeatLength <= thisMin || longestRegionBeatLength >= thisMax)
            continue;

        // Combined span: startRegion → end of region i
        const double newLength =
                regions[i + 1].firstBeat - regions[startRegionIndex].firstBeat;
        const double minBeatLen = std::max(longestRegionBeatLengthMin, thisMin);
        const double maxBeatLen = std::min(longestRegionBeatLengthMax, thisMax);
        const int maxBeats = static_cast<int>(std::round(newLength / minBeatLen));
        const int minBeats = static_cast<int>(std::round(newLength / maxBeatLen));
        if (minBeats != maxBeats) continue;

        const int nb = minBeats;
        const double newBeatLength = newLength / nb;
        if (newBeatLength > longestRegionBeatLengthMin &&
                newBeatLength < longestRegionBeatLengthMax) {
            longestRegionLength        = newLength;
            longestRegionBeatLength    = newBeatLength;
            longestRegionNumberOfBeats = nb;
            break;
        }
    }

    // ── Step 4: Recompute tight tolerance and snap BPM ───────────────────────
    longestRegionBeatLengthMin = longestRegionBeatLength -
            (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;
    longestRegionBeatLengthMax = longestRegionBeatLength +
            (kMaxSecsPhaseError * sampleRate) / longestRegionNumberOfBeats;

    const double minBpm    = 60.0 * sampleRate / longestRegionBeatLengthMax;
    const double maxBpm    = 60.0 * sampleRate / longestRegionBeatLengthMin;
    const double centerBpm = 60.0 * sampleRate / longestRegionBeatLength;
    return roundBpmWithinRange(minBpm, centerBpm, maxBpm);
}

} // namespace

QmBpmAnalyzer::QmBpmAnalyzer(int sampleRate)
        : m_sampleRate(sampleRate), m_windowSize(0), m_stepSizeFrames(0) {
    m_stepSizeFrames = static_cast<int>(m_sampleRate * kStepSecs);
    m_windowSize = MathUtilities::nextPowerOfTwo(m_sampleRate / kMaximumBinSizeHz);
    m_pDetectionFunction = std::make_unique<DetectionFunction>(
            makeDetectionFunctionConfig(m_stepSizeFrames, m_windowSize));

    m_helper.initialize(m_windowSize, m_stepSizeFrames, [this](double* pWindow, size_t) {
        m_detectionResults.push_back(m_pDetectionFunction->processTimeDomain(pWindow));
        return true;
    });
}

QmBpmAnalyzer::~QmBpmAnalyzer() = default;

void QmBpmAnalyzer::feed(const float* interleavedStereo, int numFrames) {
    m_helper.processStereoSamples(interleavedStereo, static_cast<size_t>(numFrames) * 2);
}

float QmBpmAnalyzer::result() {
    m_helper.finalize();

    // Trim trailing zeros (matches Mixxx finalize logic exactly).
    std::size_t nonZeroCount = m_detectionResults.size();
    while (nonZeroCount > 0 && m_detectionResults.at(nonZeroCount - 1) <= 0.0) {
        --nonZeroCount;
    }

    std::size_t required_size = std::max(static_cast<std::size_t>(2), nonZeroCount) - 2;

    std::vector<double> df;
    df.reserve(required_size);
    auto beatPeriod = std::vector<int>(required_size / 128 + 1);

    // Skip first 2 results (potential onset noise), matching Mixxx exactly.
    for (std::size_t i = 2; i < nonZeroCount; ++i) {
        df.push_back(m_detectionResults.at(i));
    }

    TempoTrackV2 tt(static_cast<float>(m_sampleRate), m_stepSizeFrames);
    tt.calculateBeatPeriod(df, beatPeriod);
    tt.calculateBeats(df, beatPeriod, m_beats);

    if (m_beats.size() < 2) {
        return 0.0f;
    }

    // Convert df-increment units to frame positions (matches Mixxx's
    // AnalyzerQueenMaryBeats::finalize exactly):
    //   frame = beat * stepSizeFrames + stepSizeFrames / 2
    std::vector<double> beatFrames;
    beatFrames.reserve(m_beats.size());
    for (double b : m_beats) {
        beatFrames.push_back(b * m_stepSizeFrames + m_stepSizeFrames / 2.0);
    }

    // Replicate BeatUtils::calculateBpm (the path Mixxx uses to set the
    // track's displayed BPM): find the dominant constant-tempo region,
    // compute its average beat length, and snap to a "round" BPM.
    auto regions = retrieveConstRegions(beatFrames, m_sampleRate);
    double bpm   = makeConstBpm(regions, m_sampleRate);
    return static_cast<float>(bpm);
}


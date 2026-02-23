#include "GainAnalyzer.h"

#include <cmath>

namespace {
// EBU R128 reference level for ReplayGain 2.0
constexpr double kReplayGainReferenceLUFS = -18.0;
} // namespace

GainAnalyzer::GainAnalyzer(int sampleRate)
        : m_state(ebur128_init(2, static_cast<unsigned long>(sampleRate), EBUR128_MODE_I)) {
}

GainAnalyzer::~GainAnalyzer() {
    if (m_state) {
        ebur128_destroy(&m_state);
    }
}

void GainAnalyzer::feed(const float* interleavedStereo, int numFrames) {
    if (!m_state) return;
    ebur128_add_frames_float(m_state, interleavedStereo, static_cast<size_t>(numFrames));
}

bool GainAnalyzer::result(Result& out) const {
    if (!m_state) return false;

    double lufs = 0.0;
    int err = ebur128_loudness_global(m_state, &lufs);
    if (err != EBUR128_SUCCESS) return false;
    if (!std::isfinite(lufs)) return false;

    out.lufs = lufs;
    out.replayGain = kReplayGainReferenceLUFS - lufs;
    return true;
}

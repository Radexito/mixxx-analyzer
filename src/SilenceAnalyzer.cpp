#include "SilenceAnalyzer.h"

#include <cmath>

SilenceAnalyzer::SilenceAnalyzer(int sampleRate, int channels)
        : m_sampleRate(sampleRate),
          m_channels(channels),
          m_framesProcessed(0),
          m_signalStart(-1),
          m_signalEnd(-1) {
}

void SilenceAnalyzer::feed(const float* samples, int numFrames) {
    const int count = numFrames * m_channels;
    for (int i = 0; i < count; ++i) {
        if (std::fabs(samples[i]) >= kThreshold) {
            const long long frame = m_framesProcessed + i / m_channels;
            if (m_signalStart < 0) {
                m_signalStart = frame;
            }
            m_signalEnd = frame;
        }
    }
    m_framesProcessed += numFrames;
}

SilenceAnalyzer::Result SilenceAnalyzer::result() const {
    const double sr = static_cast<double>(m_sampleRate);
    const long long start = (m_signalStart < 0) ? 0 : m_signalStart;
    const long long end   = (m_signalEnd   < 0) ? m_framesProcessed : m_signalEnd + 1;
    return {start / sr, end / sr};
}

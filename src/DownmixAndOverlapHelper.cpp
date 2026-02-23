#include "DownmixAndOverlapHelper.h"

#include <algorithm>

bool DownmixAndOverlapHelper::initialize(
        size_t windowSize, size_t stepSize, const WindowReadyCallback& callback) {
    m_buffer.assign(windowSize, 0.0);
    m_callback = callback;
    m_windowSize = windowSize;
    m_stepSize = stepSize;
    // Center the first frame in the FFT window (matches Mixxx behaviour).
    m_bufferWritePosition = windowSize / 2;
    return m_windowSize > 0 && m_stepSize > 0 &&
            m_stepSize <= m_windowSize && callback;
}

bool DownmixAndOverlapHelper::processStereoSamples(
        const float* pInput, size_t inputStereoSamples) {
    return processInner(pInput, inputStereoSamples / 2);
}

bool DownmixAndOverlapHelper::finalize() {
    size_t framesToFillWindow = m_windowSize - m_bufferWritePosition;
    size_t numInputFrames = std::max(framesToFillWindow, m_windowSize / 2 - 1);
    return processInner(nullptr, numInputFrames);
}

bool DownmixAndOverlapHelper::processInner(const float* pInput, size_t numInputFrames) {
    size_t inRead = 0;
    double* pDownmix = m_buffer.data();

    while (inRead < numInputFrames) {
        size_t readAvailable = numInputFrames - inRead;
        size_t writeAvailable = m_windowSize - m_bufferWritePosition;
        size_t numFrames = std::min(readAvailable, writeAvailable);

        if (pInput) {
            for (size_t i = 0; i < numFrames; ++i) {
                pDownmix[m_bufferWritePosition + i] =
                        (pInput[(inRead + i) * 2] + pInput[(inRead + i) * 2 + 1]) * 0.5;
            }
        } else {
            for (size_t i = 0; i < numFrames; ++i) {
                pDownmix[m_bufferWritePosition + i] = 0;
            }
        }
        m_bufferWritePosition += numFrames;
        inRead += numFrames;

        if (m_bufferWritePosition == m_windowSize) {
            if (!m_callback(pDownmix, m_windowSize)) {
                return false;
            }
            // Slide the window forward by stepSize.
            for (size_t i = 0; i < (m_windowSize - m_stepSize); ++i) {
                pDownmix[i] = pDownmix[i + m_stepSize];
            }
            m_bufferWritePosition -= m_stepSize;
        }
    }
    return true;
}

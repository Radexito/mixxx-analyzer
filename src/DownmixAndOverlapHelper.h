#pragma once

#include <functional>
#include <vector>

// Downmixes stereo to mono and feeds it into overlapping windows.
// Direct port of mixxx::DownmixAndOverlapHelper (no Qt/Mixxx types).
class DownmixAndOverlapHelper {
  public:
    DownmixAndOverlapHelper() = default;

    using WindowReadyCallback = std::function<bool(double* pBuffer, size_t frames)>;

    bool initialize(size_t windowSize, size_t stepSize, const WindowReadyCallback& callback);
    bool processStereoSamples(const float* pInput, size_t inputStereoSamples);
    bool finalize();

  private:
    bool processInner(const float* pInput, size_t numInputFrames);

    std::vector<double> m_buffer;
    size_t m_windowSize = 0;
    size_t m_stepSize = 0;
    size_t m_bufferWritePosition = 0;
    WindowReadyCallback m_callback;
};

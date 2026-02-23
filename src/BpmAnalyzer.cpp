#include "BpmAnalyzer.h"

#include <soundtouch/BPMDetect.h>

#include <vector>

BpmAnalyzer::BpmAnalyzer(int sampleRate) : m_sampleRate(sampleRate) {
    // Initialize with mono (1 channel); we downmix stereo ourselves.
    m_detector = std::make_unique<soundtouch::BPMDetect>(1, sampleRate);
}

BpmAnalyzer::~BpmAnalyzer() = default;

void BpmAnalyzer::feed(const float* interleavedStereo, int numFrames) {
    // Downmix stereo to mono
    std::vector<float> mono(numFrames);
    for (int i = 0; i < numFrames; ++i) {
        mono[i] = (interleavedStereo[i * 2] + interleavedStereo[i * 2 + 1]) * 0.5f;
    }
    m_detector->inputSamples(mono.data(), numFrames);
}

float BpmAnalyzer::result() const {
    float bpm = m_detector->getBpm();
    if (bpm <= 0.0f)
        return 0.0f;

    // Normalize to the typical DJ range [60, 200].
    // SoundTouch can detect at half or double the true tempo.
    while (bpm < 60.0f)
        bpm *= 2.0f;
    while (bpm > 200.0f)
        bpm /= 2.0f;
    return bpm;
}

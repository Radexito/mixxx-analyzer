#pragma once

#include <functional>
#include <string>

// Decodes an audio file to interleaved float32 stereo samples and
// delivers them in chunks via callback.
class AudioDecoder {
  public:
    struct AudioInfo {
        int sampleRate;
        int channels; // always 2 after decode
    };

    // callback(samples, numFrames, info)
    // Called repeatedly with successive chunks until EOF.
    using Callback = std::function<void(const float*, int, const AudioInfo&)>;

    // Returns true on success. On failure, 'error' is populated.
    static bool decode(const std::string& path, Callback cb, std::string& error);
};

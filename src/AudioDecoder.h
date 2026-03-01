#pragma once

#include <functional>
#include <string>

// Decodes an audio file to interleaved float32 stereo samples and
// delivers them in chunks via callback.
class AudioDecoder {
  public:
    struct AudioInfo {
        int sampleRate;
        int channels;  // always 2 after decode
    };

    // Metadata tags extracted from the container (ID3, Vorbis Comments, APE).
    struct Tags {
        std::string title;
        std::string artist;
        std::string album;
        std::string year;  // first 4 chars of FFmpeg "date" tag
        std::string genre;
        std::string label;
        std::string comment;
        std::string track_number;
        std::string bpm_tag;  // raw BPM/TBPM string tag (not analyzed BPM)
    };

    // callback(samples, numFrames, info)
    // Called repeatedly with successive chunks until EOF.
    using Callback = std::function<void(const float*, int, const AudioInfo&)>;

    // Returns true on success. On failure, 'error' is populated.
    // tagsOut is populated with embedded metadata tags on success.
    static bool decode(const std::string& path, Callback cb, std::string& error, Tags& tagsOut);

    // Backward-compatible overload: ignores tags.
    static bool decode(const std::string& path, Callback cb, std::string& error) {
        Tags unusedTags;
        return decode(path, cb, error, unusedTags);
    }
};

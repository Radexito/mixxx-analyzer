#include "AudioDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <vector>

namespace {

// RAII helpers
struct FormatContextDeleter {
    void operator()(AVFormatContext* c) { avformat_close_input(&c); }
};
struct CodecContextDeleter {
    void operator()(AVCodecContext* c) { avcodec_free_context(&c); }
};
struct SwrContextDeleter {
    void operator()(SwrContext* c) { swr_free(&c); }
};
struct PacketDeleter {
    void operator()(AVPacket* p) { av_packet_free(&p); }
};
struct FrameDeleter {
    void operator()(AVFrame* f) { av_frame_free(&f); }
};

std::string avError(int err) {
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    return buf;
}

}  // namespace

bool AudioDecoder::decode(const std::string& path, Callback cb, std::string& error) {
    av_log_set_level(AV_LOG_ERROR);  // suppress decoder warnings (timestamp drift etc.)

    // --- Open container ---
    AVFormatContext* rawFmt = nullptr;
    if (int err = avformat_open_input(&rawFmt, path.c_str(), nullptr, nullptr); err < 0) {
        error = "avformat_open_input: " + avError(err);
        return false;
    }
    std::unique_ptr<AVFormatContext, FormatContextDeleter> fmt(rawFmt);

    if (int err = avformat_find_stream_info(fmt.get(), nullptr); err < 0) {
        error = "avformat_find_stream_info: " + avError(err);
        return false;
    }

    // --- Find best audio stream ---
    int streamIdx = av_find_best_stream(fmt.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        error = "No audio stream found";
        return false;
    }
    AVStream* stream = fmt->streams[streamIdx];

    // --- Set up decoder ---
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        error = "Unsupported codec";
        return false;
    }
    std::unique_ptr<AVCodecContext, CodecContextDeleter> codecCtx(avcodec_alloc_context3(codec));
    if (!codecCtx) {
        error = "avcodec_alloc_context3 failed";
        return false;
    }
    if (int err = avcodec_parameters_to_context(codecCtx.get(), stream->codecpar); err < 0) {
        error = "avcodec_parameters_to_context: " + avError(err);
        return false;
    }
    if (int err = avcodec_open2(codecCtx.get(), codec, nullptr); err < 0) {
        error = "avcodec_open2: " + avError(err);
        return false;
    }

    const int outSampleRate = codecCtx->sample_rate;
    const int outChannels = 2;

    // --- Set up resampler: any input format -> stereo interleaved float32 ---
    SwrContext* rawSwr = nullptr;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    // FFmpeg >= 5.1: new channel layout API
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    if (int err = swr_alloc_set_opts2(&rawSwr, &outLayout, AV_SAMPLE_FMT_FLT, outSampleRate,
                                      &codecCtx->ch_layout, codecCtx->sample_fmt,
                                      codecCtx->sample_rate, 0, nullptr);
        err < 0) {
        error = "swr_alloc_set_opts2: " + avError(err);
        return false;
    }
#else
    // FFmpeg < 5.1: legacy channel layout API
    rawSwr = swr_alloc_set_opts(
        nullptr, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_FLT, outSampleRate,
        static_cast<int64_t>(codecCtx->channel_layout ? codecCtx->channel_layout
                                                       : av_get_default_channel_layout(
                                                             codecCtx->channels)),
        codecCtx->sample_fmt, codecCtx->sample_rate, 0, nullptr);
        error = "swr_alloc_set_opts failed";
        return false;
    }
#endif
    std::unique_ptr<SwrContext, SwrContextDeleter> swr(rawSwr);

    if (int err = swr_init(swr.get()); err < 0) {
        error = "swr_init: " + avError(err);
        return false;
    }

    std::unique_ptr<AVPacket, PacketDeleter> pkt(av_packet_alloc());
    std::unique_ptr<AVFrame, FrameDeleter> frame(av_frame_alloc());

    const AudioInfo info{outSampleRate, outChannels};

    // Buffer to accumulate converted output before calling cb
    constexpr int kChunkFrames = 8192;
    std::vector<float> outBuf;
    outBuf.reserve(kChunkFrames * outChannels);

    auto flushBuf = [&]() {
        if (!outBuf.empty()) {
            cb(outBuf.data(), static_cast<int>(outBuf.size()) / outChannels, info);
            outBuf.clear();
        }
    };

    auto convertAndBuffer = [&](AVFrame* f) {
        const int maxOut = f->nb_samples + 256;
        std::vector<float> tmp(maxOut * outChannels);
        uint8_t* dst = reinterpret_cast<uint8_t*>(tmp.data());

        int converted = swr_convert(swr.get(), &dst, maxOut, const_cast<const uint8_t**>(f->data),
                                    f->nb_samples);
        if (converted < 0)
            return;

        size_t prev = outBuf.size();
        outBuf.resize(prev + converted * outChannels);
        std::copy(tmp.begin(), tmp.begin() + converted * outChannels, outBuf.begin() + prev);

        if (static_cast<int>(outBuf.size()) / outChannels >= kChunkFrames) {
            flushBuf();
        }
    };

    // --- Decode loop ---
    while (av_read_frame(fmt.get(), pkt.get()) >= 0) {
        if (pkt->stream_index != streamIdx) {
            av_packet_unref(pkt.get());
            continue;
        }
        avcodec_send_packet(codecCtx.get(), pkt.get());
        av_packet_unref(pkt.get());

        while (true) {
            int err = avcodec_receive_frame(codecCtx.get(), frame.get());
            if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
                break;
            if (err < 0)
                break;
            convertAndBuffer(frame.get());
            av_frame_unref(frame.get());
        }
    }

    // Flush decoder
    avcodec_send_packet(codecCtx.get(), nullptr);
    while (true) {
        int err = avcodec_receive_frame(codecCtx.get(), frame.get());
        if (err == AVERROR_EOF || err < 0)
            break;
        convertAndBuffer(frame.get());
        av_frame_unref(frame.get());
    }

    // Flush resampler
    {
        const int maxOut = swr_get_delay(swr.get(), outSampleRate) + 256;
        if (maxOut > 0) {
            std::vector<float> tmp(maxOut * outChannels);
            uint8_t* dst = reinterpret_cast<uint8_t*>(tmp.data());
            int converted = swr_convert(swr.get(), &dst, maxOut, nullptr, 0);
            if (converted > 0) {
                size_t prev = outBuf.size();
                outBuf.resize(prev + converted * outChannels);
                std::copy(tmp.begin(), tmp.begin() + converted * outChannels,
                          outBuf.begin() + prev);
            }
        }
    }

    flushBuf();
    return true;
}

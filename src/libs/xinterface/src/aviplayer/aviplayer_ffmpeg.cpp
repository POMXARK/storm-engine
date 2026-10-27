// The video of the engine on Linux: FFmpeg decodes the file, Realize uploads a frame as it falls due by its time stamp
// and draws it as the Windows player does. The sound is decoded to a WAV in resource/sounds/video and played by the
// sound service, as the music is. The DirectShow player of Windows is in aviplayer.cpp.
#ifndef _WIN32

#include "aviplayer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "core.h"
#include "file_service.h"
#include "v_sound_service.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

int32_t AVI_GetTextureSize(int32_t width);

namespace
{
using Clock = std::chrono::steady_clock;

// the sound service adds resource\sounds\ to the name of a sound
constexpr auto VIDEO_SOUND_NAME_DIR = "video\\";
constexpr auto VIDEO_SOUND_DIR = "resource/sounds/video";

double Seconds(Clock::duration d)
{
    return std::chrono::duration<double>(d).count();
}

// logo1 of "resource\videos\logo1.avi"
std::string FileStem(const std::string &path)
{
    const auto cut = path.find_last_of("\\/");
    const auto name = cut == std::string::npos ? path : path.substr(cut + 1);
    return name.substr(0, name.find_last_of('.'));
}

uint8_t Clamp(float v)
{
    return static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
}

bool WriteWav(const std::string &path, const std::vector<int16_t> &samples, int channels, int rate)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (f == nullptr)
        return false;
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint16_t blockAlign = static_cast<uint16_t>(channels * 2);
    const uint32_t byteRate = static_cast<uint32_t>(rate) * blockAlign;
    const uint32_t riffSize = 36 + dataBytes;
    const uint32_t fmtSize = 16;
    const uint16_t pcm = 1;
    const uint16_t bits = 16;
    const uint16_t ch = static_cast<uint16_t>(channels);
    const uint32_t sampleRate = static_cast<uint32_t>(rate);
    std::fwrite("RIFF", 1, 4, f);
    std::fwrite(&riffSize, 4, 1, f);
    std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmtSize, 4, 1, f);
    std::fwrite(&pcm, 2, 1, f);
    std::fwrite(&ch, 2, 1, f);
    std::fwrite(&sampleRate, 4, 1, f);
    std::fwrite(&byteRate, 4, 1, f);
    std::fwrite(&blockAlign, 2, 1, f);
    std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f);
    std::fwrite(&dataBytes, 4, 1, f);
    std::fwrite(samples.data(), sizeof(int16_t), samples.size(), f);
    return std::fclose(f) == 0;
}

// the first audio stream of the file as 16-bit PCM of its own rate and at most two channels, into wavPath;
// false when the file has no sound
bool DecodeSound(const std::string &path, const std::string &wavPath)
{
    AVFormatContext *fmt = nullptr;
    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0)
        return false;
    bool ok = false;
    const int aidx = avformat_find_stream_info(fmt, nullptr) < 0
                         ? -1
                         : av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    const AVCodec *codec = aidx < 0 ? nullptr : avcodec_find_decoder(fmt->streams[aidx]->codecpar->codec_id);
    AVCodecContext *ctx = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (ctx != nullptr && avcodec_parameters_to_context(ctx, fmt->streams[aidx]->codecpar) >= 0 &&
        avcodec_open2(ctx, codec, nullptr) >= 0)
    {
        const int channels = std::min(ctx->channels, 2);
        const int rate = ctx->sample_rate;
        const int64_t inLayout = ctx->channel_layout ? ctx->channel_layout : av_get_default_channel_layout(ctx->channels);
        SwrContext *swr = swr_alloc_set_opts(nullptr, av_get_default_channel_layout(channels), AV_SAMPLE_FMT_S16,
                                             rate, inLayout, ctx->sample_fmt, rate, 0, nullptr);
        if (swr != nullptr && swr_init(swr) >= 0)
        {
            std::vector<int16_t> pcm;
            // the samples the resampler gives out for the frame (in == nullptr: what it still holds)
            auto convert = [&](AVFrame *in) {
                const int64_t delay = swr_get_delay(swr, rate);
                const int64_t cap = av_rescale_rnd(delay + (in ? in->nb_samples : 0), rate, rate, AV_ROUND_UP);
                std::vector<int16_t> out(static_cast<size_t>(cap) * channels);
                auto *dst = reinterpret_cast<uint8_t *>(out.data());
                const int n = swr_convert(swr, &dst, static_cast<int>(cap),
                                          in ? (const uint8_t **)in->extended_data : nullptr,
                                          in ? in->nb_samples : 0);
                if (n > 0)
                    pcm.insert(pcm.end(), out.begin(), out.begin() + static_cast<size_t>(n) * channels);
                return n;
            };
            AVFrame *frame = av_frame_alloc();
            AVPacket *pkt = av_packet_alloc();
            bool drained = false;
            for (;;)
            {
                const int r = avcodec_receive_frame(ctx, frame);
                if (r == 0)
                {
                    convert(frame);
                    continue;
                }
                if (r != AVERROR(EAGAIN) || drained)
                    break;
                if (av_read_frame(fmt, pkt) < 0)
                {
                    drained = true;
                    avcodec_send_packet(ctx, nullptr);
                    continue;
                }
                if (pkt->stream_index == aidx)
                    avcodec_send_packet(ctx, pkt);
                av_packet_unref(pkt);
            }
            while (convert(nullptr) > 0)
            {
            }
            ok = !pcm.empty() && WriteWav(wavPath, pcm, channels, rate);
            av_packet_free(&pkt);
            av_frame_free(&frame);
        }
        swr_free(&swr);
    }
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
    return ok;
}

// the frame as BGRA rows (the format of the texture); false for a pixel format it does not read
bool ToBgra(const AVFrame *f, std::vector<uint8_t> &out)
{
    const int w = f->width;
    const int h = f->height;
    out.resize(static_cast<size_t>(w) * h * 4);
    if (f->format == AV_PIX_FMT_RGB24 || f->format == AV_PIX_FMT_BGR24)
    {
        const bool bgr = f->format == AV_PIX_FMT_BGR24;
        for (int y = 0; y < h; ++y)
        {
            const uint8_t *src = f->data[0] + y * f->linesize[0];
            uint8_t *dst = out.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x, src += 3, dst += 4)
            {
                dst[0] = bgr ? src[0] : src[2];
                dst[1] = src[1];
                dst[2] = bgr ? src[2] : src[0];
                dst[3] = 255;
            }
        }
        return true;
    }
    if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P)
    {
        // BT.601: the limited range of YUV420P, the full range of YUVJ420P
        const bool full = f->format == AV_PIX_FMT_YUVJ420P;
        for (int y = 0; y < h; ++y)
        {
            const uint8_t *yRow = f->data[0] + y * f->linesize[0];
            const uint8_t *uRow = f->data[1] + (y / 2) * f->linesize[1];
            const uint8_t *vRow = f->data[2] + (y / 2) * f->linesize[2];
            uint8_t *dst = out.data() + static_cast<size_t>(y) * w * 4;
            for (int x = 0; x < w; ++x, dst += 4)
            {
                const float u = uRow[x / 2] - 128.0f;
                const float v = vRow[x / 2] - 128.0f;
                float r, g, b;
                if (full)
                {
                    const float Y = yRow[x];
                    r = Y + 1.402f * v;
                    g = Y - 0.344f * u - 0.714f * v;
                    b = Y + 1.772f * u;
                }
                else
                {
                    const float c = 1.164f * (yRow[x] - 16.0f);
                    r = c + 1.596f * v;
                    g = c - 0.392f * u - 0.813f * v;
                    b = c + 2.017f * u;
                }
                dst[0] = Clamp(b);
                dst[1] = Clamp(g);
                dst[2] = Clamp(r);
                dst[3] = 255;
            }
        }
        return true;
    }
    return false;
}
} // namespace

bool CAviPlayer::PlayMediaFfmpeg(const char *fileName)
{
    CloseFfmpeg();
    const std::string path = fio->ConvertPathResource(fileName);
    if (avformat_open_input(&m_fmt, path.c_str(), nullptr, nullptr) < 0)
    {
        core.Trace("Video Error!!! Can`t load video file = %s", fileName);
        return false;
    }
    if (avformat_find_stream_info(m_fmt, nullptr) < 0)
    {
        core.Trace("Video Error!!! Can`t read streams of video file = %s", fileName);
        return false;
    }
    m_vidx = av_find_best_stream(m_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_vidx < 0)
    {
        core.Trace("Video Error!!! No video stream in file = %s", fileName);
        return false;
    }
    AVStream *st = m_fmt->streams[m_vidx];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    m_vctx = codec ? avcodec_alloc_context3(codec) : nullptr;
    if (m_vctx == nullptr || avcodec_parameters_to_context(m_vctx, st->codecpar) < 0 ||
        avcodec_open2(m_vctx, codec, nullptr) < 0)
    {
        core.Trace("Video Error!!! Can`t open video codec for file = %s", fileName);
        return false;
    }
    m_srcW = m_vctx->width;
    m_srcH = m_vctx->height;
    m_timeBase = av_q2d(st->time_base);
    const AVRational fps = av_guess_frame_rate(m_fmt, st, nullptr);
    if (fps.num > 0)
        m_frameDur = av_q2d(av_inv_q(fps));
    m_frame = av_frame_alloc();
    m_pkt = av_packet_alloc();

    // the sound: a WAV of the cache, played when the picture starts
    const std::string stem = FileStem(fileName);
    std::filesystem::create_directories(VIDEO_SOUND_DIR);
    if (DecodeSound(path, std::string(VIDEO_SOUND_DIR) + "/" + stem + ".wav"))
        m_soundName = std::string(VIDEO_SOUND_NAME_DIR) + stem + ".wav";

    const int32_t texW = AVI_GetTextureSize(m_srcW);
    const int32_t texH = AVI_GetTextureSize(m_srcH);
    if (m_frame == nullptr || m_pkt == nullptr ||
        FAILED(rs->CreateTexture(texW, texH, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTex)))
    {
        core.Trace("Video Error!!! Can`t create texture for this video");
        return false;
    }

    // the picture fits the screen as the window of the Windows player: scaled to the side that fits, centred
    const POINT screen = rs->GetScreenSize();
    const float k = std::min(static_cast<float>(screen.x) / m_srcW, static_cast<float>(screen.y) / m_srcH);
    const float left = (screen.x - m_srcW * k) / 2.0f;
    const float top = (screen.y - m_srcH * k) / 2.0f;
    const float right = left + m_srcW * k;
    const float bottom = top + m_srcH * k;
    const float tu = static_cast<float>(m_srcW) / texW;
    const float tv = static_cast<float>(m_srcH) / texH;
    const float x[4] = {left, left, right, right};
    const float y[4] = {top, bottom, top, bottom};
    const float u[4] = {0.f, 0.f, tu, tu};
    const float w[4] = {tv, tv, 0.f, 0.f};
    for (int i = 0; i < 4; i++)
    {
        v[i].w = .5f;
        v[i].pos.z = 1.f;
        v[i].pos.x = x[i];
        v[i].pos.y = y[i];
        v[i].tu = u[i];
        v[i].tv = w[i];
    }
    m_hasFrame = false;
    m_finished = false;
    m_drained = false;
    return true;
}

bool CAviPlayer::NextVideoFrame()
{
    for (;;)
    {
        const int r = avcodec_receive_frame(m_vctx, m_frame);
        if (r == 0)
        {
            const int64_t pts = m_frame->best_effort_timestamp;
            m_framePts = (pts == AV_NOPTS_VALUE ? 0 : pts) * m_timeBase;
            return true;
        }
        if (r != AVERROR(EAGAIN) || m_drained)
            return false;
        if (av_read_frame(m_fmt, m_pkt) < 0)
        {
            m_drained = true;
            avcodec_send_packet(m_vctx, nullptr);
            continue;
        }
        if (m_pkt->stream_index == m_vidx)
            avcodec_send_packet(m_vctx, m_pkt);
        av_packet_unref(m_pkt);
    }
}

void CAviPlayer::UploadVideoFrame()
{
    if (!ToBgra(m_frame, m_bgra))
    {
        core.Trace("Video Error!!! Unsupported pixel format of the video");
        m_bContinue = false;
        return;
    }
    D3DLOCKED_RECT lockedRect;
    if (FAILED(pTex->LockRect(0, &lockedRect, nullptr, 0)))
        return;
    const size_t row = static_cast<size_t>(m_srcW) * 4;
    auto *out = static_cast<uint8_t *>(lockedRect.pBits);
    for (int y = 0; y < m_srcH; ++y)
        std::memcpy(out + static_cast<size_t>(y) * lockedRect.Pitch, m_bgra.data() + y * row, row);
    pTex->UnlockRect(0);
}

void CAviPlayer::RealizeFfmpeg()
{
    if (m_fmt == nullptr)
    {
        m_bContinue = false;
        return;
    }
    if (m_bFirstDraw)
    {
        rs->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 0, 0);
        m_bFirstDraw = false;
        m_start = Clock::now();
        StartVideoSound();
    }
    double now = Seconds(Clock::now() - m_start);

    // the last frame stays on the screen for its own time, then the video ends (or starts again)
    if (m_finished && now >= m_endAt)
    {
        if (!bLoop || !RestartVideo())
        {
            m_bContinue = false;
            return;
        }
        now = 0.0;
    }
    if (!m_finished && !m_hasFrame)
    {
        if (NextVideoFrame())
            m_hasFrame = true;
        else
        {
            m_finished = true;
            m_endAt = m_lastPts + m_frameDur;
        }
    }
    if (m_hasFrame && m_framePts <= now)
    {
        UploadVideoFrame();
        m_lastPts = m_framePts;
        m_hasFrame = false;
    }

    if (m_bShowVideo)
    {
        rs->SetTexture(0, pTex);
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, XI_AVIVIDEO_FVF, 2, v, sizeof(XI_AVIVIDEO_VERTEX), "battle_icons");
    }
}

bool CAviPlayer::RestartVideo()
{
    StopVideoSound();
    if (av_seek_frame(m_fmt, m_vidx, 0, AVSEEK_FLAG_BACKWARD) < 0)
        return false;
    avcodec_flush_buffers(m_vctx);
    m_drained = false;
    m_hasFrame = false;
    m_finished = false;
    m_lastPts = 0.0;
    m_start = Clock::now();
    StartVideoSound();
    return true;
}

void CAviPlayer::StartVideoSound()
{
    if (m_soundName.empty())
        return;
    auto *sound = core.GetServiceX<VSoundService>();
    if (sound == nullptr)
        return;
    VSoundService::SoundPlayOptions options{};
    options.type = PCM_STEREO;
    m_sound = sound->SoundPlay(m_soundName, options);
}

void CAviPlayer::StopVideoSound()
{
    if (m_sound == 0)
        return;
    if (auto *sound = core.GetServiceX<VSoundService>())
    {
        sound->SoundStop(m_sound);
        sound->SoundRelease(m_sound);
    }
    m_sound = 0;
}

void CAviPlayer::CloseFfmpeg()
{
    StopVideoSound();
    if (pTex != nullptr && rs != nullptr)
        rs->Release(pTex);
    pTex = nullptr;
    av_frame_free(&m_frame);
    av_packet_free(&m_pkt);
    avcodec_free_context(&m_vctx);
    avformat_close_input(&m_fmt);
    m_vidx = -1;
    m_hasFrame = false;
    m_finished = false;
    m_drained = false;
    m_lastPts = 0.0;
    m_bgra.clear();
    m_soundName.clear();
}

#endif

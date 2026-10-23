#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <mmreg.h>

#pragma comment(lib, "ole32.lib")
#endif

// Records what the default output device plays (WASAPI loopback) into a file of raw PCM. Windows only: the other
// systems get a stub that does not start. Silence is written as zeros, so the length of the file follows the clock
// and the sound stays in step with a video that was started next to it.
class LoopbackRecorder
{
  public:
    ~LoopbackRecorder()
    {
        Stop();
    }

    // true if the capture runs; Format() says how the file is laid out
    bool Start(const std::string &path)
    {
#ifdef _WIN32
        Stop();
        file_ = std::fopen(path.c_str(), "wb");
        if (file_ == nullptr)
            return false;
        ready_ = false;
        failed_ = false;
        stop_ = false;
        thread_ = std::thread([this] { Run(); });
        // the thread opens the device and tells whether it succeeded
        for (int i = 0; i < 300 && !ready_ && !failed_; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (!ready_)
        {
            Stop();
            return false;
        }
        return true;
#else
        (void)path;
        return false;
#endif
    }

    void Stop()
    {
#ifdef _WIN32
        stop_ = true;
        if (thread_.joinable())
            thread_.join();
        if (file_ != nullptr)
        {
            std::fclose(file_);
            file_ = nullptr;
        }
#endif
    }

    // the ffmpeg name of the sample format (f32le, s16le) and the rate and channels of the file
    const std::string &Format() const
    {
        return format_;
    }
    int Rate() const
    {
        return rate_;
    }
    int Channels() const
    {
        return channels_;
    }

  private:
#ifdef _WIN32
    template <typename T> static void Release(T *&p)
    {
        if (p != nullptr)
        {
            p->Release();
            p = nullptr;
        }
    }

    void Run()
    {
        const bool comInited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
        IMMDeviceEnumerator *enumerator = nullptr;
        IMMDevice *device = nullptr;
        IAudioClient *client = nullptr;
        IAudioCaptureClient *capture = nullptr;
        WAVEFORMATEX *mix = nullptr;

        bool ok = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                             __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&enumerator)));
        ok = ok && SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
        ok = ok && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                              reinterpret_cast<void **>(&client)));
        ok = ok && SUCCEEDED(client->GetMixFormat(&mix));
        if (ok)
        {
            // the format of the mix is float 32 bit nearly always; 16 bit is the other one that is understood
            const WORD tag = mix->wFormatTag;
            uint32_t sub = tag;
            if (tag == WAVE_FORMAT_EXTENSIBLE)
                sub = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix)->SubFormat.Data1;
            if (sub == WAVE_FORMAT_IEEE_FLOAT && mix->wBitsPerSample == 32)
                format_ = "f32le";
            else if (sub == WAVE_FORMAT_PCM && mix->wBitsPerSample == 16)
                format_ = "s16le";
            else
                ok = false;
        }
        ok = ok && SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0,
                                                mix, nullptr));
        ok = ok && SUCCEEDED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void **>(&capture)));
        ok = ok && SUCCEEDED(client->Start());

        if (!ok)
        {
            failed_ = true;
        }
        else
        {
            rate_ = static_cast<int>(mix->nSamplesPerSec);
            channels_ = mix->nChannels;
            const size_t frameBytes = mix->nBlockAlign;
            ready_ = true;

            const auto begin = std::chrono::steady_clock::now();
            uint64_t written = 0; // frames
            std::vector<char> zeros;

            auto pad = [&](uint64_t upTo) {
                while (written < upTo)
                {
                    const uint64_t n = std::min<uint64_t>(upTo - written, 4800);
                    zeros.assign(static_cast<size_t>(n) * frameBytes, 0);
                    std::fwrite(zeros.data(), 1, zeros.size(), file_);
                    written += n;
                }
            };
            auto elapsedFrames = [&]() {
                const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - begin)
                                    .count();
                return static_cast<uint64_t>(us) * static_cast<uint64_t>(rate_) / 1000000ull;
            };

            while (!stop_)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                UINT32 packet = 0;
                while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet != 0)
                {
                    BYTE *data = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                        break;
                    // nothing played for a while: the gap is filled with silence before the next sound
                    const uint64_t now = elapsedFrames();
                    if (now > written + frames)
                        pad(now - frames);
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0)
                    {
                        pad(written + frames);
                    }
                    else
                    {
                        std::fwrite(data, frameBytes, frames, file_);
                        written += frames;
                    }
                    capture->ReleaseBuffer(frames);
                }
                // the loopback sends nothing at all in silence
                const uint64_t now = elapsedFrames();
                if (now > written + rate_ / 10)
                    pad(now);
            }
            pad(elapsedFrames());
            client->Stop();
        }

        if (mix != nullptr)
            CoTaskMemFree(mix);
        Release(capture);
        Release(client);
        Release(device);
        Release(enumerator);
        if (comInited)
            CoUninitialize();
    }

    std::thread thread_;
    std::FILE *file_ = nullptr;
    std::atomic<bool> ready_{false};
    std::atomic<bool> failed_{false};
    std::atomic<bool> stop_{false};
#endif
    std::string format_;
    int rate_ = 0;
    int channels_ = 0;
};

#pragma once

#include "../base_video.h"
#include <chrono>
#include <string>
#include <vector>
#ifdef _WIN32 // FIX_LINUX ddraw.h and amstream.h
#include <amstream.h>
#include <ddraw.h>
#endif

#define XI_AVIVIDEO_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_TEXTUREFORMAT2)

// Linux: the file is decoded by FFmpeg (aviplayer_ffmpeg.cpp), the headers stay out of this one
#ifndef _WIN32
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
#endif

struct XI_AVIVIDEO_VERTEX
{
    CVECTOR pos;
    float w;
    float tu, tv;
};

class CAviPlayer : public xiBaseVideo
{
    VDX9RENDER *rs;
    bool m_bShowVideo;

  public:
    CAviPlayer();
    ~CAviPlayer() override;
    bool Init() override;
    void Execute(uint32_t delta_time);
    void Realize(uint32_t delta_time);
    uint64_t ProcessMessage(MESSAGE &message) override;

    void ProcessStage(Stage stage, uint32_t delta) override
    {
        switch (stage)
        {
        case Stage::execute:
            Execute(delta);
            break;
        case Stage::realize:
            Realize(delta);
            break;
            /*case Stage::lost_render:
              LostRender(delta); break;
            case Stage::restore_render:
              RestoreRender(delta); break;*/
        }
    }

    void SetShowVideo(bool bShowVideo) override
    {
        m_bShowVideo = bShowVideo;
    }

    IDirect3DTexture9 *GetCurrentVideoTexture() override
    {
        return pTex;
    }

  protected:
    bool m_bContinue;

#ifdef _WIN32 // FIX_LINUX ddraw.h and amstream.h
    IDirectDraw *pDD;
    IDirectDrawSurface *pPrimarySurface;
    IDirectDrawSurface *pVideoSurface;

    IAMMultiMediaStream *pAMStream;
    IMediaStream *pPrimaryVidStream;
    IDirectDrawMediaStream *pDDStream;
    IDirectDrawStreamSample *pSample;
#endif

    POINT dstPnt;
    RECT lockRect;

    XI_AVIVIDEO_VERTEX v[4];

    IDirect3DSurface9 *pTmpRenderTarget;
    IDirect3DTexture9 *pTex;

    void ReleaseAll();
    bool PlayMedia(const char *fileName);
    bool GetInterfaces();
    void CleanupInterfaces();

    bool m_bFirstDraw;
    bool m_bMakeUninitializeDD;

    std::string filename;
    bool bLoop{false};

#ifndef _WIN32
    bool PlayMediaFfmpeg(const char *fileName);
    void RealizeFfmpeg();
    bool NextVideoFrame();
    void UploadVideoFrame();
    bool RestartVideo();
    void StartVideoSound();
    void StopVideoSound();
    void CloseFfmpeg();

    AVFormatContext *m_fmt{nullptr};
    AVCodecContext *m_vctx{nullptr};
    AVFrame *m_frame{nullptr};
    AVPacket *m_pkt{nullptr};
    int m_vidx{-1};
    bool m_drained{false};
    int32_t m_srcW{0};
    int32_t m_srcH{0};
    double m_timeBase{0.0};
    double m_frameDur{0.04};
    double m_framePts{0.0};
    double m_lastPts{0.0};
    bool m_hasFrame{false};
    bool m_finished{false};
    double m_endAt{0.0};
    std::vector<uint8_t> m_bgra;
    std::string m_soundName;
    uint32_t m_sound{0};
    std::chrono::steady_clock::time_point m_start;
#endif
};

#pragma once

#include "dx9render.h"

class InfoHandler : public Entity
{
    VDX9RENDER *m_rs;

  public:
    InfoHandler();
    ~InfoHandler() override;
    bool Init() override;
    void Execute(uint32_t delta_time);
    void Realize(uint32_t delta_time) const;
    // the captured frame lives in the default pool: the native D3D9 Reset fails while it does
    void LostRender();
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
        case Stage::lost_render:
            LostRender();
            break;
            /*case Stage::restore_render:
              RestoreRender(delta); break;*/
        }
    }

  protected:
    void StringToBufer(char *outStr, int sizeBuf, const char *inStr, int copySize) const;
    const char *GetCutString(const char *pstr, int nOutWidth, float fScale) const;
    bool DoPreOut();

    IDirect3DTexture9 *tex;

    struct
    {
        float x, y, z, rhw;
        float u, v;
    } drawbuf_base[6];
};


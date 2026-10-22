#pragma once

#include "bi_defines.h"

#include <vector>

class IBoardingStatus : public Entity
{
    VDX9RENDER *rs;

  public:
    IBoardingStatus();
    ~IBoardingStatus() override;
    bool Init() override;
    void Realize(uint32_t delta_time);
    uint64_t ProcessMessage(MESSAGE &message) override;

    void ProcessStage(Stage stage, uint32_t delta) override
    {
        switch (stage)
        {
            // case Stage::execute:
            //    Execute(delta); break;
        case Stage::realize:
            Realize(delta);
            break;
            /*case Stage::lost_render:
              LostRender(delta); break;
            case Stage::restore_render:
              RestoreRender(delta); break;*/
        }
    }

  protected:
    void Create();
    void CreateBars(ATTRIBUTES *pBars);
    void CreateFrames(ATTRIBUTES *pFrames);
    void SetCharactersHP(float myHP, float enemyHP);
    void SetBar(int32_t index, float value);

  protected:
    // show parameters
    //-----------------------
    int32_t m_Width;
    int32_t m_Height;
    FPOINT m_myPos;
    FPOINT m_enemyPos;
    uint32_t m_myColor;
    uint32_t m_enemyColor;

    BI_COLORONLY_VERTEX m_MyChar[4];
    BI_COLORONLY_VERTEX m_EnemyChar[4];

    // the bars of the attribute "bars" (health, energy, ...) in place of the two above, when it is set
    struct Bar
    {
        FRECT rect;
        bool fromRight; // the bar shrinks to its right side
        BI_COLORONLY_VERTEX back[4];
        BI_COLORONLY_VERTEX fill[4];
    };
    std::vector<Bar> m_bars;

    // the pictures of the attribute "frames", drawn over the bars (their windows transparent)
    struct Frame
    {
        int32_t texture;
        BI_ONETEXTURE_VERTEX v[4];
    };
    std::vector<Frame> m_frames;

    void ReleaseFrames();
};

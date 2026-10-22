#include "i_boarding_status.h"
#include "entity.h"
#include "core.h"
#include "message.h"
#include "shared/battle_interface/log_msg.h"
#include <stdio.h>
#include <algorithm>

namespace
{
// a quad of the triangle strip: left top, left bottom, right top, right bottom
void SetQuad(BI_COLORONLY_VERTEX *v, float left, float top, float right, float bottom, uint32_t color)
{
    for (int32_t i = 0; i < 4; i++)
    {
        v[i].pos.x = i < 2 ? left : right;
        v[i].pos.y = i % 2 == 0 ? top : bottom;
        v[i].pos.z = 1.f;
        v[i].w = .5f;
        v[i].col = color;
    }
}
} // namespace

IBoardingStatus::IBoardingStatus()
{
    rs = nullptr;
}

IBoardingStatus::~IBoardingStatus()
{
    ReleaseFrames();
}

void IBoardingStatus::ReleaseFrames()
{
    if (rs != nullptr)
        for (const auto &frame : m_frames)
            if (frame.texture >= 0)
                rs->TextureRelease(frame.texture);
    m_frames.clear();
}

bool IBoardingStatus::Init()
{
    if ((rs = core.GetServiceX<VDX9RENDER>()) == nullptr)
    {
        throw std::runtime_error("Can`t create render service");
    }

    return true;
}

uint64_t IBoardingStatus::ProcessMessage(MESSAGE &message)
{
    switch (message.Long())
    {
    case LOG_INIT_CHARCTER_HP:
        Create();
        break;
    case LOG_SET_CHARCTER_HP: {
        const auto myHP = message.Float();
        const auto enemyHP = message.Float();
        SetCharactersHP(myHP, enemyHP);
    }
    break;
    case LOG_SET_BOARDING_BAR: {
        const auto index = message.Long();
        SetBar(index, message.Float());
    }
    break;
    }
    return 0;
}

void IBoardingStatus::Realize(uint32_t delta_time)
{
    if (rs == nullptr)
        return;

    if (m_bars.empty())
    {
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, BI_COLORONLY_VERTEX_FORMAT, 2, m_MyChar, sizeof(BI_COLORONLY_VERTEX),
                            "battle_only_color");
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, BI_COLORONLY_VERTEX_FORMAT, 2, m_EnemyChar,
                            sizeof(BI_COLORONLY_VERTEX), "battle_only_color");
    }
    for (auto &bar : m_bars)
    {
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, BI_COLORONLY_VERTEX_FORMAT, 2, bar.back, sizeof(BI_COLORONLY_VERTEX),
                            "battle_only_color");
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, BI_COLORONLY_VERTEX_FORMAT, 2, bar.fill, sizeof(BI_COLORONLY_VERTEX),
                            "battle_only_color");
    }
    for (auto &frame : m_frames)
    {
        rs->TextureSet(0, frame.texture);
        rs->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, BI_ONETEX_VERTEX_FORMAT, 2, frame.v, sizeof(BI_ONETEXTURE_VERTEX),
                            "battle_rectangle");
    }
}

void IBoardingStatus::Create()
{
    // Set parameters for active action icon
    auto *pA = core.Entity_GetAttributePointer(GetId());
    if (pA != nullptr)
    {
        m_myPos.x = static_cast<float>(pA->GetAttributeAsDword("myLeft", 10));
        m_myPos.y = static_cast<float>(pA->GetAttributeAsDword("myTop", 460));
        m_enemyPos.x = static_cast<float>(pA->GetAttributeAsDword("enemyLeft", 10));
        m_enemyPos.y = static_cast<float>(pA->GetAttributeAsDword("enemyTop", 450));
        m_Width = pA->GetAttributeAsDword("width", 120);
        m_Height = pA->GetAttributeAsDword("height", 8);
        m_myColor = pA->GetAttributeAsDword("myColor", ARGB(255, 0, 0, 128));
        m_enemyColor = pA->GetAttributeAsDword("enemyColor", ARGB(255, 128, 0, 0));
        CreateBars(pA->GetAttributeClass("bars"));
        CreateFrames(pA->GetAttributeClass("frames"));
    }
    else
    {
        m_myPos.x = 10.f;
        m_myPos.y = 460.f;
        m_enemyPos.x = 10.f;
        m_enemyPos.y = 450.f;
        m_Width = 120;
        m_Height = 8;
        m_myColor = ARGB(255, 0, 0, 128);
        m_enemyColor = ARGB(255, 128, 0, 0);
    }

    //
    m_MyChar[0].col = m_MyChar[1].col = m_MyChar[2].col = m_MyChar[3].col = m_myColor;
    m_MyChar[0].w = m_MyChar[1].w = m_MyChar[2].w = m_MyChar[3].w = .5f;
    m_MyChar[0].pos.z = m_MyChar[1].pos.z = m_MyChar[2].pos.z = m_MyChar[3].pos.z = 1.f;
    m_MyChar[2].pos.x = m_MyChar[3].pos.x = m_Width + (m_MyChar[0].pos.x = m_MyChar[1].pos.x = m_myPos.x);
    m_MyChar[1].pos.y = m_MyChar[3].pos.y = m_Height + (m_MyChar[0].pos.y = m_MyChar[2].pos.y = m_myPos.y);

    m_EnemyChar[0].col = m_EnemyChar[1].col = m_EnemyChar[2].col = m_EnemyChar[3].col = m_enemyColor;
    m_EnemyChar[0].w = m_EnemyChar[1].w = m_EnemyChar[2].w = m_EnemyChar[3].w = .5f;
    m_EnemyChar[0].pos.z = m_EnemyChar[1].pos.z = m_EnemyChar[2].pos.z = m_EnemyChar[3].pos.z = 1.f;
    m_EnemyChar[2].pos.x = m_EnemyChar[3].pos.x =
        m_Width + (m_EnemyChar[0].pos.x = m_EnemyChar[1].pos.x = m_enemyPos.x);
    m_EnemyChar[1].pos.y = m_EnemyChar[3].pos.y =
        m_Height + (m_EnemyChar[0].pos.y = m_EnemyChar[2].pos.y = m_enemyPos.y);
}

// bars.<name>: left, top, width, height (pixels), color, backColor, fromRight; LOG_SET_BOARDING_BAR fills them
// by their order, LOG_SET_CHARCTER_HP the first two
void IBoardingStatus::CreateBars(ATTRIBUTES *pBars)
{
    m_bars.clear();
    if (pBars == nullptr)
        return;
    for (uint32_t i = 0; i < pBars->GetAttributesNum(); i++)
    {
        auto *pB = pBars->GetAttributeClass(i);
        Bar bar;
        bar.rect.left = pB->GetAttributeAsFloat("left", 0.f);
        bar.rect.top = pB->GetAttributeAsFloat("top", 0.f);
        bar.rect.right = bar.rect.left + pB->GetAttributeAsFloat("width", 120.f);
        bar.rect.bottom = bar.rect.top + pB->GetAttributeAsFloat("height", 8.f);
        bar.fromRight = pB->GetAttributeAsDword("fromRight", 0) != 0;
        SetQuad(bar.back, bar.rect.left, bar.rect.top, bar.rect.right, bar.rect.bottom,
                pB->GetAttributeAsDword("backColor", ARGB(0, 0, 0, 0)));
        SetQuad(bar.fill, bar.rect.left, bar.rect.top, bar.rect.right, bar.rect.bottom,
                pB->GetAttributeAsDword("color", ARGB(255, 128, 0, 0)));
        m_bars.push_back(bar);
    }
}

// frames.<name>: texture, left, top, width, height (pixels)
void IBoardingStatus::CreateFrames(ATTRIBUTES *pFrames)
{
    ReleaseFrames();
    if (pFrames == nullptr)
        return;
    for (uint32_t i = 0; i < pFrames->GetAttributesNum(); i++)
    {
        auto *pF = pFrames->GetAttributeClass(i);
        const char *texture = pF->GetAttribute("texture");
        if (texture == nullptr)
            continue;
        Frame frame;
        frame.texture = rs->TextureCreate(texture);
        const auto left = pF->GetAttributeAsFloat("left", 0.f);
        const auto top = pF->GetAttributeAsFloat("top", 0.f);
        const auto right = left + pF->GetAttributeAsFloat("width", 256.f);
        const auto bottom = top + pF->GetAttributeAsFloat("height", 64.f);
        for (int32_t n = 0; n < 4; n++)
        {
            frame.v[n].pos.x = n < 2 ? left : right;
            frame.v[n].pos.y = n % 2 == 0 ? top : bottom;
            frame.v[n].pos.z = 1.f;
            frame.v[n].w = .5f;
            frame.v[n].tu = n < 2 ? 0.f : 1.f;
            frame.v[n].tv = n % 2 == 0 ? 0.f : 1.f;
        }
        m_frames.push_back(frame);
    }
}

void IBoardingStatus::SetBar(int32_t index, float value)
{
    if (index < 0 || index >= static_cast<int32_t>(m_bars.size()))
        return;
    auto &bar = m_bars[index];
    const auto width = (bar.rect.right - bar.rect.left) * std::clamp(value, 0.f, 1.f);
    if (bar.fromRight)
        bar.fill[0].pos.x = bar.fill[1].pos.x = bar.rect.right - width;
    else
        bar.fill[2].pos.x = bar.fill[3].pos.x = bar.rect.left + width;
}

void IBoardingStatus::SetCharactersHP(float myHP, float enemyHP)
{
    if (!m_bars.empty())
    {
        SetBar(0, myHP);
        SetBar(1, enemyHP);
        return;
    }
    m_MyChar[2].pos.x = m_MyChar[3].pos.x = m_myPos.x + myHP * m_Width;
    m_EnemyChar[2].pos.x = m_EnemyChar[3].pos.x = m_enemyPos.x + enemyHP * m_Width;
}

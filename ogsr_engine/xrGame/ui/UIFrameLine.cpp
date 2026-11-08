//=============================================================================
//  Filename:   UIFrameLine.cpp
//	Created by Roman E. Marchenko, vortex@gsc-game.kiev.ua
//	Copyright 2004. GSC Game World
//	---------------------------------------------------------------------------
//  Класс аналогичный UIFrameRect за исключением того, что он предназначен для
//	отображения затекстурированного узкого прямоуголника произвольной длинны или
//	ширины. В качестве исходных материалов необходимо 3 текстуры: правая(нижняя),
//	левая(верхняя) и центральная
//=============================================================================

#include "stdafx.h"
#include "UIFrameLine.h"
#include "../hudmanager.h"
#include "UITextureMaster.h"

// OP22: часть рамки без описания текстуры (textures_descr) имеет нулевой исходный размер до первой отрисовки - тогда
// деление на ноль переполняет счёт вершин и движок падает («vl_Count=0»). Берём размер из самой текстуры.
static void op22_frame_part_size(CUIStaticItem& it)
{
    const Frect r = it.GetOriginalRect();
    if (r.width() > 0.f && r.height() > 0.f)
        return;
    UIRender->SetShader(*it.GetShader());
    Fvector2 ts;
    UIRender->GetActiveTextureResolution(ts);
    if (ts.x > 0.f && ts.y > 0.f)
        it.SetOriginalRect(0.f, 0.f, ts.x, ts.y);
    else
        it.SetOriginalRect(0.f, 0.f, 1.f, 1.f);
    static bool once{};
    if (!once)
    {
        once = true;
        Msg("! [OP22] frame texture part without textures_descr entry: size taken from the texture file");
    }
}


static void op22_short_frame_line()
{
    static bool once{};
    if (!once)
    {
        once = true;
        Msg("! [OP22] frame line is shorter than its end parts: center part skipped");
    }
}

//////////////////////////////////////////////////////////////////////////

CUIFrameLine::CUIFrameLine() : uFlags(0), iSize(0), bHorizontalOrientation(true) { iPos.set(0, 0); }

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::Init(LPCSTR base_name, float x, float y, float size, bool horizontal, DWORD align)
{
    SetPos(x, y);
    SetSize(size);
    SetAlign(align);
    SetOrientation(horizontal);

    InitTexture(base_name);
}

void CUIFrameLine::InitTexture(const char* texture)
{
    string256 buf;

    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_back"), &elements[flBack]);
    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_b"), &elements[flFirst]);
    CUITextureMaster::InitTexture(strconcat(sizeof(buf), buf, texture, "_e"), &elements[flSecond]);
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::SetColor(u32 cl)
{
    for (int i = 0; i < flMax; ++i)
        elements[i].SetColor(cl);
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::UpdateSize()
{
    VERIFY(g_bRendering);
    for (int i = 0; i < flMax; ++i) // OP22
        op22_frame_part_size(elements[i]);

    float f_width = elements[flFirst].GetOriginalRect().width();
    float f_height = elements[flFirst].GetOriginalRect().height();
    elements[flFirst].SetPos(iPos.x, iPos.y);

    // Right or bottom texture
    float s_width = elements[flSecond].GetOriginalRect().width();
    float s_height = elements[flSecond].GetOriginalRect().height();

    (bHorizontalOrientation) ? elements[flSecond].SetPos(iPos.x + iSize - s_width, iPos.y) : elements[flSecond].SetPos(iPos.x, iPos.y + iSize - s_height);

    // Dimentions of element textures must be the same
    // OP22: в релизе ОП R_ASSERT не останавливает игру - разные части рамки рисуем как есть

    // Now stretch back texture to remaining space
    float back_width, back_height;

    if (bHorizontalOrientation)
    {
        back_width = iSize - f_width - s_width;
        back_height = f_height;

        // Size of frameline must be equal or greater than sum of size of two side textures
        // OP22: рамка короче своих концов (узел шириной 1 за экраном) - центр не рисуем, не вылет (вылет 08.11, «Управление»)
        if (back_width <= 0.f)
        {
            op22_short_frame_line();
            back_width = 0.f;
        }
    }
    else
    {
        back_width = f_width;
        back_height = iSize - f_height - s_height;

        // Size of frameline must be equal or greater than sum of size of two side textures
        if (back_height <= 0.f) // OP22: как по ширине
        {
            op22_short_frame_line();
            back_height = 0.f;
        }
    }

    // Now resize back texture
    float rem;
    int tile;

    float b_width = elements[flBack].GetOriginalRect().width();
    float b_height = elements[flBack].GetOriginalRect().height();

    if (b_width <= 0.f || b_height <= 0.f) // OP22: деление на ноль
    {
        elements[flBack].SetTile(0, 0, 0, 0);
    }
    else if (bHorizontalOrientation)
    {
        rem = fmod(back_width, b_width);
        tile = iFloor(back_width / b_width);
        elements[flBack].SetPos(iPos.x + f_width, iPos.y);
        elements[flBack].SetTile(tile, 1, rem, 0);
    }
    else
    {
        rem = fmod(back_height, b_height);
        tile = iFloor(back_height / b_height);
        elements[flBack].SetPos(iPos.x, iPos.y + f_height);
        elements[flBack].SetTile(1, tile, 0, rem);
    }

    uFlags |= flValidSize;
}

//////////////////////////////////////////////////////////////////////////

void CUIFrameLine::Render()
{
    // If size changed - update size
    if (!(uFlags & flValidSize))
        UpdateSize();
    // Now render all statics
    for (int i = 0; i < flMax; ++i)
    {
        elements[i].Render();
    }
}
#include "stdafx.h"
#include "dxUIRender.h"

#include "dxUIShader.h"

dxUIRender UIRenderImpl;

void op22_uixf_tl(FVF::TL* v, const op22_uixf_t& xf, float x, float y, u32 c, float u, float tv)
{
    float X, Y, W;
    xf.map(x, y, X, Y, W);
    // stub_notransform_t: HPos.xy = (P.xy + 0.5) * 2 / screen_res - 1, HPos.zw = P.zw - точку кладём в xy, умноженные на W
    const auto* target = RImplementation.Target;
    const float rw = float(target ? target->GetActiveWidth() : Device.dwWidth);
    const float rh = float(target ? target->GetActiveHeight() : Device.dwHeight);
    const float nx = (X + .5f) * 2.f / rw - 1.f, ny = (Y + .5f) * 2.f / rh - 1.f;
    v->set((nx * W + 1.f) * rw * .5f - .5f, (ny * W + 1.f) * rh * .5f - .5f, .0001f * W, .9999f * W, xf.color(c), u, tv);
}

void dxUIRender::CreateUIGeom()
{
    hGeom_TL.create(FVF::F_TL, RImplementation.Vertex.Buffer(), nullptr);
    hGeom_LIT.create(FVF::F_LIT, RImplementation.Vertex.Buffer(), nullptr);
}

void dxUIRender::DestroyUIGeom()
{
    hGeom_TL = nullptr;
    hGeom_LIT = nullptr;
}

void dxUIRender::SetShader(IUIShader& shader)
{
    dxUIShader* pShader = smart_cast<dxUIShader*>(&shader);
    VERIFY(&pShader);
    VERIFY(pShader->hShader);
    RCache.set_Shader(pShader->hShader);
}

void dxUIRender::SetAlphaRef(int aref)
{
    // CHK_DX(HW.pDevice->SetRenderState(D3DRS_ALPHAREF,aref));
    RCache.set_AlphaRef(aref);
}

void dxUIRender::SetScissor(Irect* rect)
{
    RCache.set_Scissor(rect);
    RCache.StateManager.OverrideScissoring(rect ? true : false, TRUE);
}

void dxUIRender::GetActiveTextureResolution(Fvector2& res)
{
    CTexture* T = RCache.get_ActiveTexture(0);
    res.set(float(T->get_Width()), float(T->get_Height()));
}

LPCSTR dxUIRender::UpdateShaderName(LPCSTR tex_name, LPCSTR sh_name)
{
    string_path buff;
    const u32 v_dev = CAP_VERSION(HW.Caps.raster_major, HW.Caps.raster_minor);
    const u32 v_need = CAP_VERSION(2, 0);
    
    if ((v_dev >= v_need) && FS.exist(buff, fsgame::game_textures, tex_name, ".ogm"))
        return "hud\\movie";
    else
        return sh_name;
}

void dxUIRender::PushPoint(float x, float y, float z, u32 C, float u, float v)
{
    //.	VERIFY(m_PointType==pttLIT);
    switch (m_PointType)
    {
    case pttLIT:
        LIT_pv->set(x, y, z, C, u, v);
        ++LIT_pv;
        break;
    case pttTL:
        if (g_op22_uixf.on) // OP22
            op22_uixf_tl(TL_pv, g_op22_uixf, x, y, C, u, v);
        else
            TL_pv->set(x, y, C, u, v);
        ++TL_pv;
        break;
    }
}

void dxUIRender::StartPrimitive(u32 iMaxVerts, ePrimitiveType primType, ePointType pointType)
{
    VERIFY(PrimitiveType == ptNone);
    VERIFY(m_PointType == pttNone);

    m_iMaxVerts = iMaxVerts;
    PrimitiveType = primType;
    m_PointType = pointType;

    switch (m_PointType)
    {
    case pttLIT:
        LIT_start_pv = (FVF::LIT*)RImplementation.Vertex.Lock(m_iMaxVerts, hGeom_LIT.stride(), vOffset);
        LIT_pv = LIT_start_pv;
        break;
    case pttTL:
        TL_start_pv = (FVF::TL*)RImplementation.Vertex.Lock(m_iMaxVerts, hGeom_TL.stride(), vOffset);
        TL_pv = TL_start_pv;
        break;
    }
}

void dxUIRender::FlushPrimitive()
{
    u32 primCount = 0;
    _D3DPRIMITIVETYPE d3dPrimType = D3DPT_FORCE_DWORD;
    std::ptrdiff_t p_cnt = 0;

    switch (m_PointType)
    {
    case pttLIT:
        p_cnt = LIT_pv - LIT_start_pv;
        VERIFY(u32(p_cnt) <= m_iMaxVerts);

        RImplementation.Vertex.Unlock(u32(p_cnt), hGeom_LIT.stride());
        RCache.set_Geometry(hGeom_LIT);
        break;
    case pttTL:
        p_cnt = TL_pv - TL_start_pv;
        VERIFY(u32(p_cnt) <= m_iMaxVerts);

        RImplementation.Vertex.Unlock(u32(p_cnt), hGeom_TL.stride());
        RCache.set_Geometry(hGeom_TL);
        break;
    default: NODEFAULT;
    }

    //	Update data for primitive type
    switch (PrimitiveType)
    {
    case ptTriStrip:
        primCount = (u32)(p_cnt - 2);
        d3dPrimType = D3DPT_TRIANGLESTRIP;
        break;
    case ptTriList:
        primCount = (u32)(p_cnt / 3);
        d3dPrimType = D3DPT_TRIANGLELIST;
        break;
    case ptLineStrip:
        primCount = (u32)(p_cnt - 1);
        d3dPrimType = D3DPT_LINESTRIP;
        break;
    case ptLineList:
        primCount = (u32)(p_cnt / 2);
        d3dPrimType = D3DPT_LINELIST;
        break;
    default: NODEFAULT;
    }

    if (primCount > 0)
        RCache.Render(d3dPrimType, vOffset, primCount);

    PrimitiveType = ptNone;
    m_PointType = pttNone;
}

void dxUIRender::CacheSetXformWorld(const Fmatrix& M) { RCache.set_xform_world(M); }

void dxUIRender::CacheSetCullMode(CullMode m) { RCache.set_CullMode(CULL_NONE + m); }

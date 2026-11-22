#pragma once

#include "../../Include/xrRender/UIRender.h"
#include "../../xr_3da/op22_uixf.h"

class dxUIRender : public IUIRender
{
public:
    dxUIRender() : PrimitiveType(ptNone), m_PointType(pttNone) { ; }

    virtual void CreateUIGeom();
    virtual void DestroyUIGeom();

    virtual void SetShader(IUIShader& shader);
    virtual void SetAlphaRef(int aref);

    virtual void SetScissor(Irect* rect = nullptr);
    virtual void GetActiveTextureResolution(Fvector2& res);

    virtual void PushPoint(float x, float y, float z, u32 C, float u, float v);

    virtual void StartPrimitive(u32 iMaxVerts, ePrimitiveType primType, ePointType pointType);
    virtual void FlushPrimitive();

    virtual LPCSTR UpdateShaderName(LPCSTR tex_name, LPCSTR sh_name);

    virtual void CacheSetXformWorld(const Fmatrix& M);
    virtual void CacheSetCullMode(CullMode);

private:
    ref_geom hGeom_TL;
    ref_geom hGeom_LIT;

    ePrimitiveType PrimitiveType;
    ePointType m_PointType;

    //	Vertex buffer attributes
    u32 m_iMaxVerts;
    u32 vOffset;

    FVF::TL* TL_start_pv;
    FVF::TL* TL_pv;

    FVF::LIT* LIT_start_pv;
    FVF::LIT* LIT_pv;
};

extern dxUIRender UIRenderImpl;

// OP22: вершина TL через перспективу группы окна (op22_uixf.h): позиция, глубина в w, непрозрачность
void op22_uixf_tl(FVF::TL* v, const op22_uixf_t& xf, float x, float y, u32 c, float u, float tv);

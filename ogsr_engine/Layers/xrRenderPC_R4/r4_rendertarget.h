#pragma once

#include "../xrRender/ColorMapManager.h"

class light;
class light_Package; // OP22 (SSS 23): phase_ssfx_sss_ext

// OP22: замер фаз кадра на видеокарте (op22_gpu_stats 1) - метки времени D3D11, результат через несколько кадров
void op22_gpu_frame_begin();
void op22_gpu_mark(LPCSTR name); // конец фазы name (время от прошлой метки)
// OP22 (28.11): счётчики ламп за кадр для строки [OP22 gpu]: 0 с тенью, 1 тысяч текселей их карт теней, 2 со своим флагом лучей,
// 3 точечные без тени, 4 прожекторы без тени; op22_gpu_count - только когда замер включён
void op22_gpu_count(int what, u32 n);
void op22_gpu_frame_end();
// OP22 (28.11 perf): запросы принадлежат устройству, а не набору RT.
void op22_gpu_release_queries();

static void dummy(){}

class CRenderTarget : public IRender_Target
{
private:
    u32 dwWidth[R__NUM_CONTEXTS];
    u32 dwHeight[R__NUM_CONTEXTS];
    u32 m_renderWidth{};
    u32 m_renderHeight{};
    u32 m_displayWidth{};
    u32 m_displayHeight{};
    bool m_temporalUpscaleInput{};
    bool m_resetTemporalHistory{true};
    // OP22 (28.11 perf): история хранится отдельно для каждого метода и для прицела.
    u32 m_lastAAMethod{u32(-1)};
    float m_3dssScaleFactor{};
    void UpdateTemporalAAMethod(u32 mode);
    void CommitSceneColor(CBackend& cmd_list);
    // OP22 (28.11 perf): история отражений и активность атласа принадлежат этим RT.
    Fmatrix m_waterPreviousViewProj{};
    u32 m_waterHistoryFrame{};
    bool m_waterHistoryValid{};
    bool m_lampHistoryLive[2]{};
    // OP22 (28.11 perf): выделяем два буфера маски FSR3 при первом включении.
    ref_rt rt_fsr3_opaque, rt_fsr3_reactive;
    ref_shader s_fsr3_reactive;
    u32 dwAccumulatorClearMark;
    u32 dwFlareClearMark;

public:
    u32 dwLightMarkerID;

#ifdef DEBUG
    struct dbg_line_t
    {
        Fvector P0, P1;
        u32 color;
    };
    xr_vector<dbg_line_t> dbg_lines;
    xr_vector<Fplane> dbg_planes;
#endif

    // Base targets
    xr_vector<ref_rt> rt_base;
    ref_rt rt_Base_Depth;

    // MRT-path

    ref_rt rt_Position; // 64bit,	fat	(x,y,z,?)				(eye-space)
    ref_rt rt_Color; // 64/32bit,fat	(r,g,b,specular-gloss)	(or decompressed MET-8-8-8-8)
    ref_rt rt_Velocity; // r2_RT_velocity

    ref_rt rt_zbuffer; // r2_RT_zbuffer
    ref_rt rt_tempzb, rt_tempzb_dof;

    //
    ref_rt rt_Accumulator; // 64bit		(r,g,b,specular)
    ref_rt rt_Generic_0; // 32bit		(r,g,b,a)				// post-process, intermidiate results, etc.
    ref_rt rt_Generic_0_prev; // r2_RT_generic0_prev
    ref_rt rt_Generic_1; // 32bit		(r,g,b,a)				// post-process, intermidiate results, etc.
    ref_rt rt_Generic_scene_scratch; // low-resolution scene ping-pong target
    ref_rt rt_Generic_combine; // r2_RT_generic_combine
    ref_rt rt_Postprocess_0; // full-resolution color after temporal upscaling
    ref_rt rt_Generic_0_temp;
    ref_rt rt_Generic_combine_scope;

    //  Second viewport
    ref_rt rt_second_vp; // 32bit		(r,g,b,a) 

    ref_rt rt_dof;
    ref_rt rt_blur_2, rt_blur_h_2, rt_blur_4, rt_blur_h_4, rt_blur_8, rt_blur_h_8;
    ref_rt rt_mask_drops_blur;

    ref_rt rt_pp_bloom;

    ref_rt rt_ssfx_bloom1;
    ref_rt rt_ssfx_bloom_emissive;
    ref_rt rt_ssfx_bloom_lens;
    ref_rt rt_ssfx_bloom_tmp2;
    ref_rt rt_ssfx_bloom_tmp4;
    ref_rt rt_ssfx_bloom_tmp8;
    ref_rt rt_ssfx_bloom_tmp16;
    ref_rt rt_ssfx_bloom_tmp32;
    ref_rt rt_ssfx_bloom_tmp64;
    ref_rt rt_ssfx_bloom_tmp32_2;
    ref_rt rt_ssfx_bloom_tmp16_2;
    ref_rt rt_ssfx_bloom_tmp8_2;
    ref_rt rt_ssfx_bloom_tmp4_2;

    //	Igor: for volumetric lights
    ref_rt rt_Generic_2; // 32bit		(r,g,b,a)				// post-process, intermidiate results, etc.
    ref_rt rt_Generic_3; // 32bit		(r,g,b,a)				// post-process, intermidiate results, etc.
    ref_rt rt_Bloom_1; // 32bit, dim/4	(r,g,b,?)
    ref_rt rt_LUM_64; // 64bit, 64x64,	log-average in all components
    ref_rt rt_LUM_8; // 64bit, 8x8,		log-average in all components

    ref_rt rt_ssr1, rt_ssr2;
    // OP22 (SSS 23): AO/IL отдельными проходами
    ref_rt rt_ssfx_temp, rt_ssfx_temp2, rt_ssfx_temp3, rt_ssfx_ao, rt_ssfx_il, rt_ssfx_prevPos, rt_ssfx_motion_vectors;
    ref_rt rt_ssfx_water, rt_ssfx_water_waves; // OP22 (SSS 23): вода
    ref_rt rt_ssfx_volumetric, rt_ssfx_volumetric_tmp; // OP22 (SSS 23): объёмный свет прожекторов в уменьшенном разрешении
    ref_rt rt_ssfx, rt_ssfx_sss; // OP22 (SSS 23): экранные тени солнца
    ref_rt rt_ssfx_sss_ext, rt_ssfx_sss_ext2, rt_ssfx_sss_tmp; // OP22 (SSS 23): экранные тени ламп
    bool m_op22_ssfx_prep_done{}; // OP22: векторы движения SSS и матрицы кадра уже посчитаны в этом кадре
    Fmatrix Matrix_current{}, Matrix_previous{};

    ref_rt rt_LUM_pool[2]; // 1xfp32,1x1, exp-result -> scaler

    ref_texture t_LUM_src; // source
    ref_texture t_LUM_dest; // destination & usage for current frame

    // env
    ref_texture t_envmap_0; // env-0
    ref_texture t_envmap_1; // env-1

    // sun smap
    ref_rt rt_smap_sun_cascade[R__NUM_SUN_CASCADES];
    // rain smap
    ref_rt rt_smap_rain;
    // lights smap
    ref_rt rt_smap_lights;

    ID3DTexture2D* t_noise_surf[TEX_jitter_count];
    ref_texture t_noise[TEX_jitter_count];

    ref_shader s_puddles;

private:
    // OCCq

    ref_shader s_occq;

    // Accum
    ref_shader s_accum_mask;
    ref_shader s_accum_sun_cascade[R__NUM_SUN_CASCADES];
    ref_shader s_accum_direct_volumetric;
    ref_shader s_accum_point;
    ref_shader s_accum_spot;
    ref_shader s_accum_volume;

    ref_shader s_flare;

    //	DX10 Rain
    ref_shader s_rain;

    ref_geom g_accum_point;
    ref_geom g_accum_spot;
    ref_geom g_accum_omnipart;
    ref_geom g_flare;

    ID3DVertexBuffer* g_accum_point_vb;
    ID3DIndexBuffer* g_accum_point_ib;

    ID3DVertexBuffer* g_accum_omnip_vb;
    ID3DIndexBuffer* g_accum_omnip_ib;

    ID3DVertexBuffer* g_accum_spot_vb;
    ID3DIndexBuffer* g_accum_spot_ib;

    // Bloom
    ref_geom g_bloom_build;
    ref_geom g_bloom_filter;
    ref_shader s_bloom;
    float f_bloom_factor;

    ref_shader s_ssfx_bloom;
    ref_shader s_ssfx_bloom_lens;
    ref_shader s_ssfx_bloom_upsample;
    ref_shader s_ssfx_bloom_downsample;

    // Luminance
    ref_shader s_luminance;
    float f_luminance_adapt;

    // ScreenTriangle
    ref_geom TriangleGeom;
    ID3DVertexBuffer* TriangleVB{};
    ID3DIndexBuffer* TriangleIB{};

    // Combine
    ref_geom g_combine;
    ref_geom g_combine_2UV;
    ref_geom g_combine_cuboid;
    ref_shader s_combine;
    ref_shader s_combine_volumetric;

    ref_shader s_blur;
    ref_shader s_ssr;
    ref_shader s_ssfx_ao; // OP22 (SSS 23): shaders/r3/ssfx_ao.s - AO (0-2) и IL (3-5)
    ref_shader s_op22_ssfx_mv; // OP22: векторы движения OGSR -> формат SSS (shaders/r3/op22_ssfx_mv.s)
    ref_shader s_ssfx_water, s_ssfx_water_ssr, s_ssfx_water_blur; // OP22 (SSS 23): вода (ssfx_water*.s)
    ref_shader s_ssfx_volumetric_blur; // OP22 (SSS 23): размытие и перенос объёмного света (ssfx_volumetric_blur.s)
    ref_shader s_ssfx_sss; // OP22 (SSS 23): экранные тени солнца (ssfx_sss.s)
    ref_shader s_dof;
    ref_shader s_lut;
    ref_shader s_taa;
    ref_shader s_cas;
    ref_shader s_temporal_resolve;

    ref_shader s_rain_drops;
    ref_shader s_gasmask_dudv;
    ref_shader s_fakescope; //crookr
    ref_shader s_op22_scope2d; // OP22: живой 2D-прицел
    ref_shader s_nightvision;
    ref_shader s_heatvision;

    ref_rt rt_flares;
    ref_rt rt_fakescope; // crookr
public:
    ref_rt rt_heat;

    ref_shader s_postprocess;
    ref_geom g_postprocess;

private:
    float im_noise_time;
    u32 im_noise_shift_w;
    u32 im_noise_shift_h;

    float param_blur;
    float param_gray;
    float param_duality_h;
    float param_duality_v;
    float param_noise;
    float param_noise_scale;
    float param_noise_fps;
    u32 param_color_base;
    u32 param_color_gray;
    Fvector param_color_add;

    //	Color mapping
    float param_color_map_influence;
    float param_color_map_interpolate;
    ColorMapManager color_map_manager;

    //	Igor: used for volumetric lights
    bool m_bHasActiveVolumetric;
    bool m_bHasActiveVolumetric_spot{}; // OP22 (SSS 23): в этом кадре есть лучи прожекторов в rt_ssfx_volumetric

public:
    CRenderTarget();
    ~CRenderTarget();

    void reset_target_dimensions();

    void accum_point_geom_create();
    void accum_point_geom_destroy();
    void accum_omnip_geom_create();
    void accum_omnip_geom_destroy();
    void accum_spot_geom_create();
    void accum_spot_geom_destroy();

    void u_compute_texgen_screen(CBackend& cmd_list, Fmatrix& dest);

    ID3DRenderTargetView* get_base_rt() const { return rt_base[0/*HW.CurrentBackBuffer*/]->pRT; }
    ID3DDepthStencilView* get_base_zb() const { return rt_Base_Depth->pZRT[CHW::IMM_CTX_ID]; }

    void u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& _4, ID3DDepthStencilView* zb);
    void u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, ID3DDepthStencilView* zb);
    void u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, ID3DDepthStencilView* zb);
    void u_setrt(CBackend& cmd_list, u32 W, u32 H, ID3DRenderTargetView* _1, ID3DRenderTargetView* _2, ID3DRenderTargetView* _3, ID3DDepthStencilView* zb);

    void u_calc_tc_noise(CBackend& cmd_list, Fvector2& p0, Fvector2& p1);
    void u_calc_tc_duality_ss(Fvector2& r0, Fvector2& r1, Fvector2& l0, Fvector2& l1);

    bool u_need_CM() const;

    void phase_scene_prepare();
    void phase_scene_begin(CBackend& cmd_list);
    void phase_occq(CBackend& cmd_list);
    void phase_wallmarks(CBackend& cmd_list);
    // only sun && rain
    void phase_smap_direct(CBackend& cmd_list, light* L, u32 sub_phase);
    void phase_smap_spot_clear(CBackend& cmd_list);
    void phase_smap_spot(CBackend& cmd_list, light* L) const;
    void phase_accumulator(CBackend& cmd_list);
    void phase_vol_accumulator(CBackend& cmd_list, bool cascade);
    void phase_blur(CBackend& cmd_list);
    void phase_dof(CBackend& cmd_list);
    // OP22 (28.11 perf): одно условие для прохода DOF и копии его глубины.
    bool needs_dof() const;
    void phase_lut(CBackend& cmd_list);
    void phase_gasmask_dudv(CBackend& cmd_list);
    void phase_nightvision(CBackend& cmd_list);
    void phase_heatvision(CBackend& cmd_list); //--DSR-- HeatVision
    void phase_heatvision_overlay(CBackend& cmd_list);

    void draw_rain(CBackend& cmd_list, light& RainSetup);

    bool need_to_render_sunshafts();

    void geom_volume(CBackend& cmd_list, light* L);
    void render_volume(CBackend& cmd_list, light* L);
    void accum_direct_cascade(CBackend& cmd_list, u32 sub_phase, Fmatrix& xform, Fmatrix& xform_prev, float fBias);
    void accum_direct_blend(CBackend& cmd_list);
    void accum_direct_volumetric(CBackend& cmd_list, u32 sub_phase, u32 Offset, u32 i_offset, const Fmatrix& mShadow);
    void accum_point(CBackend& cmd_list, light* L);
    void accum_spot(CBackend& cmd_list, light* L);
    void accum_volumetric(CBackend& cmd_list, light* L);
    void phase_bloom(CBackend& cmd_list);
    void phase_ssfx_bloom(CBackend& cmd_list);
    // OP22 (SSS 23): AO/IL отдельными проходами в уменьшенном разрешении с учётом прошлого кадра
    void phase_op22_ssfx_mv(CBackend& cmd_list);
    void phase_ssfx_ao(CBackend& cmd_list);
    void phase_ssfx_il(CBackend& cmd_list);
    void phase_ssfx_water(CBackend& cmd_list); // OP22 (SSS 23): отражения, размытие, волны и сама вода
    void phase_ssfx_volumetric_blur(CBackend& cmd_list); // OP22 (SSS 23): размытие лучей прожекторов и перенос в rt_Generic_3
    void phase_ssfx_sss(CBackend& cmd_list); // OP22 (SSS 23): экранные тени солнца (до накопления солнца)
    void phase_ssfx_sss_ext(CBackend& cmd_list, const light_Package& LP); // OP22 (SSS 23): экранные тени ламп (до ламп)
    void op22_ssfx_frame_prep(CBackend& cmd_list); // OP22: раз за кадр - матрицы текущего/прошлого кадра и векторы движения SSS
    void op22_set_tiles(CBackend& cmd_list); // OP22 (28.11): op22_ssfx_tiles
    void op22_ssfx_quad(CBackend& cmd_list, const ref_rt& rt, ref_selement& sh, float vw, float vh, float uv, const std::function<void()>& lambda);
    void phase_luminance(CBackend& cmd_list);
    void phase_combine(CBackend& cmd_list);
    void phase_pp(CBackend& cmd_list);
    void phase_combine_volumetric(CBackend& cmd_list);

    void phase_rain_drops(CBackend& cmd_list);
    void phase_fakescope(CBackend& cmd_list); // crookr
    void phase_flares(CBackend& cmd_list);

    void render_flare(CBackend& cmd_list, light* L);

    virtual void set_blur(float f) { param_blur = f; }
    virtual void set_gray(float f) { param_gray = f; }
    virtual void set_duality_h(float f) { param_duality_h = _abs(f); }
    virtual void set_duality_v(float f) { param_duality_v = _abs(f); }
    virtual void set_noise(float f) { param_noise = f; }
    virtual void set_noise_scale(float f) { param_noise_scale = f; }
    virtual void set_noise_fps(float f) { param_noise_fps = _abs(f) + EPS_S; }
    virtual void set_color_base(u32 f) { param_color_base = f; }
    virtual void set_color_gray(u32 f) { param_color_gray = f; }
    virtual void set_color_add(const Fvector& f) { param_color_add = f; }

    virtual u32 get_width(CBackend& cmd_list) { return dwWidth[cmd_list.context_id]; }
    virtual u32 get_height(CBackend& cmd_list) { return dwHeight[cmd_list.context_id]; }

    u32 GetRenderWidth() const { return m_renderWidth; }
    u32 GetRenderHeight() const { return m_renderHeight; }
    u32 GetDisplayWidth() const { return m_displayWidth; }
    u32 GetDisplayHeight() const { return m_displayHeight; }
    u32 GetActiveWidth() const { return m_temporalUpscaleInput ? m_renderWidth : m_displayWidth; }
    u32 GetActiveHeight() const { return m_temporalUpscaleInput ? m_renderHeight : m_displayHeight; }
    u32 GetViewportWidth(CBackend& cmd_list) const { return dwWidth[cmd_list.context_id]; }
    u32 GetViewportHeight(CBackend& cmd_list) const { return dwHeight[cmd_list.context_id]; }
    float GetRenderSubrectScaleX() const { return static_cast<float>(m_renderWidth) / static_cast<float>(m_displayWidth); }
    float GetRenderSubrectScaleY() const { return static_cast<float>(m_renderHeight) / static_cast<float>(m_displayHeight); }
    bool IsTemporalUpscaleInput() const { return m_temporalUpscaleInput; }

    void SetTemporalRenderSize(u32 renderWidth, u32 renderHeight, u32 displayWidth, u32 displayHeight);
    void ConfigureTemporalRenderSize();
    void BeginTemporalUpscaleInput() { m_temporalUpscaleInput = true; }
    void EndTemporalUpscaleInput() { m_temporalUpscaleInput = false; }
    void ResetTemporalHistory();

    virtual void set_cm_imfluence(float f) { param_color_map_influence = f; }
    virtual void set_cm_interpolate(float f) { param_color_map_interpolate = f; }
    virtual void set_cm_textures(const shared_str& tex0, const shared_str& tex1) { color_map_manager.SetTextures(tex0, tex1); }

    //	Need to reset stencil only when marker overflows.
    //	Don't clear when render for the first time
    void reset_light_marker(CBackend& cmd_list, bool bResetStencil = false);
    void increment_light_marker(CBackend& cmd_list);

#ifdef DEBUG
    IC void dbg_addline(Fvector& P0, Fvector& P1, u32 c)
    {
        dbg_lines.push_back(dbg_line_t());
        dbg_lines.back().P0 = P0;
        dbg_lines.back().P1 = P1;
        dbg_lines.back().color = c;
    }
    IC void dbg_addplane(Fplane& P0, u32 c) { dbg_planes.push_back(P0); }
#else
    IC void dbg_addline(Fvector& P0, Fvector& P1, u32 c) {}
    IC void dbg_addplane(Fplane& P0, u32 c) {}
#endif

private:
    void RenderScreenTriangle(CBackend& cmd_list, const ref_rt& rt, ref_selement& sh, const std::function<void()>& lambda = dummy);
    void RenderScreenQuad(CBackend& cmd_list, const u32 w, u32 const h, const ref_rt& rt, ref_selement& sh, const std::function<void()>& lambda = dummy);

    // Anti Aliasing
    ref_shader s_pp_antialiasing;
    ref_rt rt_smaa_edgetex;
    ref_rt rt_smaa_blendtex;

    bool PhaseAA(CBackend& cmd_list, ID3D11Resource* reactiveMask); // OP22 (28.11 perf): передаём маску этого прохода.
    bool Phase3DSSUpscale(bool needScope); // OP22 (28.11 perf): учитываем вход в прицел и снижение масштаба.

    void ProcessSMAA(CBackend& cmd_list);
    void ProcessTAA(CBackend& cmd_list);

    void InitDLSS();
    bool ProcessDLSS();
    void DestroyDLSS();

    void InitFSR();
    bool ProcessFSR(ID3D11Resource* reactiveMask = nullptr); // OP22 (28.11 perf): необязательная маска сцены.
    bool ProcessFSR_3DSS(const bool need_reset);
    void DestroyFSR();

    bool reset_3dss_rendertarget(const bool need_reset = false);

    void ProcessCAS(CBackend& cmd_list);
    void BeginPostprocess(CBackend& cmd_list, bool temporalOutput);

    void PhaseSSSS(CBackend& cmd_list);

    ref_rt rt_sunshafts_0; // ss0
    ref_rt rt_sunshafts_1; // ss1
    ref_rt rt_SunShaftsMask;
    ref_rt rt_SunShaftsMaskSmoothed;
    ref_rt rt_SunShaftsPass0;
    ref_shader s_ssss_mrmnwar;
    ref_shader s_ssss_ogse;

public:
    ID3D11Texture2D* m_ImguiTex{};
    ID3D11ShaderResourceView* m_ImguiSRV{};

};

#include "stdafx.h"
#include "../../xr_3da/igame_persistent.h"
#include "../../xr_3da/environment.h"
#include "../../xr_3da/xr_efflensflare.h"
#include "../../xr_3da/CustomHUD.h"
#include "../xrRender/dxEnvironmentRender.h"

void CRenderTarget::phase_combine(CBackend& cmd_list)
{
    ZoneScoped;

    //*** exposure-pipeline
    {
        // if (t_LUM_src != rt_LUM_pool[0]->pTexture)
            t_LUM_src->surface_set(rt_LUM_pool[0]->pSurface);
        // if (t_LUM_dest != rt_LUM_pool[1]->pTexture)
            t_LUM_dest->surface_set(rt_LUM_pool[1]->pSurface);
    }

    // OP22 (SSS 23): лучи прожекторов - размытие в уменьшенном разрешении и перенос в rt_Generic_3 (до сведения)
    if (RImplementation.o.ssfx_volumetric && m_bHasActiveVolumetric_spot)
        phase_ssfx_volumetric_blur(cmd_list);
    op22_gpu_mark("beams_blur");
    // OP22: в кадре без лучей rt_Generic_3 не очищался - капли дождя (effects_rain.ps слоя) читают его
    if (RImplementation.o.ssfx_volumetric && !m_bHasActiveVolumetric)
        cmd_list.ClearRT(rt_Generic_3, {});

    // OP22 (SSS 23, Ascii1457; перенос из xray-monolith / SSS UPDATE 23.2): AO и IL отдельными проходами до combine_1.
    // Матрицы текущего/прошлого кадра - для учёта прошлого кадра в шейдерах (как Matrix_current/previous в monolith).
    if (ps_op22_ssfx_ao_pass || ps_op22_ssfx_il_pass || RImplementation.o.ssfx_sss)
    {
        PIX_EVENT(op22_ssfx_ao_il);
        op22_ssfx_frame_prep(cmd_list); // матрицы и векторы движения (если экранные тени уже не посчитали их до солнца)

        if (ps_op22_ssfx_ao_pass || ps_op22_ssfx_il_pass)
        {
            Fcolor clear_color;
            clear_color.set(0.f, 0.f, 0.f, 1.f);
            cmd_list.ClearRT(rt_ssfx_temp->pRT, clear_color);
            cmd_list.ClearRT(rt_ssfx_temp2->pRT, clear_color);
        }

        if (ps_op22_ssfx_ao_pass && ps_ssfx_ao.y > 0)
            phase_ssfx_ao(cmd_list);
        if (ps_op22_ssfx_il_pass && ps_ssfx_il.y > 0)
            phase_ssfx_il(cmd_list);

        // позиции этого кадра - «прошлые» для следующего
        HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_prevPos->pSurface, rt_Position->pSurface);
    }

    op22_gpu_mark("ao_il");

    u_setrt(cmd_list, rt_Generic_0, nullptr, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]);
    cmd_list.set_CullMode(CULL_NONE);
    cmd_list.set_Stencil(FALSE);

    {
        PIX_EVENT(CLOUDS_RENDER);

        g_pGamePersistent->Environment().RenderClouds(cmd_list);
    }

    cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00); // stencil should be >= 1

    // Draw full-screen quad textured with our scene image
    {
        PIX_EVENT(combine_1);

        // Compute params
        CEnvDescriptorMixer& envdesc = *g_pGamePersistent->Environment().CurrentEnv;
        const float minamb = ps_pnv_mode == 1 ? 0.005f : 0.001f;
        Fvector4 ambclr = {std::max(envdesc.ambient.x * 2, minamb), std::max(envdesc.ambient.y * 2, minamb), std::max(envdesc.ambient.z * 2, minamb), 0};
        ambclr.mul(ps_r2_sun_lumscale_amb);

        Fvector4 envclr;

        if (g_pGamePersistent->Environment().USED_COP_WEATHER)
            envclr = {envdesc.hemi_color_unmodded.x * 2 + EPS, envdesc.hemi_color_unmodded.y * 2 + EPS, envdesc.hemi_color_unmodded.z * 2 + EPS, envdesc.weight};
        else
            envclr = {envdesc.sky_color.x * 2 + EPS, envdesc.sky_color.y * 2 + EPS, envdesc.sky_color.z * 2 + EPS, envdesc.weight};

        Fvector4 fogclr = {envdesc.fog_color.x, envdesc.fog_color.y, envdesc.fog_color.z, 0};
        envclr.x *= 2 * ps_r2_sun_lumscale_hemi;
        envclr.y *= 2 * ps_r2_sun_lumscale_hemi;
        envclr.z *= 2 * ps_r2_sun_lumscale_hemi;
        Fvector4 sunclr, sundir;

        // sun-params
        {
            light* fuckingsun = smart_cast<light*>(RImplementation.Lights.sun_adapted._get());
            Fvector L_dir, L_clr;
            float L_spec;
            L_clr.set(fuckingsun->color.r, fuckingsun->color.g, fuckingsun->color.b);
            L_spec = u_diffuse2s(L_clr);
            Device.mView.transform_dir(L_dir, fuckingsun->direction);
            L_dir.normalize();

            sunclr.set(L_clr.x, L_clr.y, L_clr.z, L_spec);
            sundir.set(L_dir.x, L_dir.y, L_dir.z, 0);
        }

        dxEnvDescriptorMixerRender& envdescren = *smart_cast<dxEnvDescriptorMixerRender*>(&*envdesc.m_pDescriptorMixer);

        // Setup textures
        ID3DBaseTexture* e0 = envdescren.sky_r_textures_env[0].second->surface_get();
        ID3DBaseTexture* e1 = envdescren.sky_r_textures_env[1].second->surface_get();
        t_envmap_0->surface_set(e0);
        t_envmap_1->surface_set(e1);

        // Draw
        cmd_list.set_Element(s_combine->E[0]);
        cmd_list.set_Geometry(TriangleGeom);

        cmd_list.set_c("m_inv_v", Device.mInvView);

        cmd_list.set_c("m_v2w", Device.mInvView);
        cmd_list.set_c("L_ambient", ambclr);

        cmd_list.set_c("Ldynamic_color", sunclr);
        cmd_list.set_c("Ldynamic_dir", sundir);

        cmd_list.set_c("env_color", envclr);
        cmd_list.set_c("fog_color", fogclr);

        cmd_list.Render(D3DPT_TRIANGLELIST, 0, 0, 3, 0, 1);
    }

    op22_gpu_mark("clouds_combine1");

    // Copy rt_Generic_0 to new RT
    HW.get_context(cmd_list.context_id)->CopyResource(rt_Generic_0_temp->pSurface, rt_Generic_0->pSurface);

    // OP22: op22_ssr_always 1 - отражения SSR всегда, а не только при влажности (дождь)
    if (ps_r2_ls_flags_ext.test(R2FLAGEXT_SSLR) && (ps_op22_ssr_always || !fis_zero(g_pGamePersistent->Environment().wetness_factor)))
    {
        PIX_EVENT(PHASE_SSR_BLUR);

        Fmatrix m_previous{}, m_current{};
        {
            static Fmatrix m_saved_viewproj;

            m_previous.mul(m_saved_viewproj, Device.mInvView);
            m_current.set(Device.mProject);
            m_saved_viewproj.set(Device.mFullTransform);
        }

        // OP22: ssr_setup.x > 1 - шейдер пишет результат в левую верхнюю часть (tc * x), остальное не читается:
        // ограничиваем растеризацию этой частью, иначе пониженное разрешение ничего не экономит
        const bool ssr_scissor = ps_ssfx_ssr_1.x > 1.01f;
        Irect ssr_rect{0, 0, iCeil(rt_ssr2->dwWidth / ps_ssfx_ssr_1.x) + 1, iCeil(rt_ssr2->dwHeight / ps_ssfx_ssr_1.x) + 1};
        RenderScreenTriangle(cmd_list, rt_ssr2, s_ssr->E[0], [&]() {
            cmd_list.set_c("ssr_setup", ps_ssfx_ssr_1);
            cmd_list.set_c("ssfx_ssr_2", ps_ssfx_ssr_2);
            cmd_list.set_c("m_current", m_current);
            cmd_list.set_c("m_previous", m_previous);
            if (ssr_scissor)
                cmd_list.set_Scissor(&ssr_rect);
        });
        if (ssr_scissor)
            cmd_list.set_Scissor(nullptr);

        HW.get_context(CHW::IMM_CTX_ID)->CopyResource(rt_ssr1->pSurface, rt_ssr2->pSurface);

        RenderScreenTriangle(cmd_list, rt_Generic_scene_scratch, s_ssr->E[1], [&]() {
            cmd_list.set_c("ssr_setup", ps_ssfx_ssr_1);
            cmd_list.set_c("ssfx_ssr_2", ps_ssfx_ssr_2);
        });

        HW.get_context(CHW::IMM_CTX_ID)->CopyResource(rt_Generic_0->pSurface, rt_Generic_scene_scratch->pSurface);
    }

    op22_gpu_mark("ssr");

    // OP22 (SSS 23): обычная вода - отражения в уменьшенном разрешении, размытие, волны и сама вода (до прямого прохода)
    if (RImplementation.o.ssfx_water)
        phase_ssfx_water(cmd_list);
    op22_gpu_mark("water");

    // OP22 (SSS 23): кадр для преломления стекла - уже с водой (как copy_glass xray-monolith)
    if (RImplementation.o.ssfx_glass)
        HW.get_context(cmd_list.context_id)->CopyResource(rt_Generic_0_temp->pSurface, rt_Generic_0->pSurface);

    // Forward rendering
    {
        PIX_EVENT(render_forward);

        u_setrt(cmd_list, rt_Generic_0, rt_heat, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]); // LDR RT

        cmd_list.set_CullMode(CULL_CCW);
        cmd_list.set_Stencil(FALSE);
        cmd_list.set_ColorWriteEnable();

        RImplementation.render_forward();
    }
    op22_gpu_mark("forward_rain_glass_particles");

    auto& dsgraph = RImplementation.get_imm_context();

    if (g_hud && g_hud->RenderActiveItemUIQuery())
        dsgraph.r_dsgraph_render_hud_ui();

    //	Igor: for volumetric lights
    //	combine light volume here
    if (m_bHasActiveVolumetric)
        phase_combine_volumetric(cmd_list);
    op22_gpu_mark("hud_ui_beams_combine");

    // Distortion filter
    if (RImplementation.o.distortion_enabled && !dsgraph.mapDistort.empty())
    {
        {
            PIX_EVENT(render_distort_objects);
            u_setrt(cmd_list, rt_Generic_1, nullptr, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]); // Now RT is a distortion mask
            cmd_list.ClearRT(rt_Generic_1, color_rgba(127, 127, 0, 127));
            cmd_list.set_CullMode(CULL_CCW);
            cmd_list.set_Stencil(FALSE);
            cmd_list.set_ColorWriteEnable();
            dsgraph.r_dsgraph_render_distort();
        }

        {
            PIX_EVENT(combine_distort);
            RenderScreenTriangle(cmd_list, rt_Generic_scene_scratch, s_combine->E[1]);
            HW.get_context(cmd_list.context_id)->CopyResource(rt_Generic_0->pSurface, rt_Generic_scene_scratch->pSurface);
        }
    }

    op22_gpu_mark("distort"); // OP22: подфаза блока постобработки (op22_gpu_stats)
    {
        // Perform blooming filter and distortion if needed
        cmd_list.set_Stencil(FALSE);
        phase_bloom(cmd_list); // HDR RT invalidated here
    }
    op22_gpu_mark("bloom_old"); // OP22: подфаза блока постобработки (op22_gpu_stats)

    {
        PIX_EVENT(combine_tonemap);
        RenderScreenTriangle(cmd_list, rt_Generic_scene_scratch, s_combine->E[2]);
        HW.get_context(cmd_list.context_id)->CopyResource(rt_Generic_0->pSurface, rt_Generic_scene_scratch->pSurface);
    }

    op22_gpu_mark("tonemap"); // OP22: подфаза блока постобработки (op22_gpu_stats)
    const bool need_heatvision = (ps_pnv_mode == 2 || ps_pnv_mode == 3);
    const bool need_3dss = !dsgraph.mapScopeHUDSorted.empty();

    // Stable false-color at render resolution so DLSS/FSR can reconstruct heat shapes.
    if (need_heatvision)
        phase_heatvision(cmd_list);

    const bool upscaled_3dss = (need_3dss && Phase3DSSUpscale(cmd_list));

    bool temporalOutput{false};
    if (ps_r_pp_aa_mode) // должно быть перед 3DSS
    {
        temporalOutput = PhaseAA(cmd_list); // anti - aliasing
    }
    else
    {
        EndTemporalUpscaleInput();
        RImplementation.rmNormal(cmd_list);
        BeginPostprocess(cmd_list, false);
    }

    //Костыль для 3DSS. Там юзается текстура из rt_Generic_combine, а она приходит из DLSS/FSR, а если они выключены, то изображение брать не откуда.
    if (need_3dss && !upscaled_3dss && !temporalOutput)
    {
        HW.get_context(cmd_list.context_id)->CopyResource(rt_Generic_combine->pSurface, rt_Postprocess_0->pSurface);
    }

    op22_gpu_mark("aa_3dss_upscale"); // OP22: подфаза блока постобработки (op22_gpu_stats)
    // Sensor noise / lens after temporal upscaling. Before 3DSS so the reticle is not thermal-tinted.
    if (need_heatvision)
        phase_heatvision_overlay(cmd_list);

    if (!need_heatvision) // должны быть до 3DSS
    { // Screen space sunshafts
#pragma todo("Simp: Ощущение что есть микроподергивания лучей (из-за TAA jitter?)")
        PIX_EVENT(phase_ss_ss);
        PhaseSSSS(cmd_list);
    }
    op22_gpu_mark("sunshafts_ss"); // OP22: подфаза блока постобработки (op22_gpu_stats)

    if (need_3dss)
    {
        PIX_EVENT(phase_3DSSReticle);

        // The reticle is composited after temporal upscaling. Scene depth is
        // render-sized and cannot be bound with this display-sized color RT.
        u_setrt(cmd_list, rt_Postprocess_0, nullptr, nullptr, nullptr, nullptr);
        RImplementation.rmNormal(cmd_list);

        cmd_list.set_CullMode(CULL_CCW);
        cmd_list.set_Stencil(FALSE);
        cmd_list.set_ColorWriteEnable();

        dsgraph.r_dsgraph_render_scope_sorted(upscaled_3dss);
    }

    op22_gpu_mark("scope3dss"); // OP22: подфаза блока постобработки (op22_gpu_stats)
    // Compute blur textures
    phase_blur(cmd_list);
    op22_gpu_mark("blur"); // OP22: подфаза блока постобработки (op22_gpu_stats)

#pragma todo("Simp: Если отключить новый блум, то старый, который рендерится до прицела, багует, потому что рендрится очень рано. Пока запретил отключение нового блума.")
    if (ps_r2_ls_flags.test(R2FLAG_SSFX_BLOOM))
        phase_ssfx_bloom(cmd_list);
    op22_gpu_mark("bloom_ssfx"); // OP22: подфаза блока постобработки (op22_gpu_stats)

    if (!need_heatvision)
        phase_dof(cmd_list);

    if (!need_heatvision)
        phase_lut(cmd_list);
    op22_gpu_mark("dof_lut"); // OP22: подфаза блока постобработки (op22_gpu_stats)

/*
    if (scope_fake_radius > 0.f)
        phase_fakescope(cmd_list); // crookr
*/

    // OP22 (26.11): живой 2D-прицел - оптика над изображением мира (до маски/ПНВ: они ближе к глазу, чем прицел)
    if (Device.op22_scope2d.w > 0.f && Device.op22_scope2d.z > 0.f && Device.dwFrame - Device.op22_scope2d_frame <= 1)
    {
        PIX_EVENT(phase_op22_scope2d);
        RenderScreenTriangle(cmd_list, rt_Generic_combine, s_op22_scope2d->E[0]);
        HW.get_context(cmd_list.context_id)->CopyResource(rt_Postprocess_0->pSurface, rt_Generic_combine->pSurface);
    }

#pragma todo("Simp: эффект шлема багует в прицеле, надо бы придумать решение получше")
    extern bool Is3dssZoomed;
    if (!Is3dssZoomed &&
        ((ps_r2_ls_flags_ext.test(R2FLAGEXT_VISOR_REFL_CONTROL) && ps_r2_ls_flags_ext.test(R2FLAGEXT_VISOR_REFL)) ||
         (ps_r2_ls_flags_ext.test(R2FLAGEXT_MASK_CONTROL) && ps_r2_ls_flags_ext.test(R2FLAGEXT_MASK))))
    {
        phase_gasmask_dudv(cmd_list);
    }

    if (ps_pnv_mode == 1) // должен быть после дофа и после шлема
        phase_nightvision(cmd_list);

    // Rain droplets on screen
    if (ps_r2_ls_flags_ext.test(R2FLAGEXT_RAIN_DROPS))
        phase_rain_drops(cmd_list);

    op22_gpu_mark("scope2d_mask_nv_drops"); // OP22: подфаза блока постобработки (op22_gpu_stats)
    {
        PIX_EVENT(combine_2);

        Fmatrix m_previous{}, m_current{};

        static Fmatrix m_saved_viewproj;

        // (new-camera) -> (world) -> (old_viewproj)
        m_previous.mul(m_saved_viewproj, Device.mInvView);
        m_current.set(Device.mProject);
        m_saved_viewproj.set(Device.mFullTransform);

        Fvector2 m_blur_scale{};
        if (!Is3dssZoomed)
        {
            const float scale = ps_r2_mblur / 2.f;
            m_blur_scale.set(scale, -scale).div(12.f);
        }

        RenderScreenTriangle(cmd_list, rt_Generic_combine, s_combine->E[3], [&]() {
            cmd_list.set_c("m_current", m_current);
            cmd_list.set_c("m_previous", m_previous);
            cmd_list.set_c("m_blur", m_blur_scale.x, m_blur_scale.y, 0, 0);
        });

        HW.get_context(cmd_list.context_id)->CopyResource(rt_Postprocess_0->pSurface, rt_Generic_combine->pSurface);
    }

    {
        PIX_EVENT(RenderFlares);
        cmd_list.set_Stencil(FALSE);
        g_pGamePersistent->Environment().RenderFlares(cmd_list, FALSE, ps_r2_ls_flags_ext.test(R2FLAGEXT_LENS_FLARE) && r_lens_flare_mode == old_style_flare, TRUE);
    }

    op22_gpu_mark("combine2_flares"); // OP22: до 3.270 весь блок от distort - одной меткой distort_tonemap_aa_bloom

    //	PP-if required
    {
        PIX_EVENT(phase_pp);
        phase_pp(cmd_list);
    }

    {
        //	Re-adapt luminance
        cmd_list.set_Stencil(FALSE);

        //*** exposure-pipeline-clear
        {
            std::swap(rt_LUM_pool[0], rt_LUM_pool[1]);

            t_LUM_src->surface_set(nullptr);
            t_LUM_dest->surface_set(nullptr);
        }
    }

    phase_flares(cmd_list);

    u_setrt(cmd_list, Device.dwWidth, Device.dwHeight, get_base_rt(), nullptr, nullptr, nullptr);
    RImplementation.rmNormal(cmd_list);

#ifdef DEBUG
    cmd_list.set_CullMode(CULL_CCW);
    static xr_vector<Fplane> saved_dbg_planes;
    if (bDebug)
        saved_dbg_planes = dbg_planes;
    else
        dbg_planes = saved_dbg_planes;
    if (1)
        for (u32 it = 0; it < dbg_planes.size(); it++)
        {
            Fplane& P = dbg_planes[it];
            Fvector zero;
            zero.mul(P.n, P.d);

            Fvector L_dir, L_up = P.n, L_right;
            L_dir.set(0, 0, 1);
            if (_abs(L_up.dotproduct(L_dir)) > .99f)
                L_dir.set(1, 0, 0);
            L_right.crossproduct(L_up, L_dir);
            L_right.normalize();
            L_dir.crossproduct(L_right, L_up);
            L_dir.normalize();

            Fvector p0, p1, p2, p3;
            float sz = 100.f;
            p0.mad(zero, L_right, sz).mad(L_dir, sz);
            p1.mad(zero, L_right, sz).mad(L_dir, -sz);
            p2.mad(zero, L_right, -sz).mad(L_dir, -sz);
            p3.mad(zero, L_right, -sz).mad(L_dir, +sz);
            cmd_list.dbg_DrawTRI(Fidentity, p0, p1, p2, 0xffffffff);
            cmd_list.dbg_DrawTRI(Fidentity, p2, p3, p0, 0xffffffff);
        }

    static xr_vector<dbg_line_t> saved_dbg_lines;
    if (bDebug)
        saved_dbg_lines = dbg_lines;
    else
        dbg_lines = saved_dbg_lines;
    if (1)
        for (u32 it = 0; it < dbg_lines.size(); it++)
        {
            cmd_list.dbg_DrawLINE(Fidentity, dbg_lines[it].P0, dbg_lines[it].P1, dbg_lines[it].color);
        }
#endif

#ifdef DEBUG
    dbg_lines.clear();
    dbg_planes.clear();
#endif
}

void CRenderTarget::phase_wallmarks(CBackend& cmd_list)
{
    // Targets
    u_setrt(cmd_list, rt_Color, nullptr, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]);
    // Stencil	- draw only where stencil >= 0x1
    cmd_list.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00);
    cmd_list.set_CullMode(CULL_CCW);
    cmd_list.set_ColorWriteEnable(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
}

void CRenderTarget::phase_combine_volumetric(CBackend& cmd_list)
{
    PIX_EVENT(phase_combine_volumetric);

    u_setrt(cmd_list, rt_Generic_0, nullptr, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]);

    //	Sets limits to both render targets
    cmd_list.set_ColorWriteEnable(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);

    // Draw
    cmd_list.set_Element(s_combine_volumetric->E[0]);
    cmd_list.set_Geometry(TriangleGeom);
    cmd_list.Render(D3DPT_TRIANGLELIST, 0, 0, 3, 0, 1);
    cmd_list.set_ColorWriteEnable();
}

void CRenderTarget::phase_flares(CBackend& cmd_list)
{
    if (!rt_flares)
        return;

    // clear
    // Flares are generated after temporal upscaling and consumed by the
    // display-sized combine pass. Keep their target and viewport in the
    // display domain; the flare shader performs its own depth lookup.
    u_setrt(cmd_list, rt_flares, nullptr, nullptr, nullptr, nullptr);
    RImplementation.rmNormal(cmd_list);

    // Targets
    if (dwFlareClearMark != Device.dwFrame)
    {
        // initial setup
        dwFlareClearMark = Device.dwFrame;

        cmd_list.ClearRT(rt_flares->pRT, {});            
    }
}

void CRenderTarget::render_flare(CBackend& cmd_list, light* L)
{
    if (!rt_flares)
        return;

    Fvector vLightDir{}, vecY{};
    vLightDir.sub(L->position, Device.vCameraPosition);

    // Calculate the point directly in front of us, on the far clip plane
    float fDistance = vLightDir.magnitude();

    Fmatrix matEffCamPos;
    matEffCamPos.identity();

    // Calculate our position and direction
    matEffCamPos.i.set(Device.vCameraRight);
    matEffCamPos.j.set(Device.vCameraTop);
    matEffCamPos.k.set(Device.vCameraDirection);

    Fvector vecDir = {0.0f, 0.0f, 1.0f};
    matEffCamPos.transform_dir(vecDir);
    vecDir.normalize();

    // Figure out if light is behind something else
    Fvector vecX = {1.0f, 0.0f, 0.0f};
    matEffCamPos.transform_dir(vecX);
    vecX.normalize();
    vecY.crossproduct(vecX, vecDir);
    vLightDir.normalize();

    // Figure out of light (or flare) might be visible
    float fDot = vLightDir.dotproduct(vecDir);
    if (fDot <= 0.01f)
    {
        L->fBlend = 0.f;
        return;
    }

    float cur_dist = fDistance;
    {
        L->FlareRayPick.RayPickSubmit(Device.vCameraPosition, vLightDir, cur_dist, collide::rqtBoth, g_pGameLevel->CurrentViewEntity());

        collide::rq_result RQ;
        if (L->FlareRayPick.Ready(RQ))
        {
            cur_dist = RQ.range;
        }
    }
    bool hide = (fDistance - cur_dist) > 0.2f;

    blend_lerp(L->fBlend, hide ? 0.0f : 1.0f, hide ? 8.0f : 4.0f, Device.fTimeDelta);
    clamp(L->fBlend, 0.0f, 1.0f);

    if (L->fBlend <= EPS_L)
    {
        return;
    }

    phase_flares(cmd_list);

    Fvector vecLight = L->position;

    Fvector scr_pos{};
    Device.mFullTransform.transform(scr_pos, vecLight);

    scr_pos.y *= -1.0f;

    const float sun_blend = 0.5f;
    const float sun_max = 2.5f;

    Fvector2 kXY{1.f, 1.f};
    if (_abs(scr_pos.x) > sun_blend)
        kXY.x = (sun_max - _abs(scr_pos.x)) / (sun_max - sun_blend);
    if (_abs(scr_pos.y) > sun_blend)
        kXY.y = (sun_max - _abs(scr_pos.y)) / (sun_max - sun_blend);

    float fGradientValue = 0;
    if (!((_abs(scr_pos.x) > sun_max) || (_abs(scr_pos.y) > sun_max)))
        fGradientValue = kXY.x * kXY.y * L->fBlend;

    Fvector vecSx{}, vecSy{};
    vecSx.mul(vecX, fGradientValue * (1.f + 0.02f * fDistance) * 3.f);
    vecSy.mul(vecY, fGradientValue * (1.f + 0.02f * fDistance) * 3.f);

    Fcolor color = L->color;
    color.normalize_rgb();
    color.mul_rgba(fGradientValue /* * L->fBlend*/);
    u32 c = color.get();

    u32 VS_Offset;
    FVF::LIT* pv = (FVF::LIT*)RImplementation.Vertex.Lock(4, g_flare.stride(), VS_Offset);

    pv->set(vecLight.x + vecSx.x - vecSy.x, vecLight.y + vecSx.y - vecSy.y, vecLight.z + vecSx.z - vecSy.z, c, 0, 0);
    pv++;
    pv->set(vecLight.x + vecSx.x + vecSy.x, vecLight.y + vecSx.y + vecSy.y, vecLight.z + vecSx.z + vecSy.z, c, 0, 1);
    pv++;
    pv->set(vecLight.x - vecSx.x - vecSy.x, vecLight.y - vecSx.y - vecSy.y, vecLight.z - vecSx.z - vecSy.z, c, 1, 0);
    pv++;
    pv->set(vecLight.x - vecSx.x + vecSy.x, vecLight.y - vecSx.y + vecSy.y, vecLight.z - vecSx.z + vecSy.z, c, 1, 1);
    pv++;

    RImplementation.Vertex.Unlock(4, g_flare.stride());

    cmd_list.set_xform_world(Fidentity);
    cmd_list.set_Geometry(g_flare);
    cmd_list.set_Stencil(FALSE);
    cmd_list.set_CullMode(CULL_NONE);
    cmd_list.set_Shader(s_flare);

    float intensity_invert;

    if (cur_dist > 5.0f)
        intensity_invert = std::lerp(0.0f, 1.0f, cur_dist / 100.f);
    else
        intensity_invert = 1 - std::lerp(0.0f, 1.0f, cur_dist / 5.f);


    cmd_list.set_c("flare_params", intensity_invert, L->flags.bFlare ? 1.f : 0.f, 0.f, 0.f);
    cmd_list.set_c("flare_color", L->color.r, L->color.g, L->color.b, L->color.a);


    cmd_list.Render(D3DPT_TRIANGLELIST, VS_Offset, 0, 4, 0, 2);
}

// ---------------------------------------------------------------------------------------------------------------------
// OP22 (SSS 23): AO/IL отдельными проходами. Перенос phase_ssfx_ao/phase_ssfx_il (xray-monolith,
// r4_rendertarget_phase_ssao.cpp; SSS UPDATE 23.2, Ascii1457) под CBackend OGSR.

// Экранный прямоугольник: вьюпорт vw x vh пикселей, текстурные координаты 0..uv (как p1 = 1/ScaleFactor в monolith -
// шейдеры SSS сами умножают tc на ao_setup.x, а hpos - пиксель уменьшенного вьюпорта).
// OP22 (3.258): вершины - сразу в пространстве отсечения (-1..1), как TriangleGeom: stub_screen_space.vs OGSR
// передаёт HPos без перевода (у monolith он переводит пиксели через screen_res). В 3.256-3.257 здесь были пиксели
// (0..vw) - прямоугольник закрывал только угол вьюпорта, проходы AO/IL SSS 23 почти не работали.
void CRenderTarget::op22_ssfx_quad(CBackend& cmd_list, const ref_rt& rt, ref_selement& sh, float vw, float vh, float uv, const std::function<void()>& lambda)
{
    u32 Offset = 0;
    constexpr float d_Z = EPS_S;
    constexpr float d_W = 1.0f;
    constexpr u32 C = color_rgba(0, 0, 0, 255);

    u_setrt(cmd_list, rt->dwWidth, rt->dwHeight, rt->pRT, nullptr, nullptr, nullptr);
    cmd_list.SetViewport({0.f, 0.f, vw, vh, 0.f, 1.f});

    cmd_list.set_CullMode(CULL_NONE);
    cmd_list.set_Stencil(FALSE);

    FVF::TL* pv = (FVF::TL*)RImplementation.Vertex.Lock(4, g_combine->vb_stride, Offset);
    pv->set(-1.f, -1.f, d_Z, d_W, C, 0.f, uv); // низ слева
    pv++;
    pv->set(-1.f, 1.f, d_Z, d_W, C, 0.f, 0.f); // верх слева
    pv++;
    pv->set(1.f, -1.f, d_Z, d_W, C, uv, uv); // низ справа
    pv++;
    pv->set(1.f, 1.f, d_Z, d_W, C, uv, 0.f); // верх справа
    pv++;
    RImplementation.Vertex.Unlock(4, g_combine->vb_stride);

    cmd_list.set_Element(sh);
    lambda();
    cmd_list.set_Geometry(g_combine);
    cmd_list.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

// Векторы движения в формате SSS: xy = (dx, -dy) * 0.5 в UV, z - маска худа, w - флора (op22_ssfx_mv.ps из rt_Velocity)
void CRenderTarget::phase_op22_ssfx_mv(CBackend& cmd_list)
{
    PIX_EVENT(op22_ssfx_mv);
    RenderScreenTriangle(cmd_list, rt_ssfx_motion_vectors, s_op22_ssfx_mv->E[0]);
}

void CRenderTarget::phase_ssfx_ao(CBackend& cmd_list)
{
    PIX_EVENT(phase_ssfx_ao);

    const float w = float(rt_ssfx_temp->dwWidth);
    const float h = float(rt_ssfx_temp->dwHeight);
    const float k = std::clamp(ps_ssfx_ao.x, 1.0f, 8.0f);
    const float sx = w / k, sy = h / k;

    // AO в уменьшенном разрешении
    op22_ssfx_quad(cmd_list, rt_ssfx_temp, s_ssfx_ao->E[0], sx, sy, 1.0f / k, [&]() {
        cmd_list.set_c("ao_setup", ps_ssfx_ao);
        cmd_list.set_c("m_current", Matrix_current);
        cmd_list.set_c("m_previous", Matrix_previous);
    });

    // кадр - для учёта прошлого
    HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_ao->pSurface, rt_ssfx_temp->pSurface);

    // 4 прохода размытия в полном разрешении (temp -> temp3 -> temp -> temp3 -> temp)
    const float blur[4][2] = {{k, 0.25f}, {1.f, 0.5f}, {1.f, 0.75f}, {1.f, 1.0f}};
    for (int i = 0; i < 4; i++)
    {
        const ref_rt& dst = (i % 2 == 0) ? rt_ssfx_temp3 : rt_ssfx_temp;
        op22_ssfx_quad(cmd_list, dst, s_ssfx_ao->E[1 + (i % 2)], w, h, 1.0f, [&]() {
            cmd_list.set_c("blur_setup", blur[i][0], blur[i][1], sx, sy);
            cmd_list.set_c("ao_setup", ps_ssfx_ao);
        });
    }

    cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
}

// OP22 (SSS 23): вода. Перенос phase_combine (блок [SSFX] Water), phase_ssfx_water_blur, phase_ssfx_water_waves
// (xray-monolith rendertarget_phase_blur.cpp; SSS UPDATE 23.2, Ascii1457) под CBackend OGSR.
// rt_ssfx_temp - отражения (1/ssfx_water.x), затем итог размытия в левой верхней четверти (шейдер воды читает tc / 2);
// rt_ssfx_water - кадр отражений для учёта прошлого; rt_ssfx_water_waves - высоты волн для параллакса.
void CRenderTarget::phase_ssfx_water(CBackend& cmd_list)
{
    auto& dsgraph = RImplementation.get_imm_context();
    if (dsgraph.mapWater.empty())
        return;

    PIX_EVENT(phase_ssfx_water);

    const float w = float(rt_ssfx_temp->dwWidth);
    const float h = float(rt_ssfx_temp->dwHeight);
    const float k = std::clamp(ps_ssfx_water.x, 1.0f, 8.0f);
    auto& Env = g_pGamePersistent->Environment();

    // матрицы текущего/прошлого кадра для учёта прошлого кадра отражений (как Matrix_current/previous monolith)
    static Fmatrix m_saved_viewproj;
    Fmatrix m_previous, m_current;
    m_previous.mul(m_saved_viewproj, Device.mInvView);
    m_current.set(Device.mProject);
    m_saved_viewproj.set(Device.mFullTransform);

    Fcolor clear_color;
    clear_color.set(0.f, 0.f, 0.f, 1.f);
    cmd_list.ClearRT(rt_ssfx_temp->pRT, clear_color);
    cmd_list.ClearRT(rt_ssfx_temp2->pRT, clear_color);

    // 1. отражения воды в 1/k разрешения (без буфера глубины, как в monolith)
    {
        PIX_EVENT(phase_ssfx_water_ssr);
        u_setrt(cmd_list, rt_ssfx_temp->dwWidth, rt_ssfx_temp->dwHeight, rt_ssfx_temp->pRT, nullptr, nullptr, nullptr);
        cmd_list.SetViewport({0.f, 0.f, w / k, h / k, 0.f, 1.f});
        cmd_list.set_CullMode(CULL_CCW);
        cmd_list.set_Stencil(FALSE);
        cmd_list.set_ColorWriteEnable();
        cmd_list.set_xform_world(Fidentity);
        dsgraph.r_dsgraph_render_water(s_ssfx_water_ssr->E[0]._get(), [&](CBackend& cl) {
            cl.set_c("m_current", m_current);
            cl.set_c("m_previous", m_previous);
        });
        cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
    }

    // кадр отражений - для учёта прошлого в следующем кадре
    HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_water->pSurface, rt_ssfx_temp->pSurface);

    // 2. размытие в половине разрешения: temp -> temp2 -> temp (или без размытия - копия)
    {
        PIX_EVENT(phase_ssfx_water_blur);
        const float hw = w * 0.5f, hh = h * 0.5f;
        if (ps_ssfx_water.y > 0)
        {
            op22_ssfx_quad(cmd_list, rt_ssfx_temp2, s_ssfx_water_blur->E[0], hw, hh, 0.5f, [&]() { cmd_list.set_c("blur_setup", 1.f, 0.f, 0.f, 2.0f / k); });
            op22_ssfx_quad(cmd_list, rt_ssfx_temp, s_ssfx_water_blur->E[1], hw, hh, 0.5f, [&]() { cmd_list.set_c("blur_setup", 0.f, 1.f, 0.f, 1.0f); });
        }
        else
        {
            HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_temp2->pSurface, rt_ssfx_temp->pSurface);
            op22_ssfx_quad(cmd_list, rt_ssfx_temp, s_ssfx_water_blur->E[2], hw, hh, 0.5f, [&]() { cmd_list.set_c("blur_setup", 0.f, 0.f, 0.f, 2.0f / k); });
        }
    }

    // 3. высоты волн 512x512 (параллакс воды)
    {
        PIX_EVENT(phase_ssfx_water_waves);
        op22_ssfx_quad(cmd_list, rt_ssfx_water_waves, s_ssfx_water_blur->E[3], 512.f, 512.f, 1.0f,
                       [&]() { cmd_list.set_c("wind_setup", Env.op22_wind_anim_w, Env.CurrentEnv->wind_velocity, 0.f, 0.f); });
    }

    // 4. сама вода поверх сцены (с буфером глубины)
    {
        PIX_EVENT(render_water);
        u_setrt(cmd_list, rt_Generic_0, nullptr, nullptr, nullptr, rt_Base_Depth->pZRT[cmd_list.context_id]);
        cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
        cmd_list.set_CullMode(CULL_CCW);
        cmd_list.set_Stencil(FALSE);
        cmd_list.set_ColorWriteEnable();
        cmd_list.set_xform_world(Fidentity);
        dsgraph.r_dsgraph_render_water(s_ssfx_water->E[0]._get(), [&](CBackend& cl) {
            cl.set_c("wind_setup", Env.CurrentEnv->wind_direction, Env.CurrentEnv->wind_velocity, 0.f, 0.f);
        });
    }

    dsgraph.mapWater.clear();
}

// OP22 (SSS 23): размытие лучей прожекторов (phase_ssfx_volumetric_blur SSS UPDATE 23.2, Ascii1457): 4 прохода
// vol -> tmp -> vol -> tmp -> vol в уменьшенном разрешении, затем перенос с растяжением в rt_Generic_3
// (его сводит combine_volumetric OGSR вместе с солнечными лучами rt_Generic_2).
void CRenderTarget::phase_ssfx_volumetric_blur(CBackend& cmd_list)
{
    PIX_EVENT(phase_ssfx_volumetric_blur);

    const float vw = float(rt_ssfx_volumetric->dwWidth);
    const float vh = float(rt_ssfx_volumetric->dwHeight);
    const int pixelsize[4] = {0, 1, 1, 2}; // как в SSS 23: полпикселя + размер
    const float pixelscale[4] = {2.0f, 0.5f, 2.0f, 0.5f};

    for (int b = 0; b < 4; b++)
    {
        const ref_rt& dst = (b % 2 == 0) ? rt_ssfx_volumetric_tmp : rt_ssfx_volumetric;
        op22_ssfx_quad(cmd_list, dst, s_ssfx_volumetric_blur->E[b % 2], vw, vh, 1.0f,
                       [&]() { cmd_list.set_c("blur_setup", vw, vh, float(pixelsize[b]), pixelscale[b]); });
    }

    // перенос в полный кадр (rt_Generic_3 содержит только лучи прожекторов - их теперь пишут сюда)
    const float w = float(rt_Generic_3->dwWidth);
    const float h = float(rt_Generic_3->dwHeight);
    op22_ssfx_quad(cmd_list, rt_Generic_3, s_ssfx_volumetric_blur->E[2], w, h, 1.0f, []() {});
    cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
}

// OP22: раз за кадр - матрицы текущего/прошлого кадра (Matrix_current/previous monolith) и векторы движения SSS.
// Вызывают экранные тени (до солнца) и блок AO/IL (в phase_combine); сброс флага - phase_scene_prepare.
void CRenderTarget::op22_ssfx_frame_prep(CBackend& cmd_list)
{
    if (m_op22_ssfx_prep_done)
        return;
    m_op22_ssfx_prep_done = true;

    static Fmatrix m_saved_viewproj;
    Matrix_previous.mul(m_saved_viewproj, Device.mInvView);
    Matrix_current.set(Device.mProject);
    m_saved_viewproj.set(Device.mFullTransform);

    phase_op22_ssfx_mv(cmd_list);
}

// OP22 (SSS 23): экранные тени солнца (phase_ssfx_sss SSS UPDATE 23.2, Ascii1457): расчёт с учётом прошлого кадра в
// rt_ssfx, два размытия маски (temp, temp2), итог - rt_ssfx_sss (его .b читают каскады солнца слоя).
void CRenderTarget::phase_ssfx_sss(CBackend& cmd_list)
{
    PIX_EVENT(phase_ssfx_sss);

    op22_ssfx_frame_prep(cmd_list);

    const float w = float(rt_ssfx->dwWidth);
    const float h = float(rt_ssfx->dwHeight);

    if (ps_ssfx_sss_quality.z <= 0.f)
    {
        cmd_list.ClearRT(rt_ssfx_sss, Fcolor{1.f, 1.f, 1.f, 1.f}); // выключено консолью - без теней
        return;
    }

    op22_ssfx_quad(cmd_list, rt_ssfx, s_ssfx_sss->E[0], w, h, 1.0f, [&]() {
        cmd_list.set_c("m_current", Matrix_current);
        cmd_list.set_c("m_previous", Matrix_previous);
    });
    op22_ssfx_quad(cmd_list, rt_ssfx_temp, s_ssfx_sss->E[1], w, h, 1.0f, [&]() { cmd_list.set_c("blur_setup", 0.f, 1.f, 0.f, 0.f); });
    op22_ssfx_quad(cmd_list, rt_ssfx_temp2, s_ssfx_sss->E[2], w, h, 1.0f, [&]() { cmd_list.set_c("blur_setup", 1.f, 0.f, 2.f, 0.f); });

    HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_sss->pSurface, rt_ssfx_temp2->pSurface);
    cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
}

// OP22 (SSS 23): экранные тени ламп (phase_ssfx_sss_ext SSS UPDATE 23.2, Ascii1457). До 8 ближних прожекторов/ламп с
// тенью получают слот (у всенаправленной - все 6 частей); 2 прохода по 4 лампы (rgba) с учётом прошлого кадра, сведение в
// rt_ssfx_sss_tmp (левая половина - лампы 0-3, правая - 4-7). Освещение лампы (accum_base.ps слоя) читает свой канал.
// Слоты: g_op22_sss_slots (light.cpp, удалённая лампа сама освобождает слот).
extern light* g_op22_sss_slots[8];

static void op22_sss_set_id(light* L, int id) { L->op22_set_sss_id(id); }

void CRenderTarget::phase_ssfx_sss_ext(CBackend& cmd_list, const light_Package& LP)
{
    PIX_EVENT(phase_ssfx_sss_ext);

    op22_ssfx_frame_prep(cmd_list);

    struct cand_t
    {
        light* L;
        int prio;
        float dist;
    };
    static xr_vector<cand_t> cands;
    cands.clear();

    const Fvector& cam = Device.vCameraPosition;
    for (light* L : LP.v_shadowed)
    {
        light* O = (L->flags.type == IRender_Light::OMNIPART) ? L->op22_omni_owner : L;
        if (!O || !O->flags.bActive || O->range <= 1.5f)
            continue;
        if (std::any_of(cands.begin(), cands.end(), [O](const cand_t& c) { return c.L == O; }))
            continue;
        const float d = cam.distance_to(O->position);
        if (d > O->range * 2.0f)
            continue;
        cands.push_back({O, d <= O->range ? 0 : 1, d}); // внутри радиуса лампы - раньше
    }
    std::sort(cands.begin(), cands.end(), [](const cand_t& a, const cand_t& b) { return a.prio != b.prio ? a.prio < b.prio : a.dist < b.dist; });

    // освободить слоты ламп, которых больше нет среди кандидатов
    for (auto& s : g_op22_sss_slots)
    {
        if (s && std::none_of(cands.begin(), cands.end(), [s](const cand_t& c) { return c.L == s; }))
        {
            op22_sss_set_id(s, -1);
            s = nullptr;
        }
    }
    // новые лампы - в свободные слоты (по порядку важности)
    for (const cand_t& c : cands)
    {
        if (std::find(std::begin(g_op22_sss_slots), std::end(g_op22_sss_slots), c.L) != std::end(g_op22_sss_slots))
            continue;
        auto free = std::find(std::begin(g_op22_sss_slots), std::end(g_op22_sss_slots), nullptr);
        if (free == std::end(g_op22_sss_slots))
            break;
        *free = c.L;
        op22_sss_set_id(c.L, int(free - std::begin(g_op22_sss_slots)));
    }

    // положение ламп в пространстве вида + затухание (w: 1 - не видно ~ 0 - полностью, как в SSS 23)
    Fvector4 data[8];
    for (int s = 0; s < 8; s++)
    {
        light* L = g_op22_sss_slots[s];
        if (!L)
        {
            data[s].set(0.f, 0.f, 0.f, 1.f);
            continue;
        }
        Fvector Lp;
        Device.mView.transform_tiny(Lp, L->position);
        const float d = cam.distance_to(L->position);
        const float atte = 1.0f - clampr((d - L->range * 1.9f) / -(L->range / 2.0f), 0.f, 1.f);
        data[s].set(Lp.x, Lp.y, Lp.z, atte);
    }

    const float w = float(rt_ssfx_sss_tmp->dwWidth);
    const float h = float(rt_ssfx_sss_tmp->dwHeight);

    // OP22 (28.11): проход по 4 лампам только если в его слотах есть лампа (днём/на открытом слоты обычно пусты -
    // ~0.4 мс впустую). Освещение лампы без слота (op22_sss_id -1) канал не читает, так что пропуск картинку не меняет.
    // История прохода (prev в ssfx_sss_ext.ps, вес до 0.97) после пропуска устарела - перед первым кадром с лампой
    // очищается в 1 (= без тени, как пустой слот), чтобы не всплыла тень прежней лампы.
    static bool pass_live[2]{};
    bool any_live = false;
    for (int pass = 0; pass < 2; pass++)
    {
        const bool live = std::any_of(g_op22_sss_slots + 4 * pass, g_op22_sss_slots + 4 * pass + 4, [](light* L) { return L != nullptr; });
        if (live && !pass_live[pass])
            cmd_list.ClearRT(pass == 0 ? rt_ssfx_sss_ext : rt_ssfx_sss_ext2, Fcolor{1.f, 1.f, 1.f, 1.f});
        pass_live[pass] = live;
        any_live |= live;
    }
    if (!any_live)
        return;

    for (int pass = 0; pass < 2; pass++)
    {
        if (!pass_live[pass])
            continue; // правую/левую половину сведения читают только лампы этих слотов - их нет
        op22_ssfx_quad(cmd_list, rt_ssfx_sss_tmp, s_ssfx_sss->E[3 + pass], w, h, 1.0f, [&]() {
            cmd_list.set_c("m_current", Matrix_current);
            cmd_list.set_c("m_previous", Matrix_previous);
            cmd_list.set_c("id_offset", pass);
            for (u32 i = 0; i < 8; i++)
                cmd_list.set_ca("lights_data", i, data[i]);
        });
        HW.get_context(cmd_list.context_id)->CopyResource((pass == 0 ? rt_ssfx_sss_ext : rt_ssfx_sss_ext2)->pSurface, rt_ssfx_sss_tmp->pSurface);
    }
    op22_ssfx_quad(cmd_list, rt_ssfx_sss_tmp, s_ssfx_sss->E[5], w, h, 1.0f, []() {});
    cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
}

void CRenderTarget::phase_ssfx_il(CBackend& cmd_list)
{
    PIX_EVENT(phase_ssfx_il);

    const float w = float(rt_ssfx_temp2->dwWidth);
    const float h = float(rt_ssfx_temp2->dwHeight);
    const float k = std::clamp(ps_ssfx_il.x, 1.0f, 8.0f);
    const float sx = w / k, sy = h / k;

    // IL в уменьшенном разрешении
    op22_ssfx_quad(cmd_list, rt_ssfx_temp2, s_ssfx_ao->E[3], sx, sy, 1.0f / k, [&]() {
        cmd_list.set_c("ao_setup", ps_ssfx_il);
        cmd_list.set_c("m_current", Matrix_current);
        cmd_list.set_c("m_previous", Matrix_previous);
    });

    HW.get_context(cmd_list.context_id)->CopyResource(rt_ssfx_il->pSurface, rt_ssfx_temp2->pSurface);

    // 4 прохода размытия там же, в уменьшенном разрешении (temp2 -> temp3 -> temp2 -> temp3 -> temp2), как в monolith
    const float blur[4] = {0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 4; i++)
    {
        const ref_rt& dst = (i % 2 == 0) ? rt_ssfx_temp3 : rt_ssfx_temp2;
        op22_ssfx_quad(cmd_list, dst, s_ssfx_ao->E[4 + (i % 2)], sx, sy, 1.0f / k, [&]() {
            cmd_list.set_c("blur_setup", 1.f, blur[i], sx, sy);
            cmd_list.set_c("ao_setup", ps_ssfx_il);
        });
    }

    cmd_list.SetViewport({0.f, 0.f, w, h, 0.f, 1.f});
}

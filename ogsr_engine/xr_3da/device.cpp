#include "stdafx.h"

#include "x_ray.h"
#include "render.h"
#include "xr_input.h"
#include "igame_persistent.h"
#include "imgui.h"
#include "..\Layers\xrRenderDX10\imgui_impl_dx11.h"
#include "..\Layers\xrRenderDX10\imgui_impl_win32.h"
#include "igame_level.h"
#include "splash.h"
#include <mmsystem.h>
#include "XR_IOConsole.h"
#include "xr_ioc_cmd.h"
#include "../xrGame/embedded_editor/embedded_editor_main.h"
#include "op22_frame_profile.h"
#include "op22_parallel_jobs.h"
#include <atomic>
#include <limits>
#include <mutex>

extern int g_op22_frame_stats;

// OP22 (28.11 perf): счётчик в мкс и метка кадра хранятся вместе, чтобы запоздавшая задача не попала в следующий кадр.
int g_op22_hitch_log = 1;
int g_op22_hitch_ms = 20;
static std::atomic<u64> op22_hitch_times[static_cast<unsigned>(Op22HitchStage::count)]{};

// OP22 (28.11 perf): публикуем самое долгое имя вместе с результатами замера текстур.
static std::mutex op22_hitch_texture_mutex;
static string_path op22_hitch_texture_name{};
static Op22FrameProfile::Clock::duration op22_hitch_texture_duration{};

Op22HitchTimer::Op22HitchTimer(Op22HitchStage stage, bool enabled, const char* texture_name)
    : stage(stage), frame_tag(enabled ? static_cast<unsigned>(op22_hitch_times[static_cast<unsigned>(stage)].load(std::memory_order_relaxed) >> 32) : 0),
      texture_name(stage == Op22HitchStage::tex ? texture_name : nullptr)
{
    if (frame_tag)
        started = Clock::now();
}

Op22HitchTimer::~Op22HitchTimer()
{
    if (!frame_tag)
        return;

    const auto duration = Clock::now() - started;
    const auto elapsed = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    // OP22 (28.11 perf): имя и счётчик текстур закрываются вместе при завершении кадра.
    std::unique_lock lock(op22_hitch_texture_mutex, std::defer_lock);
    if (texture_name)
        lock.lock();
    auto& counter = op22_hitch_times[static_cast<unsigned>(stage)];
    u64 previous = counter.load(std::memory_order_relaxed);
    while ((previous >> 32) == frame_tag)
    {
        const auto total = std::min<u64>(static_cast<u32>(previous) + elapsed, (std::numeric_limits<u32>::max)());
        if (counter.compare_exchange_weak(previous, (u64(frame_tag) << 32) | total, std::memory_order_relaxed))
        {
            if (texture_name && (!*op22_hitch_texture_name || duration > op22_hitch_texture_duration))
            {
                strncpy_s(op22_hitch_texture_name, texture_name, _TRUNCATE);
                op22_hitch_texture_duration = duration;
            }
            break;
        }
    }
}

static void op22_hitch_begin()
{
    static u32 frame_tag{};
    if (++frame_tag == 0)
        ++frame_tag;
    for (auto& counter : op22_hitch_times)
        counter.store(u64(frame_tag) << 32, std::memory_order_relaxed);
}

// OP22 (28.11 perf): раннее ожидание проверяем отдельно от работы кадра.
static void op22_hitch_finish(const Op22FrameProfile& profile, bool in_game, float early_wait = 0.f)
{
    const auto now = Op22FrameProfile::Clock::now();
    float times[static_cast<unsigned>(Op22HitchStage::count)];
    // OP22 (28.11 perf): запоздавший таймер не добавит имя после сброса счётчика кадра.
    std::unique_lock texture_lock(op22_hitch_texture_mutex);
    for (unsigned i = 0; i < static_cast<unsigned>(Op22HitchStage::count); ++i)
        times[i] = static_cast<float>(static_cast<u32>(op22_hitch_times[i].exchange(0, std::memory_order_relaxed))) / 1000.f;

    const float present_wait = times[static_cast<unsigned>(Op22HitchStage::present_wait)];
    const float total = Op22FrameProfile::milliseconds(now - profile.started) - present_wait;
    const float threshold = static_cast<float>(g_op22_hitch_ms);
    static Op22FrameProfile::Tick last{};
    const bool log = in_game && (total > threshold || early_wait > threshold) && (last == Op22FrameProfile::Tick{} || now - last >= std::chrono::milliseconds(100));
    string_path texture_name;
    if (log)
        strcpy_s(texture_name, *op22_hitch_texture_name ? op22_hitch_texture_name : "-");
    *op22_hitch_texture_name = 0;
    op22_hitch_texture_duration = {};
    texture_lock.unlock();
    if (!log)
        return;

    last = now;
    Msg("[OP22 hitch] frame=%u total=%.2f update=%.2f draw=%.2f wait=%.2f early_wait=%.2f gc=%.2f alife=%.2f destroy=%.2f spawn=%.2f tex=%.2f tex_name=%s",
        Device.dwFrame, total, profile.sample.update, profile.sample.draw, profile.sample.wait + present_wait, early_wait,
        times[static_cast<unsigned>(Op22HitchStage::gc)], times[static_cast<unsigned>(Op22HitchStage::alife)],
        times[static_cast<unsigned>(Op22HitchStage::destroy)], times[static_cast<unsigned>(Op22HitchStage::spawn)],
        times[static_cast<unsigned>(Op22HitchStage::tex)], texture_name);
}

//#define LOG_SECOND_THREAD_STATS

ENGINE_API CRenderDevice Device;
ENGINE_API CLoadScreenRenderer load_screen_renderer;

ENGINE_API BOOL g_bRendering = FALSE;

extern ENGINE_API float psHUD_FOV;

u32 g_dwFPSlimit{};

BOOL g_bLoaded = FALSE;

bool is_editor_active{};

BOOL CRenderDevice::Begin()
{
    switch (m_pRender->GetDeviceState())
    {
    case IRenderDeviceRender::dsOK: break;

    case IRenderDeviceRender::dsLost:
        // If the device was lost, do not render until we get it back
        Sleep(33);
        return FALSE;
        break;

    case IRenderDeviceRender::dsNeedReset:
        // Check if the device is ready to be reset
        Reset();
        break;

    default: R_ASSERT(0);
    }

    
    ::Render->Begin();

    R_ASSERT(g_bRendering == FALSE, "Something Strange");

    g_bRendering = TRUE;

    return TRUE;
}

void CRenderDevice::Clear() { ::Render->Clear(); }

void CRenderDevice::End()
{
    if (dwPrecacheFrame)
    {
        ::Sound->set_master_volume(0.f);

        dwPrecacheFrame--;

        if (!load_screen_renderer.b_registered)
        {
            ::Render->ClearTarget();

            if (0 == dwPrecacheFrame)
            {
                ::Sound->set_master_volume(1.f);
            }
        }        

        if (0 == dwPrecacheFrame)
        {
            ::Sound->set_master_volume(1.f);

            //Memory.mem_compact();

            Msg("* MEMORY USAGE: %d K", Memory.mem_usage() / 1024);
            Msg("* End of synchronization A[%d] R[%d]", b_is_Active, b_is_Ready);
        }
    }

    g_bRendering = FALSE;

    extern BOOL g_appLoaded;

    if (g_appLoaded && is_editor_active)
    {
        ImGui::Render();

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    ::Render->End();

    vCameraPositionSaved = vCameraPosition;
    vCameraDirectionSaved = vCameraDirection;
    vCameraTopSaved = vCameraTop;
    vCameraRightSaved = vCameraRight;

    mFullTransformSaved = mFullTransform;
    mViewSaved = mView;
    mProjectSaved = mProject;
}

void CRenderDevice::PreCache(u32 amount, bool b_draw_loadscreen, bool b_wait_user_input)
{
    dwPrecacheFrame = dwPrecacheTotal = amount;

    if (amount && b_draw_loadscreen && load_screen_renderer.b_registered == false)
    {
        load_screen_renderer.start(b_wait_user_input);
    }
}

ENGINE_API xr_list<fastdelegate::FastDelegate<bool()>> g_loading_events;

void CRenderDevice::OnCameraUpdated(bool from_actor)
{
    static u32 frame{u32(-1)};
    if (frame == dwFrame)
        return;

    frame = dwFrame;

    // Precache
    if (dwPrecacheFrame)
    {
        float factor = float(dwPrecacheFrame) / float(dwPrecacheTotal);
        float angle = PI_MUL_2 * factor;

        vCameraDirection.set(_sin(angle), 0, _cos(angle));
        vCameraDirection.normalize();

        vCameraTop.set(0, 1, 0);
        vCameraRight.crossproduct(vCameraTop, vCameraDirection);

        mView.build_camera_dir(vCameraPosition, vCameraDirection, vCameraTop);
    }

    // Matrices
    mInvView.invert(mView);
    mFullTransform.mul(mProject, mView);
    mInvFullTransform.invert_44(mFullTransform);

    ::Render->OnCameraUpdated(from_actor);

    ::Render->SetCacheXform(mView, mProject);

    mView_hud_old = mView_hud;
    mView_hud_old2 = mView_hud2;
    mProject_hud_old = mProject_hud;
    mProject_hud_old2 = mProject_hud2;
    mFullTransform_hud_old = mFullTransform_hud;
    mFullTransform_hud_old2 = mFullTransform_hud2;

    mView_old = mViewSaved;
    mProject_old = mProjectSaved;
    mFullTransform_old = mFullTransformSaved;

    ::Render->SetCacheXformOld(mView_old, mProject_old);

    mView_hud.build_camera_dir(Fvector{}, vCameraDirection, vCameraTop);
    mProject_hud.build_projection(deg2rad(psHUD_FOV <= 1.f ? psHUD_FOV * Device.fFOV : psHUD_FOV), fASPECT, HUD_VIEWPORT_NEAR,
                                  g_pGamePersistent->Environment().CurrentEnv->far_plane);
    mFullTransform_hud.mul(mProject_hud, mView_hud);

    mView_hud2.set(mView);
    mProject_hud2.build_projection(deg2rad(psHUD_FOV <= 1.f ? psHUD_FOV * Device.fFOV : psHUD_FOV), fASPECT, HUD_VIEWPORT_NEAR,
                                   g_pGamePersistent->Environment().CurrentEnv->far_plane);
    mFullTransform_hud2.mul(mProject_hud2, mView_hud2);
}

struct _SoundProcessor : public pureFrame
{
    void OnFrame() override
    {
        ZoneScopedN("_SoundProcessor");

        Device.Statistic->Sound.Begin();
        ::Sound->update(Device.vCameraPosition, Device.vCameraDirection, Device.vCameraTop /*, Device.Paused()*/);
        Device.Statistic->Sound.End();
    }
} g_sound_update;

#pragma todo("SIMP: для этого кода нужны переделки в xrSound")
/*
struct _SoundRender : public pureFrame
{
    virtual void OnFrame()
    {
        ZoneScopedN("_SoundRender");

        // Msg							("------------- sound: %d [%3.2f,%3.2f,%3.2f]",u32(Device.dwFrame),VPUSH(Device.vCameraPosition));
        Device.Statistic->Sound.Begin();
        ::Sound->render();
        Device.Statistic->Sound.End();
    }
} g_sound_render;
*/

void CRenderDevice::on_idle()
{
    is_editor_active = CImGuiEditor::Get().IsEditorShouldOpen();

    if (!b_is_Ready)
    {
        Sleep(100);
        return;
    }

    if (!g_loading_events.empty())
    {
        if (g_loading_events.front()())
            g_loading_events.pop_front();

        pApp->LoadDraw();

        return;
    }

    {
        ZoneScoped;

        // OP22 (28.11 perf): замеряем ожидание до старта таймера работы кадра.
        const bool op22_hitch = g_op22_hitch_log && g_op22_ingame && g_pGameLevel && b_is_Active && !dwPrecacheFrame && !load_screen_renderer.b_registered;
        float op22_early_wait = 0.f;
        // OP22 (28.11 perf): ждём до ввода и FrameMove, вне загрузки и предзагрузки.
        if (b_is_Active && !dwPrecacheFrame && !load_screen_renderer.b_registered && (!pApp || !pApp->IsLoading()) &&
            m_pRender && ::Render)
        {
            const auto started = op22_hitch ? Op22FrameProfile::Clock::now() : Op22FrameProfile::Tick{};
            m_pRender->WaitForFrameLatency();
            if (op22_hitch)
                op22_early_wait = Op22FrameProfile::milliseconds(Op22FrameProfile::Clock::now() - started);
        }

#ifdef LOG_SECOND_THREAD_STATS
        const auto FrameStartTime = std::chrono::high_resolution_clock::now();
#endif

        if (psDeviceFlags.test(rsStatistic))
            g_bEnableStatGather = TRUE;
        else
            g_bEnableStatGather = FALSE;

        if (is_editor_active)
        {
            ImGui_ImplDX11_NewFrame(); // должно быть перед FrameMove
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
        }

        // OP22 (28.11 perf): измеряем игровой кадр и закрываем счётчики даже при переходе к загрузке.
        Op22FrameProfile op22_profile(g_op22_frame_stats > 1 && g_pGameLevel, op22_hitch);
        if (op22_hitch)
            op22_hitch_begin();
        const Fvector op22_previous_direction = vCameraDirection;
        const auto op22_update_start = op22_profile.now();

        // if (b_is_Active)
        {
            FrameMove();
        }
        op22_profile.sample.update = op22_profile.since(op22_update_start);
        op22_profile.sample.precache = int(dwPrecacheFrame);

        if (!g_loading_events.empty()) // that is to avoid loading screen flickering
        {
            if (op22_hitch)
                op22_hitch_finish(op22_profile, false);
            pApp->LoadDraw();

            return;
        }

        const auto op22_camera_start = op22_profile.now(op22_profile.detailed);
        OnCameraUpdated(false);
        op22_profile.sample.camera = op22_profile.since(op22_camera_start);
        if (op22_profile.detailed)
            op22_profile.sample.turn = rad2deg(acosf(std::clamp(op22_previous_direction.dotproduct(vCameraDirection), -1.f, 1.f)));

        const auto SecondThreadStartTime = std::chrono::high_resolution_clock::now();

        // allow secondary thread to do its job
        auto awaiter = TTAPI->submit([this, &op22_profile] {
            ZoneScoped;

            const auto SecondThreadTasksStartTime = std::chrono::high_resolution_clock::now();

            // OP22 (28.11 perf): дешёвый журнал обходится без таймеров отдельных фоновых задач.
            auto op22_task_start = op22_profile.now(op22_profile.detailed);
            g_sound_update.OnFrame();
            op22_profile.sample.sound = op22_profile.since(op22_task_start);
            op22_task_start = op22_profile.now(op22_profile.detailed);

            // add_to_awaiters(TTAPI->submit([]() { g_sound_render.OnFrame(); }));

            {
                ZoneScopedN("seqParallel");

                Op22ParallelJobs op22_jobs;
                static const auto op22_image_base = reinterpret_cast<std::uintptr_t>(GetModuleHandle(nullptr));
                static const auto op22_image_end = op22_image_base + reinterpret_cast<const IMAGE_NT_HEADERS*>(
                    op22_image_base + reinterpret_cast<const IMAGE_DOS_HEADER*>(op22_image_base)->e_lfanew)->OptionalHeader.SizeOfImage;
                auto size = seqParallel.size();
                while (size > 0)
                {
                    const auto op22_identity = op22_profile.detailed ? Op22DelegateIdentity(seqParallel.front().GetMemento()).code() : 0;
                    const auto op22_job_start = op22_profile.now(op22_profile.detailed);
                    seqParallel.front()();
                    if (op22_profile.detailed)
                        op22_jobs.record(op22_identity, op22_image_base, op22_image_end, op22_profile.since(op22_job_start));
                    seqParallel.pop_front();

                    size--;
                }
                const auto op22_job_worst = op22_jobs.worst();
                op22_profile.sample.parallel_job_rva = static_cast<unsigned long long>(op22_job_worst.rva);
                op22_profile.sample.parallel_job_ms = op22_job_worst.ms;
                op22_profile.sample.parallel_job_calls = op22_job_worst.calls;
                op22_profile.sample.parallel_jobs = op22_jobs.calls;
            }

            op22_profile.sample.parallel = op22_profile.since(op22_task_start);
            if (g_pGameLevel && ps_lua_gc_method == gc_timeout)
            {
                const auto GCTime = (SecondThreadFreeTimeLast.count() / 2.0) * 1000.0;
                psLUA_GCTIMEOUT = std::max(psLUA_GCTIMEOUT_MIN, static_cast<int>(GCTime));
                op22_profile.sample.gc_budget = psLUA_GCTIMEOUT;
                add_to_awaiters(TTAPI->submit([&op22_profile] {
                    const auto gc_started = op22_profile.now(op22_profile.detailed);
                    g_pGameLevel->script_gc();
                    op22_profile.sample.gc = op22_profile.since(gc_started);
                }));
            }

            {
                ZoneScopedN("seqFrameMT");

                op22_task_start = op22_profile.now(op22_profile.detailed);
                seqFrameMT.Process(rp_Frame); // only PHWorld and NetProcessor
                op22_profile.sample.physics = op22_profile.since(op22_task_start);
            }

            const auto SecondThreadTasksEndTime = std::chrono::high_resolution_clock::now();
            SecondThreadTasksElapsedTime = SecondThreadTasksEndTime - SecondThreadTasksStartTime;
        });

        Statistic->RenderTOTAL_Real.FrameStart();
        Statistic->RenderTOTAL_Real.Begin();

        if (b_is_Active)
        {
            ZoneScopedN("DoRender");

            const auto op22_begin_start = op22_profile.now(op22_profile.detailed);
            const bool op22_render_ready = Begin();
            op22_profile.sample.begin = op22_profile.since(op22_begin_start);
            if (op22_render_ready)
            {
                {
                    ZoneScopedN("Render process");
                    const auto op22_draw_start = op22_profile.now();
                    seqRender.Process(rp_Render);
                    op22_profile.sample.draw = op22_profile.since(op22_draw_start);
                }

                if (is_editor_active || CImGuiEditor::Get().IsEditorShouldOpenOnNextFrame())
                {
                    ::Render->CaptureMainRTTexture();
                }

                {
                    ZoneScopedN("DoRender End");
                    const auto op22_end_start = op22_profile.now(op22_profile.detailed);
                    End();
                    op22_profile.sample.present = op22_profile.since(op22_end_start);
                }
            }
        }

        if (is_editor_active)
        {
            ImGui::EndFrame();
        }

        Statistic->RenderTOTAL_Real.End();
        Statistic->RenderTOTAL_Real.FrameEnd();

        Statistic->RenderTOTAL.accum = Statistic->RenderTOTAL_Real.accum;

#ifdef LOG_SECOND_THREAD_STATS
        const auto FrameEndTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double, std::milli> FrameElapsedTime = FrameEndTime - FrameStartTime;
#endif

        {
            ZoneScopedN("WaitSecondThread");
            const auto op22_wait_start = op22_profile.now();

            const auto SecondThreadEndTime = std::chrono::high_resolution_clock::now();

#ifdef LOG_SECOND_THREAD_STATS
            bool show_stats{};
            if (awaiter.wait_for(FrameElapsedTime) == std::future_status::timeout)
                show_stats = true;
#else
            if (awaiter.valid())
#endif
            awaiter.wait();

            while (!second_tasks.empty())
            {
                if (second_tasks.back().valid())
                    second_tasks.back().wait();
                second_tasks.pop_back();
            }

            op22_profile.sample.wait = op22_profile.since(op22_wait_start);

            const std::chrono::duration<double, std::milli> SecondThreadElapsedTime = SecondThreadEndTime - SecondThreadStartTime;
            SecondThreadFreeTimeLast = SecondThreadElapsedTime - SecondThreadTasksElapsedTime;

#ifdef LOG_SECOND_THREAD_STATS
            if (show_stats && dwPrecacheFrame == 0)
            {
                Msg("##[%s] Second thread work time is too long! Avail: [%f]ms, used: [%f]ms, free: [%f]ms", __FUNCTION__, SecondThreadElapsedTime.count(),
                    SecondThreadTasksElapsedTime.count(), SecondThreadFreeTimeLast.count());
            }
#endif
        }

        extern void LogD3D11DebugMessages();
        LogD3D11DebugMessages();

        // OP22 (28.11 perf): учитываем прогрев или экран загрузки, включившиеся внутри кадра.
        if (op22_hitch)
        {
            const bool in_game = g_op22_ingame && g_pGameLevel && !op22_profile.sample.precache && !dwPrecacheFrame &&
                !load_screen_renderer.b_registered && g_loading_events.empty();
            op22_hitch_finish(op22_profile, in_game, op22_early_wait); // OP22 (28.11 perf): передаём раннее ожидание отдельно.
        }
        op22_profile.finish();
        FrameMark;
    }

    if (!b_is_Active)
        Sleep(1);
}

void CRenderDevice::message_loop()
{
    MSG msg{};

    while (msg.message != WM_QUIT)
    {
        // Разбираем всю очередь сообщений за кадр, а не по одному - иначе ввод (WM_MOUSEMOVE и т.п.) отстаёт на N кадров
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                break;
        }

        on_idle();
    }
}

void CRenderDevice::ShowMainWindow() const
{
    ShowWindow(m_hWnd, SW_SHOWNORMAL);
    SetForegroundWindow(m_hWnd);
}

void CRenderDevice::Run()
{
    //	DUMP_PHASE;
    g_bLoaded = FALSE;

    Log("Starting engine...");

    set_current_thread_name("X-RAY Primary thread");
    mainThreadId = std::this_thread::get_id();
    Msg("Main thread id: [%u]", _Thrd_id());

    // Startup timers and calculate timer delta
    dwTimeGlobal = 0;
    Timer_MM_Delta = 0;
    {
        u32 time_mm = timeGetTime();
        while (timeGetTime() == time_mm)
            ; // wait for next tick
        u32 time_system = timeGetTime();
        u32 time_local = TimerAsync();
        Timer_MM_Delta = time_system - time_local;
    }

    // Message cycle
    seqAppStart.Process(rp_AppStart);

    ::Render->ClearTarget();

    // Destroy LOGO
    HideSplash();

    ShowMainWindow();

    pInput->clip_cursor(true);

    Memory.mem_compact();

    g_bLoaded = TRUE;

    message_loop();

    g_bLoaded = FALSE;

    seqAppEnd.Process(rp_AppEnd);
}

u32 app_inactive_time = 0;
u32 app_inactive_time_start = 0;

void CRenderDevice::FrameMove()
{
    ZoneScoped;

    dwFrame++;

    u32 _old_continual = dwTimeContinual;
    dwTimeContinual = TimerMM.GetElapsed_ms() - app_inactive_time;
    dwTimeDeltaContinual = dwTimeContinual - _old_continual;

    fTimeDeltaReal = Timer.GetElapsed_sec();
    fTimeDeltaRealMS = static_cast<float>(Timer.GetElapsed_ms());
    if (!_valid(fTimeDeltaReal))
        fTimeDeltaReal = EPS_S + EPS_S;
    Timer.Start(); // previous frame

    {
        if (Paused())
            fTimeDelta = 0.0f;
        else
        {
            fTimeDelta = 0.1f * fTimeDelta + 0.9f * fTimeDeltaReal; // smooth random system activity - worst case ~7% error
            clamp(fTimeDelta, EPS_S + EPS_S, .1f); // limit to 10fps minimum
        }
        fTimeGlobal = TimerGlobal.GetElapsed_sec();

        u32 _old_global = dwTimeGlobal;
        dwTimeGlobal = TimerGlobal.GetElapsed_ms();
        dwTimeDelta = dwTimeGlobal - _old_global;
    }

    // Frame move
    Statistic->EngineTOTAL.Begin();

    Device.seqFrame.Process(rp_Frame);

    Statistic->EngineTOTAL.End();
}

ENGINE_API BOOL bShowPauseString = TRUE;

void CRenderDevice::Pause(BOOL bOn, BOOL bTimer, BOOL bSound, LPCSTR reason)
{
    static int snd_emitters_ = -1;

#ifdef DEBUG
//	Msg("pause [%s] timer=[%s] sound=[%s] reason=%s",bOn?"ON":"OFF", bTimer?"ON":"OFF", bSound?"ON":"OFF", reason);
#endif // DEBUG


    if (bOn)
    {
        if (!Paused())
            bShowPauseString =
#ifdef DEBUG
                !xr_strcmp(reason, "li_pause_key_no_clip") ? FALSE :
#endif // DEBUG
                                                             TRUE;

        if (bTimer)
        {
            g_pauseMngr->Pause(TRUE);
#ifdef DEBUG
            if (!xr_strcmp(reason, "li_pause_key_no_clip"))
                TimerGlobal.Pause(FALSE);
#endif // DEBUG
        }

        if (bSound && ::Sound)
        {
            snd_emitters_ = ::Sound->pause_emitters(true);
#ifdef DEBUG
//			Log("snd_emitters_[true]",snd_emitters_);
#endif // DEBUG
        }
    }
    else
    {
        if (bTimer && g_pauseMngr->Paused())
        {
            fTimeDelta = EPS_S + EPS_S;

            g_pauseMngr->Pause(FALSE);
        }

        if (bSound)
        {
            if (snd_emitters_ > 0) // avoid crash
            {
                snd_emitters_ = ::Sound->pause_emitters(false);
#ifdef DEBUG
//				Log("snd_emitters_[false]",snd_emitters_);
#endif // DEBUG
            }
            else
            {
#ifdef DEBUG
                Log("Sound->pause_emitters underflow");
#endif // DEBUG
            }
        }
    }

}

BOOL CRenderDevice::Paused() { return g_pauseMngr->Paused(); }

void CRenderDevice::OnWM_Activate(WPARAM wParam, LPARAM lParam)
{
    const u16 fActive = LOWORD(wParam);
    const BOOL fMinimized = (BOOL)HIWORD(wParam);
    const BOOL bWndActive = (fActive != WA_INACTIVE) && !fMinimized ? TRUE : FALSE;

    const BOOL isGameActive = (!g_loading_events.empty() || (psDeviceFlags.is(rsAlwaysActive)) || bWndActive) ? TRUE : FALSE;

    if (g_bLoaded)
        pInput->clip_cursor(fActive != WA_INACTIVE);

    if (isGameActive != b_is_Active)
    {
        b_is_Active = isGameActive;

        if (b_is_Active)
        {
            seqAppActivate.Process(rp_AppActivate);
            app_inactive_time += TimerMM.GetElapsed_ms() - app_inactive_time_start;
        }
        else
        {
            app_inactive_time_start = TimerMM.GetElapsed_ms();
            seqAppDeactivate.Process(rp_AppDeactivate);
        }
    }
}

void CRenderDevice::SetAltScopeActive(bool bState)
{
    m_AltScopeActive = bState;
}

CLoadScreenRenderer::CLoadScreenRenderer() : b_registered(false) {}

void CLoadScreenRenderer::start(bool b_user_input)
{
    Device.seqRender.Add(this);
    b_registered = true;
    b_need_user_input = b_user_input;
}

void CLoadScreenRenderer::stop()
{
    if (!b_registered)
        return;

    Device.seqRender.Remove(this);
    pApp->DestroyLoadingScreen();
    b_registered = false;
    b_need_user_input = false;
}

void CLoadScreenRenderer::OnRender() { pApp->load_draw_internal(); }

void CRenderDevice::time_factor(const float& time_factor)
{
    Timer.time_factor(time_factor);
    TimerGlobal.time_factor(time_factor);
    psSoundTimeFactor = time_factor; //--#SM+#--
}

bool OnMainThread() { return Device.OnMainThread(); } // for core

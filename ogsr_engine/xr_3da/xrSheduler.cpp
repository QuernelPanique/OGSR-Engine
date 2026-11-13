#include "stdafx.h"
#include "xrSheduler.h"
#include "xr_object.h"

#include <chrono>

float psShedulerCurrent = 10.f;
float psShedulerTarget = 10.f;
float psShedulerMax = 10.f;
constexpr float psShedulerReaction = 1.f; // 0.1f;

// OP22: op22_frame_stats 1 - раз в 10 с строка [op22_frames] в логе: рывки кадра и цена очереди
int g_op22_frame_stats{};
int g_op22_sleeping{}; // объектов вне очереди: предметы в ящиках (CObject::op22_shedule_sleep)


// Diagnostics only: keep long callbacks visible without logging every object.
static void op22_slow_update(const char* name, float elapsed_ms, bool realtime)
{
    if (elapsed_ms < 4.f)
        return;
    using clock = std::chrono::steady_clock;
    static clock::time_point window{};
    static u32 messages{};
    const auto now = clock::now();
    if (window == clock::time_point{} || now - window >= std::chrono::seconds(10))
    {
        window = now;
        messages = 0;
    }
    if (messages++ < 20)
        Msg("[op22_slow] rt=%d object=[%s] %.2f ms", int(realtime), name ? name : "?", elapsed_ms);
}

static void op22_frame_stats(float sched_ms, size_t queue)
{
    using clock = std::chrono::steady_clock;
    const float window_ms = g_op22_frame_stats > 1 ? 1000.f : 10000.f;
    constexpr float late_ms = 3.f, skip_ms = 1000.f; // кадр дольше секунды - загрузка, не рывок
    static xr_vector<float> frames;
    static clock::time_point last{};
    static float window{}, sched_sum{}, sched_max{};

    const auto now = clock::now();
    const float frame_ms = last == clock::time_point{} ? 0.f : std::chrono::duration<float, std::milli>(now - last).count();
    last = now;

    if (!g_op22_frame_stats || frame_ms <= 0.f || frame_ms > skip_ms)
    {
        frames.clear();
        window = sched_sum = sched_max = 0.f;
        return;
    }

    frames.push_back(frame_ms);
    window += frame_ms;
    sched_sum += sched_ms;
    sched_max = std::max(sched_max, sched_ms);
    if (window < window_ms)
        return;

    std::sort(frames.begin(), frames.end());
    const size_t worst = std::max<size_t>(1, frames.size() / 100);
    float low{};
    for (size_t i = frames.size() - worst; i < frames.size(); ++i)
        low += frames[i];
    low /= float(worst);

    const float budget = g_dwFPSlimit ? 1000.f / float(g_dwFPSlimit) : 1000.f / 144.f;
    const size_t late = size_t(frames.end() - std::upper_bound(frames.begin(), frames.end(), budget + late_ms));
    Msg("[op22_frames] %u fr, avg %.2f ms, 1%% low %.2f ms (%.0f fps), max %.2f ms, late %u; sched avg %.3f max %.2f ms; queue %u, asleep %d",
        u32(frames.size()), window / float(frames.size()), low, 1000.f / low, frames.back(), u32(late), sched_sum / float(frames.size()), sched_max,
        u32(queue), g_op22_sleeping);

    frames.clear();
    window = sched_sum = sched_max = 0.f;
}

void CSheduler::Initialize() { m_processing_now = false; }

void CSheduler::Destroy()
{
    internal_Registration();

    ItemsRT.clear();
    Items.clear();
    Registration.clear();
}

void CSheduler::internal_Registration()
{
    for (u32 it = 0; it < Registration.size(); it++)
    {
        ItemReg& R = Registration[it];
        if (R.OP)
        {
            // register
            // search for paired "unregister"

            BOOL bFoundAndErased = FALSE;
            for (u32 pair = it + 1; pair < Registration.size(); pair++)
            {
                ItemReg& R_pair = Registration[pair];
                if ((!R_pair.OP) && (R_pair.Object == R.Object))
                {
                    bFoundAndErased = TRUE;
                    Registration.erase(Registration.begin() + pair);
                    break;
                }
            }

            // register if non-paired
            if (!bFoundAndErased)
            {
                //Msg("SCHEDULER: internal register [%s][%x][%s]", *R.Object->shedule_Name(), R.Object, R.RT ? "true" : "false");
                internal_Register(R.Object, R.RT);
            }
            //else
            //    Msg("SCHEDULER: internal register skipped, because unregister found [%s][%x][%s]", "unknown", R.Object, R.RT ? "true" : "false");
        }
        else
        {
            // unregister
            internal_Unregister(R.Object, R.RT);
        }
    }

    Registration.clear();
}

void CSheduler::internal_Register(ISheduled* object, BOOL RT)
{
    VERIFY(!O->shedule.b_locked);

    if (RT)
    {
        // Fill item structure
        auto& TNext = ItemsRT.emplace_back();
        TNext.dwTimeForExecute = Device.dwTimeGlobal;
        TNext.dwTimeOfLastExecute = Device.dwTimeGlobal;
        TNext.Object = object;
        TNext.scheduled_name = object->shedule_Name();
        object->shedule.b_RT = TRUE;
    }
    else
    {
        // Fill item structure && Insert into priority Queue
        auto& TNext = Items.emplace_back();
        TNext.dwTimeForExecute = Device.dwTimeGlobal;
        TNext.dwTimeOfLastExecute = Device.dwTimeGlobal;
        TNext.Object = object;
        TNext.scheduled_name = object->shedule_Name();
        object->shedule.b_RT = FALSE;
    }
}

bool CSheduler::internal_Unregister(const ISheduled* object, BOOL RT)
{
    if (RT)
    {
        for (u32 i = 0; i < ItemsRT.size(); i++)
        {
            if (ItemsRT[i].Object == object)
            {
                ItemsRT.erase(ItemsRT.begin() + i);
                return true;
            }
        }
    }
    else
    {
        for (auto& Item : Items)
        {
            if (Item.Object == object)
            {
                Item.Object = nullptr;
                return true;
            }
        }
    }

    return false;
}

#ifdef DEBUG
bool CSheduler::Registered(ISheduled* object) const
{
    u32 count = 0;
    typedef xr_vector<Item> ITEMS;

    {
        ITEMS::const_iterator I = ItemsRT.begin();
        ITEMS::const_iterator E = ItemsRT.end();
        for (; I != E; ++I)
            if ((*I).Object == object)
            {
                //				Msg				("0x%8x found in RT",object);
                count = 1;
                break;
            }
    }
    {
        ITEMS::const_iterator I = Items.begin();
        ITEMS::const_iterator E = Items.end();
        for (; I != E; ++I)
            if ((*I).Object == object)
            {
                //				Msg				("0x%8x found in non-RT",object);
                VERIFY(!count);
                count = 1;
                break;
            }
    }

    {
        ITEMS::const_iterator I = ItemsProcessed.begin();
        ITEMS::const_iterator E = ItemsProcessed.end();
        for (; I != E; ++I)
            if ((*I).Object == object)
            {
                //				Msg				("0x%8x found in process items",object);
                VERIFY(!count);
                count = 1;
                break;
            }
    }

    typedef xr_vector<ItemReg> ITEMS_REG;
    ITEMS_REG::const_iterator I = Registration.begin();
    ITEMS_REG::const_iterator E = Registration.end();
    for (; I != E; ++I)
    {
        if ((*I).Object == object)
        {
            if ((*I).OP)
            {
                //				Msg				("0x%8x found in registration on register",object);
                VERIFY(!count);
                ++count;
            }
            else
            {
                //				Msg				("0x%8x found in registration on UNregister",object);
                VERIFY(count == 1);
                --count;
            }
        }
    }

    VERIFY(!count || (count == 1));
    return (count == 1);
}
#endif // DEBUG

void CSheduler::Register(ISheduled* A, BOOL RT)
{
    VERIFY(!Registered(A));

    auto& R = Registration.emplace_back();
    R.OP = TRUE;
    R.RT = RT;
    R.Object = A;
    R.Object->shedule.b_RT = RT;

    //Msg("SCHEDULER: register [%s][%x]", *A->shedule_Name(), A);
}

void CSheduler::Unregister(ISheduled* A, bool force)
{
    VERIFY(Registered(A));

    if (m_processing_now || force)
    {
        if (internal_Unregister(A, A->shedule.b_RT))
            return;
    }

    auto& R = Registration.emplace_back();
    R.OP = FALSE;
    R.RT = A->shedule.b_RT;
    R.Object = A;
}

void CSheduler::ProcessStep()
{
    ZoneScoped;

    // Normal priority
    u32 dwTime = Device.dwTimeGlobal;

    const bool prefetch = Device.dwPrecacheFrame > 0;
    bool stopped{};
    size_t processed{};
    shared_str first_processed;
    CTimer t_total;
    t_total.Start();

    // OP22: раньше каждый выполненный объект стирался из середины вектора, цена кадра росла быстрее числа объектов (рывки у
    // ящика на 1338 предметов). Теперь внутри прохода вектор не меняет размер, выполненные уходят в конец одним проходом
    // ниже. Порядок вызовов прежний. Объект, снятый во время прохода, в том числе сам собой, получает Object = nullptr
    // (internal_Unregister), поэтому после shedule_Update проверяется curr.Object.
    const size_t count = Items.size();
    for (size_t it{}; it < count; ++it)
    {
        Item& curr = Items[it];

        if (curr.Object == nullptr || curr.dwTimeForExecute >= dwTime)
        {
            continue;
        }

        bool shed_need{};
        __try
        {
            shed_need = curr.Object->shedule_Needed();
        }
        __except (ExceptStackTrace("[CSheduler::ProcessStep] stack trace:\n"))
        {
            Msg("Scheduler tried to update object %s", *curr.scheduled_name);
        }

        if (!shed_need)
        {
            curr.Object = nullptr;
            continue;
        }

        __try
        {
            // Calc next update interval
            const u32 dwMin = std::max(30u, curr.Object->shedule.t_min);
            const u32 dwMax = (1000u + curr.Object->shedule.t_max) / 2;

            const float scale = curr.Object->shedule_Scale();

            u32 dwUpdate = dwMin + iFloor(float(dwMax - dwMin) * scale);
            clamp(dwUpdate, std::max(dwMin, 20u), dwMax);

            const u32 elapsed = dwTime - curr.dwTimeOfLastExecute;

            // if (!Core.DebugFlags.test(xrCore::dbg_DisableObjectsScheduler))
            const auto op22_item_begin = g_op22_frame_stats ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            curr.Object->shedule_Update(std::clamp(elapsed, 1u, std::max(curr.Object->shedule.t_max, 1000u)));
            if (g_op22_frame_stats)
                op22_slow_update(curr.scheduled_name.c_str(),
                    std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - op22_item_begin).count(), false);

            if (!curr.Object)
            {
                continue;
            }

            // Fill item structure
            curr.dwTimeForExecute = dwTime + dwUpdate;
            curr.dwTimeOfLastExecute = dwTime;
            curr.scheduled_name = curr.Object->shedule_Name();
            curr.op22_done = true;

            if (!processed++)
            {
                first_processed = curr.scheduled_name;
            }
        }
        __except (ExceptStackTrace("[CSheduler::ProcessStep2] stack trace:\n"))
        {
            Msg("Scheduler tried to update object %s", *curr.scheduled_name);
            curr.Object = nullptr;
            continue;
        }

        if (!prefetch && t_total.GetElapsed_ms() > static_cast<u32>(std::floor(psShedulerCurrent)))
        {
            // we have maxed out the load - increase heap
            psShedulerTarget += (psShedulerReaction * 3);

            // if (Core.DebugFlags.test(xrCore::dbg_TraceScheduler))
            {
                // Msg("Break ProcessStep. Processed: [%u], left in queue: [%u]", ItemsProcessed.size(), Items.size());
                if (processed == 1) // кто то жрет все время на кадре
                    Msg("! Single item [%s] took whole update frame!!!", first_processed.c_str());
            }

            stopped = true;
            break;
        }
    }

    //if (/*Core.DebugFlags.test(xrCore::dbg_TraceScheduler) &&*/ !prefetch && t_total.GetElapsed_ms() > 20)
    //    Msg("Long ProcessStep !!! duration [%u]ms. updated: [%u] objects!", t_total.GetElapsed_ms(), cnt);

    //if (prefetch)
    //    Msg("Prefetch frame, updated: [%u] objects!", cnt);

    // Push "processed" back
    size_t kept{};
    for (size_t it{}; it < count; ++it)
    {
        Item& item = Items[it];
        if (item.Object == nullptr)
        {
            continue;
        }
        if (item.op22_done)
        {
            item.op22_done = false;
            ItemsDone.emplace_back(std::move(item));
            continue;
        }
        if (kept != it)
        {
            Items[kept] = std::move(item);
        }
        ++kept;
    }
    Items.resize(kept);
    Items.insert(Items.end(), std::make_move_iterator(ItemsDone.begin()), std::make_move_iterator(ItemsDone.end()));
    ItemsDone.clear();

    if (!stopped)
    {
        // always try to decrease target
        psShedulerTarget -= psShedulerReaction;
    }
}

void CSheduler::Update()
{
    ZoneScoped;

    // Initialize
    Device.Statistic->Sheduler.Begin();

    const auto op22_begin = std::chrono::steady_clock::now();

    internal_Registration();

    m_processing_now = true;

    u32 dwTime = Device.dwTimeGlobal;

    {
        ZoneScopedN("ItemsRT");

        // Realtime priority
        for (auto& curr : ItemsRT)
        {
            R_ASSERT(curr.Object);

            if (!curr.Object->shedule_Needed())
            {
                curr.dwTimeOfLastExecute = dwTime;
                continue;
            }

            const u32 elapsed = dwTime - curr.dwTimeOfLastExecute;
            const auto op22_item_begin = g_op22_frame_stats ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            curr.Object->shedule_Update(elapsed);
            if (g_op22_frame_stats)
                op22_slow_update(curr.scheduled_name.c_str(),
                    std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - op22_item_begin).count(), true);
            curr.dwTimeOfLastExecute = dwTime;
        }
    }

    // Normal (sheduled)
    ProcessStep();

    clamp(psShedulerTarget, 3.f, psShedulerMax);

    psShedulerCurrent = 0.9f * psShedulerCurrent + 0.1f * psShedulerTarget;
    Device.Statistic->fShedulerLoad = psShedulerCurrent;

    m_processing_now = false;

    internal_Registration();

    op22_frame_stats(std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - op22_begin).count(), Items.size());

    Device.Statistic->Sheduler.End();
}

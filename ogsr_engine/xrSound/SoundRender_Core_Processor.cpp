#include "stdafx.h"

#include <efx.h>
#include <algorithm> // OP22: std::clamp

#include "../xrCDB/cl_intersect.h"
#include "SoundRender_Core.h"
#include "SoundRender_Emitter.h"
#include "SoundRender_Target.h"
#include "SoundRender_Source.h"

CSoundRender_Emitter* CSoundRender_Core::i_play(ref_sound* S, BOOL _loop, float delay)
{
    VERIFY(S->_p->feedback == 0);
    CSoundRender_Emitter* E = xr_new<CSoundRender_Emitter>();
    S->_p->feedback = E;
    E->start(S, _loop, delay);
    s_emitters.push_back(E);
    return E;
}

void CSoundRender_Core::update(const Fvector& P, const Fvector& D, const Fvector& N) // called on seqFrameMT
{
    u32 it;

    if (0 == bReady)
        return;

    std::scoped_lock m{m_bLocked};

    bLocked = TRUE;
    Timer.time_factor(psSoundTimeFactor); //--#SM+#--
    float new_tm = Timer.GetElapsed_sec();
    fTimer_Delta = new_tm - fTimer_Value;
    float dt_sec = fTimer_Delta;
    fTimer_Value = new_tm;

    s_emitters_u++;

    // Firstly update emitters, which are now being rendered
    // Msg	("! update: r-emitters");
    for (it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        CSoundRender_Emitter* E = T->get_emitter();
        if (E)
        {
            E->update(dt_sec);
            E->marker = s_emitters_u;
            E = T->get_emitter(); // update can stop itself
            if (E)
                T->priority = E->priority();
            else
                T->priority = -1;
        }
        else
        {
            T->priority = -1;
        }
    }

    // Update emmitters
    // Msg	("! update: emitters");
    for (it = 0; it < s_emitters.size(); it++)
    {
        CSoundRender_Emitter* pEmitter = s_emitters[it];
        if (pEmitter->marker != s_emitters_u)
        {
            pEmitter->update(dt_sec);
            pEmitter->marker = s_emitters_u;
        }
        if (!pEmitter->isPlaying())
        {
            // Stopped
            xr_delete(pEmitter);
            s_emitters.erase(s_emitters.begin() + it);
            it--;
        }
    }

    // Get currently rendering emitters
    // Msg	("! update: targets");
    s_targets_defer.clear();
    s_targets_pu++;
    // u32 PU				= s_targets_pu%s_targets.size();
    for (it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        if (T->get_emitter())
        {
            // Has emmitter, maybe just not started rendering
            if (T->get_Rendering())
            {
                T->fill_parameters(this);
                T->update();
            }
            else
                s_targets_defer.push_back(T);
        }
    }

    // Commit parameters from pending targets
    if (!s_targets_defer.empty())
    {
        // Msg	("! update: start render - commit");
        s_targets_defer.erase(std::unique(s_targets_defer.begin(), s_targets_defer.end()), s_targets_defer.end());
        for (it = 0; it < s_targets_defer.size(); it++)
            s_targets_defer[it]->fill_parameters(this);
    }

    // update EAX or EFX
    if (psSoundFlags.test(ss_EAX) && (bEAX || bEFX))
    {
        static shared_str curr_env;

        if (bListenerMoved || !m_op22_authored)
        {
            bListenerMoved = FALSE;
            m_op22_authored = get_environment(P);
            e_target = m_op22_authored;

            if (!curr_env.size() || curr_env != e_target->name)
            {
                curr_env = e_target->name;
                MsgDbg("~ current environment sound zone name [%s]", curr_env.c_str());
            }
        }
        // OP22 23.11: нет авторской зоны (среда по умолчанию) - динамическая акустика по лучам
        if (psSoundFlags.test(ss_op22_acoustics) && geom_MODEL && m_op22_authored == get_environment_def())
        {
            op22_acoustics(P);
            if (m_op22_ready)
                e_target = &m_op22_env;
        }
        else
            e_target = m_op22_authored;

        if (!e_currentPaused)
            e_current.lerp(e_current, *e_target, dt_sec);
        else
            e_current.set_from(*e_target);

        if (bEAX)
        {
            i_eax_listener_set(&e_current);
            i_eax_commit_setting();
        }
        else if (bEFX)
        {
            i_efx_listener_set(&e_current);
            bEFX = i_efx_commit_setting();

            if (!bEFX)
            {
                for (u32 it = 0; it < s_targets.size(); it++)
                {
                    CSoundRender_Target* T = s_targets[it];

                    T->alAuxInit(AL_EFFECTSLOT_NULL);

                    T->bEFX = false;
                }

                release_efx_objects();
            }
        }
    }

    // update listener
    update_listener(P, D, N, dt_sec);

    // Start rendering of pending targets
    if (!s_targets_defer.empty())
    {
        // Msg	("! update: start render");
        for (it = 0; it < s_targets_defer.size(); it++)
            s_targets_defer[it]->render();
    }

    // Events
    update_events();

    bLocked = FALSE;
}

static u32 g_saved_event_count = 0;
void CSoundRender_Core::update_events()
{
    g_saved_event_count = s_events.size();
    for (u32 it = 0; it < s_events.size(); it++)
    {
        event& E = s_events[it];
        Handler(E.first, E.second);
    }
    s_events.clear();
}

void CSoundRender_Core::statistic(CSound_stats* dest, CSound_stats_ext* ext)
{
    if (dest)
    {
        dest->_rendered = 0;
        for (u32 it = 0; it < s_targets.size(); it++)
        {
            CSoundRender_Target* T = s_targets[it];
            if (T->get_emitter() && T->get_Rendering())
                dest->_rendered++;
        }
        dest->_simulated = s_emitters.size();
        dest->_cache_hits = cache._stat_hit;
        dest->_cache_misses = cache._stat_miss;
        dest->_events = g_saved_event_count;
        cache.stats_clear();
    }
    if (ext)
    {
        for (u32 it = 0; it < s_emitters.size(); it++)
        {
            CSoundRender_Emitter* _E = s_emitters[it];
            CSound_stats_ext::SItem _I;
            _I._3D = !_E->b2D;
            _I._rendered = !!_E->target;
            _I.params = _E->p_source;
            _I.volume = _E->smooth_volume;
            if (_E->owner_data)
            {
                _I.name = _E->source()->fname;
                _I.game_object = _E->owner_data->g_object;
                _I.game_type = _E->owner_data->g_type;
                _I.type = _E->owner_data->s_type;
            }
            else
            {
                _I.game_object = 0;
                _I.game_type = 0;
                _I.type = st_Effect;
            }
            ext->append(_I);
        }
    }
}

float CSoundRender_Core::get_occlusion_to(const Fvector& hear_pt, const Fvector& snd_pt, float dispersion)
{
    float occ_value = 1.f;

    if (0 != geom_SOM)
    {
        // Calculate RAY params
        Fvector pos, dir;
        pos.random_dir();
        pos.mul(dispersion);
        pos.add(snd_pt);
        dir.sub(pos, hear_pt);
        float range = dir.magnitude();
        dir.div(range);

        geom_DB.ray_query(CDB::OPT_CULL, geom_SOM, hear_pt, dir, range);
        u32 r_cnt = u32(geom_DB.r_count());
        CDB::RESULT* _B = geom_DB.r_begin();

        if (0 != r_cnt)
        {
            for (u32 k = 0; k < r_cnt; k++)
            {
                CDB::RESULT* R = _B + k;
                occ_value *= *(float*)&R->dummy;
            }
        }
    }
    return occ_value;
}

float CSoundRender_Core::get_occlusion(Fvector& P, float R, Fvector* occ)
{
    float occ_value = 1.f;

    // Calculate RAY params
    Fvector base = listener_position();
    Fvector pos, dir;
    float range;
    pos.random_dir();
    pos.mul(R);
    pos.add(P);
    dir.sub(pos, base);
    range = dir.magnitude();
    dir.div(range);

    if (0 != geom_MODEL)
    {
        bool bNeedFullTest = true;
        // 1. Check cached polygon
        float _u, _v, _range;
        if (CDB::TestRayTri(base, dir, occ, _u, _v, _range, true))
            if (_range > 0 && _range < range)
            {
                occ_value = psSoundOcclusionScale;
                bNeedFullTest = false;
            }
        // 2. Polygon doesn't picked up - real database query
        if (bNeedFullTest)
        {
            geom_DB.ray_query(CDB::OPT_ONLYNEAREST, geom_MODEL, base, dir, range);
            if (0 != geom_DB.r_count())
            {
                // cache polygon
                const CDB::RESULT* R = geom_DB.r_begin();
                const CDB::TRI& T = geom_MODEL->get_tris()[R->id];
                const Fvector* V = geom_MODEL->get_verts();
                occ[0].set(V[T.verts[0]]);
                occ[1].set(V[T.verts[1]]);
                occ[2].set(V[T.verts[2]]);
                occ_value = psSoundOcclusionScale;
            }
        }
    }
    if (0 != geom_SOM)
    {
        geom_DB.ray_query(CDB::OPT_CULL, geom_SOM, base, dir, range);
        u32 r_cnt = u32(geom_DB.r_count());
        CDB::RESULT* _B = geom_DB.r_begin();

        if (0 != r_cnt)
        {
            for (u32 k = 0; k < r_cnt; k++)
            {
                CDB::RESULT* R = _B + k;
                occ_value *= *(float*)&R->dummy;
            }
        }
    }
    return occ_value;
}

// OP22 23.11: динамическая акустика. 13 направлений: вверх, 4 наискосок вверх (50°), 8 по горизонтали через 45°.
// Один луч не чаще 15 мс (поток звука seqFrameMT, не кадр), полный круг ~0,2 с; среда - смесь пресетов EAX по
// «потолок есть / стены близко / размер / туннель», дальше её плавно ведёт штатное e_current.lerp.
namespace
{
struct op22_preset
{
    float room, room_hf, decay, decay_hf, refl, refl_delay, reverb, reverb_delay, size, diffusion;
};
// значения EAX 2.0 (мБ, с): открытое место - почти сухо; двор/улица - ALLEY; комната - ROOM; зал - STONEROOM;
// ангар/цех - HANGAR с укороченным хвостом; туннель/коллектор - SEWERPIPE
constexpr op22_preset OP22_OPEN{-1800.f, -2500.f, 1.2f, .5f, -2600.f, .15f, -3000.f, .1f, 40.f, .2f};
constexpr op22_preset OP22_ALLEY{-1000.f, -270.f, 1.49f, .86f, -1204.f, .007f, -4.f, .011f, 7.5f, .3f};
constexpr op22_preset OP22_ROOM{-1000.f, -454.f, .4f, .83f, -1646.f, .002f, 53.f, .003f, 1.9f, 1.f};
constexpr op22_preset OP22_HALL{-1000.f, -300.f, 2.31f, .64f, -711.f, .012f, 83.f, .017f, 11.6f, 1.f};
constexpr op22_preset OP22_HANGAR{-1000.f, -1000.f, 6.f, .23f, -602.f, .02f, 198.f, .03f, 50.3f, 1.f};
constexpr op22_preset OP22_TUNNEL{-1000.f, -1000.f, 2.81f, .14f, 429.f, .014f, 1023.f, .021f, 1.7f, .8f};
constexpr float OP22_RANGE = 40.f;

const Fvector& op22_dir(u32 i)
{
    static Fvector d[13];
    static bool init = false;
    if (!init)
    {
        d[0].set(0.f, 1.f, 0.f);
        d[1].set(.64f, .77f, 0.f);
        d[2].set(-.64f, .77f, 0.f);
        d[3].set(0.f, .77f, .64f);
        d[4].set(0.f, .77f, -.64f);
        for (u32 k = 0; k < 8; ++k)
            d[5 + k].set(_cos(PI_MUL_2 * k / 8.f), 0.f, _sin(PI_MUL_2 * k / 8.f));
        for (auto& v : d)
            v.normalize();
        init = true;
    }
    return d[i];
}
} // namespace

void CSoundRender_Core::op22_acoustics(const Fvector& P)
{
    // один луч за раз, не чаще 15 мс
    if (fTimer_Value - m_op22_ray_t >= .015f)
    {
        m_op22_ray_t = fTimer_Value;
        const Fvector& dir = op22_dir(m_op22_ray);
        m_op22_db.ray_query(CDB::OPT_ONLYNEAREST, geom_MODEL, P, dir, OP22_RANGE);
        m_op22_dist[m_op22_ray] = m_op22_db.r_count() ? m_op22_db.r_begin()->range : OP22_RANGE;
        if (++m_op22_ray >= 13)
        {
            m_op22_ray = 0;
            m_op22_ready = true;
        }
        else if (!m_op22_ready)
            return;
    }
    else if (!m_op22_ready)
        return;

    const float* d = m_op22_dist;
    auto near_ = [](float x, float full, float none) { return std::clamp((none - x) / (none - full), 0.f, 1.f); };
    // потолок: прямо вверх весит вдвое; полностью - ближе 20 м, нет - дальше 30 м
    float up = 2.f * near_(d[0], 20.f, 30.f);
    for (u32 k = 1; k <= 4; ++k)
        up += near_(d[k], 20.f, 30.f);
    up /= 6.f;
    // стены: доля горизонтальных лучей с препятствием ближе 20 м (нет - дальше 30 м)
    float hor = 0.f, wall_sum = 0.f, wall_n = 0.f;
    for (u32 k = 5; k < 13; ++k)
    {
        hor += near_(d[k], 20.f, 30.f);
        if (d[k] < OP22_RANGE)
            wall_sum += d[k], wall_n += 1.f;
    }
    hor /= 8.f;
    // размер помещения: средняя дистанция до стен и высота потолка
    const float span = wall_n > 0.f ? wall_sum / wall_n : OP22_RANGE;
    const float size = .7f * span + .3f * _min(d[0], OP22_RANGE);
    // туннель: в одной оси узко (сумма встречных лучей < 8 м), в другой - далеко (> 20 м), над головой потолок
    float narrow = 1e9f, longest = 0.f;
    for (u32 k = 0; k < 4; ++k)
    {
        const float pair = d[5 + k] + d[5 + k + 4];
        narrow = _min(narrow, pair);
        longest = _max(longest, pair);
    }
    const float tunnel = std::clamp((longest - 20.f) / 15.f, 0.f, 1.f) * std::clamp((8.f - narrow) / 4.f, 0.f, 1.f) * up;

    const float w_open = (1.f - up) * (1.f - hor);
    const float w_alley = (1.f - up) * hor + .5f * up * (1.f - hor);
    const float w_in = up * hor + .5f * up * (1.f - hor);
    // внутри: комната (до 4 м) -> зал (до 10 м) -> ангар (от 18 м)
    const float t_hall = std::clamp((size - 4.f) / 6.f, 0.f, 1.f), t_hangar = std::clamp((size - 10.f) / 8.f, 0.f, 1.f);
    const float w_room = w_in * (1.f - tunnel) * (1.f - t_hall);
    const float w_hall = w_in * (1.f - tunnel) * t_hall * (1.f - t_hangar);
    const float w_hangar = w_in * (1.f - tunnel) * t_hall * t_hangar;
    const float w_tunnel = w_in * tunnel;
    const float wsum = _max(w_open + w_alley + w_room + w_hall + w_hangar + w_tunnel, 1e-4f);

    op22_preset r{};
    auto acc = [&](const op22_preset& p, float w) {
        w /= wsum;
        r.room += p.room * w, r.room_hf += p.room_hf * w, r.decay += p.decay * w, r.decay_hf += p.decay_hf * w;
        r.refl += p.refl * w, r.refl_delay += p.refl_delay * w, r.reverb += p.reverb * w, r.reverb_delay += p.reverb_delay * w;
        r.size += p.size * w, r.diffusion += p.diffusion * w;
    };
    acc(OP22_OPEN, w_open);
    acc(OP22_ALLEY, w_alley);
    acc(OP22_ROOM, w_room);
    acc(OP22_HALL, w_hall);
    acc(OP22_HANGAR, w_hangar);
    acc(OP22_TUNNEL, w_tunnel);

    CSoundRender_Environment& e = m_op22_env;
    e.set_default();
    static const shared_str op22_env_name{"op22_dynamic"}; // OP22: без поиска строки в общем хранилище на каждом такте звука
    e.name = op22_env_name;
    e.Room = r.room;
    e.RoomHF = r.room_hf;
    e.DecayTime = r.decay;
    e.DecayHFRatio = r.decay_hf;
    e.Reflections = r.refl;
    e.ReflectionsDelay = r.refl_delay;
    e.Reverb = r.reverb;
    e.ReverbDelay = r.reverb_delay;
    e.EnvironmentSize = r.size;
    e.EnvironmentDiffusion = r.diffusion;
    e.clamp();
}

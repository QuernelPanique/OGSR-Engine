//////////////////////////////////////////////////////////////////////
// HudItem.cpp: класс родитель для всех предметов имеющих
//				собственный HUD (CWeapon, CMissile etc)
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "op22_hud_motion.h" // OP22 (27.11)
#include "Inventory.h"
#include <cmath>
#include "HudItem.h"
#include "op22_suppression.h" // OP22
#include "player_hud.h"
#include "../xr_3da/gamemtllib.h"
#include <array>
#include "HUDManager.h"
#include "Weapon.h"
#include "ActorCondition.h"
#include "Missile.h"
#include "../xr_3da/x_ray.h"
#include "../../xr_3da/igame_persistent.h"
#include "Pda.h"
#include <ctime>

ENGINE_API extern float psHUD_FOV_def;
extern float g_op22_hud_soft, g_op22_hud_gait, g_op22_hud_bank, g_op22_hud_pivot; // OP22 (27.11, 3.251): ниже в файле

CHudItem::CHudItem()
{
    m_huditem_flags.zero();
    m_animation_slot = u32(-1);

    EnableHudInertion(TRUE);
    AllowHudInertion(TRUE);
    m_bobbing = std::make_unique<CWeaponBobbing>(this);
    m_current_motion_def = nullptr;
}

DLL_Pure* CHudItem::_construct()
{
    m_object = smart_cast<CPhysicItem*>(this);
    VERIFY(m_object);

    m_item = smart_cast<CInventoryItem*>(this);
    VERIFY(m_item);

    return (m_object);
}

void CHudItem::Load(LPCSTR section)
{
    world_sect = section;

    //загрузить hud, если он нужен
    if (pSettings->line_exist(section, "hud"))
    {
        hud_sect = pSettings->r_string(section, "hud");

        if (pSettings->line_exist(hud_sect, "allow_inertion"))
            EnableHudInertion(pSettings->r_bool(hud_sect, "allow_inertion"));

        if (pSettings->line_exist(hud_sect, "allow_bobbing"))
            allow_bobbing = pSettings->r_bool(hud_sect, "allow_bobbing");

        hud_recalc_koef = READ_IF_EXISTS(pSettings, r_float, hud_sect, "hud_recalc_koef",
                                         1.35f); //На калаше при 1.35 вроде норм смотрится, другим стволам возможно придется подбирать другие значения.
    }

    m_animation_slot = pSettings->r_u32(section, "animation_slot");

    m_nearwall_on = READ_IF_EXISTS(pSettings, r_bool, section, "nearwall_on", IS_OGSR_GA ? true : READ_IF_EXISTS(pSettings, r_bool, "features", "default_nearwall_on", true));
    AimAlt = READ_IF_EXISTS(pSettings, r_bool, section, "use_alt_aim_hud", false); // OP22

    if (m_nearwall_on)
    {
        // Координаты офсетов для сдвига худа нашел в интернетах :)
        static std::array<std::tuple<float, float, Fvector, Fvector, float, float>, _CollisionWeaponTypesCount_> CollisionParamsBase{{
            // Min, Max dist, offset, rotate, HudFov, HudFov Aim
            {0.25f, 0.95f, {-0.0615f, -0.4380f, 0.1235f}, {-0.9219f, -0.0972f, 0.2525f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // Общие для всех оружий
            {0.25f, 0.70f, {-0.1000f, -0.5537f, 0.0350f}, {-1.0630f, 0.1751f, -0.0600f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.20f : 0.10f}, // Пистолеты
            {0.30f, 1.30f, {-0.0615f, -0.4380f, 0.1235f}, {-0.9219f, -0.0972f, 0.2525f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // СВД и прочие длинные снайперки
            {0.35f, 1.85f, {-0.0399f, 0.0929f, -0.0589f}, {0.3908f, 0.0488f, -0.0193f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // РПГ
            {0.25f, 0.80f, {0.0015f, -0.5655f, 0.1240f}, {-1.0319f, 0.0678f, 0.0700f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // РГ-6
            {0.25f, 0.80f, {-0.0406f, -0.4191f, 0.1718f}, {-0.8981f, -0.1101f, 0.4420f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // Гроза
            {0.25f, 0.80f, {-0.0335f, -0.4618f, 0.1098f}, {-0.9119f, -0.0973f, 0.4143f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // ФН2000
            {0.25f, 0.60f, {-0.0650f, -0.5170f, 0.0465f}, {-1.0405f, 0.1051f, -0.0350f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // БМ-16
            {0.30f, 0.50f, {-0.0025f, -0.4045f, -0.1415f}, {-0.7900f, 0.0100f, 0.f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // Болт
            {0.25f, 0.65f, {0.0120f, -0.4780f, -0.1150f}, {-0.6250f, -0.0725f, -0.1950f}, 0.5f, 1.f}, //Детектор
            {0.30f, 0.50f, {-0.0025f, -0.4045f, -0.1415f}, {-0.7900f, 0.0100f, 0.f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.25f : 0.15f}, // Нож, гранаты и прочее
            {0.25f, 0.70f, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, IS_OGSR_GA ? 0.5f : 0.25f, IS_OGSR_GA ? 0.40f : 0.30f}, // Бинокль
        }};
        const size_t type = GetWeaponTypeForCollision();
        const auto& CollisionParams = CollisionParamsBase.at(type);

        // Параметры изменения коллизии когда игрок стоит вплотную к стене
        m_nearwall_hud_offset_speed = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_hud_offset_speed", 0.1f); // Скорость поднятия\опускания ствола
        m_nearwall_dist_min = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_dist_min", std::get<0>(CollisionParams)); //Максимальное расстояние, на которое камера ГГ может упереться к стене
        m_nearwall_dist_max = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_dist_max", std::get<1>(CollisionParams)); //Расстояние, ближе которого начинаем поднимать ствол
        m_nearwall_target_hud_offset = READ_IF_EXISTS(pSettings, r_fvector3, section, "nearwall_target_hud_offset", std::get<2>(CollisionParams)); //Максимальный оффсет худа
        m_nearwall_target_hud_rotate = READ_IF_EXISTS(pSettings, r_fvector3, section, "nearwall_target_hud_rotate", std::get<3>(CollisionParams)); //Максимальный поворот худа
        m_nearwall_target_hud_fov = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_target_hud_fov", std::get<4>(CollisionParams));
        m_nearwall_target_aim_hud_fov = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_target_aim_hud_fov", std::get<5>(CollisionParams));
        m_nearwall_speed_mod = READ_IF_EXISTS(pSettings, r_float, section, "nearwall_speed_mod", 10.f);
    }

    //if (pSettings->line_exist(hud_sect, "hud_fov"))
    //    m_base_fov = READ_IF_EXISTS(pSettings, r_float, hud_sect, "hud_fov", 0.0f);
    //else
        m_base_fov = READ_IF_EXISTS(pSettings, r_float, section, "hud_fov", 0.0f);

    m_nearwall_last_hud_fov = m_base_fov > 0.0f ? m_base_fov : psHUD_FOV_def;

    // OP22 (27.11, 3.251): точка опоры поворотов рук (op22_hud_pivot), оси худа (x вправо, y вверх, z вперёд, 0 - глаз):
    // винтовка - приклад в плече, пистолет - кисть, остальное - кисть руки; в прицеле опора ближе к линии глаза
    switch (GetWeaponTypeForCollision())
    {
    case Pistol:
        m_op22_pivot[0].set(0.06f, -0.13f, 0.22f);
        m_op22_pivot[1].set(0.f, -0.06f, 0.24f);
        break;
    case RPG:
        m_op22_pivot[0].set(0.05f, -0.05f, 0.f);
        m_op22_pivot[1].set(0.f, -0.03f, -0.02f);
        break;
    case Bolt:
    case Detector:
    case Knife_and_other:
    case Binocular:
        m_op22_pivot[0].set(0.07f, -0.15f, 0.20f);
        m_op22_pivot[1].set(0.035f, -0.08f, 0.15f);
        break;
    default:
        m_op22_pivot[0].set(0.08f, -0.16f, 0.f);
        m_op22_pivot[1].set(0.f, -0.10f, -0.04f);
        break;
    }

    ////////////////////////////////////////////
    m_strafe_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_hud_offset_pos", (Fvector{0.025f, 0.f, 0.f}));
    m_strafe_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_hud_offset_rot", (Fvector{0.f, 0.f, 5.5f}));

    m_strafe_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_aim_hud_offset_pos", (Fvector{0.f, 0.f, 0.f}));
    m_strafe_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "strafe_aim_hud_offset_rot", (Fvector{0.f, 0.f, 3.5f}));

    m_strafe_offset[2][0].set(READ_IF_EXISTS(pSettings, r_bool, section, "strafe_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "strafe_transition_time", 0.25f), 0.f);
    m_strafe_offset[2][1].set(READ_IF_EXISTS(pSettings, r_bool, section, "strafe_aim_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "strafe_aim_transition_time", 0.15f), 0.f);

    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_lookout_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_hud_offset_pos", (Fvector{0.045f, 0.f, 0.f}));
    m_lookout_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_hud_offset_rot", (Fvector{0.f, 0.f, 10.f}));

    m_lookout_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_aim_hud_offset_pos", (Fvector{0.f, 0.f, 0.f}));
    m_lookout_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "lookout_aim_hud_offset_rot", (Fvector{0.f, 0.f, 15.f}));

    m_lookout_offset[2][0].set(READ_IF_EXISTS(pSettings, r_bool, section, "lookout_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "lookout_transition_time", 0.25f), 0.f);
    m_lookout_offset[2][1].set(READ_IF_EXISTS(pSettings, r_bool, section, "lookout_aim_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "lookout_aim_transition_time", 0.15f), 0.f);
    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_jump_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "jump_hud_offset_pos", (Fvector{0.f, 0.05f, 0.03f}));
    m_jump_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "jump_hud_offset_rot", (Fvector{0.f, -10.f, -10.f}));

    m_jump_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "jump_aim_hud_offset_pos", (Fvector{0.f, 0.03f, 0.01f}));
    m_jump_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "jump_aim_hud_offset_rot", (Fvector{0.f, 2.5f, -3.f}));

    m_jump_offset[2][0].set(READ_IF_EXISTS(pSettings, r_bool, section, "jump_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "jump_transition_time", 0.35f), 0.f);
    m_jump_offset[2][1].set(READ_IF_EXISTS(pSettings, r_bool, section, "jump_aim_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "jump_aim_transition_time", 0.4f), 0.f);
    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_fall_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "fall_hud_offset_pos", (Fvector{0.f, -0.05f, 0.06f}));
    m_fall_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "fall_hud_offset_rot", (Fvector{0.f, 5.f, 0.f}));

    m_fall_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "fall_aim_hud_offset_pos", (Fvector{0.f, 0.03f, -0.01f}));
    m_fall_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "fall_aim_hud_offset_rot", (Fvector{0.f, -2.5f, 3.f}));
    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_landing_offset[0][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "landing_hud_offset_pos", (Fvector{0.f, -0.2f, 0.03f}));
    m_landing_offset[1][0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "landing_hud_offset_rot", (Fvector{0.f, -5.f, 10.f}));

    m_landing_offset[0][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "landing_aim_hud_offset_pos", (Fvector{0.f, -0.1f, 0.02f}));
    m_landing_offset[1][1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "landing_aim_hud_offset_rot", (Fvector{0.f, -2.5f, 5.f}));
    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_move_offset[0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "stay_hud_offset_pos", (Fvector{0.f, -0.03f, 0.f}));
    m_move_offset[1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "stay_hud_offset_rot", (Fvector{0.f, 0.5f, -3.f}));
    m_move_offset[2].set(READ_IF_EXISTS(pSettings, r_bool, section, "move_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "move_transition_time", 0.25f), 0.f);
    ////////////////////////////////////////////
    ////////////////////////////////////////////
    m_walk_offset[0] = READ_IF_EXISTS(pSettings, r_fvector3, section, "walk_hud_offset_pos", (Fvector{-0.02f, -0.02f, -0.03f}));
    m_walk_offset[1] = READ_IF_EXISTS(pSettings, r_fvector3, section, "walk_hud_offset_rot", (Fvector{0.f, 0.05f, -1.f}));
    m_walk_offset[2].set(READ_IF_EXISTS(pSettings, r_bool, section, "walk_enabled", true), READ_IF_EXISTS(pSettings, r_float, section, "walk_transition_time", 0.25f), 0.f);

    //Загрузка параметров инерции --#SM+# Begin--
    constexpr float PITCH_OFFSET_R = 0.0f; // Насколько сильно ствол смещается вбок (влево) при вертикальных поворотах камеры
    constexpr float PITCH_OFFSET_N = 0.0f; // Насколько сильно ствол поднимается\опускается при вертикальных поворотах камеры
    constexpr float PITCH_OFFSET_D = 0.02f; // Насколько сильно ствол приближается\отдаляется при вертикальных поворотах камеры
    constexpr float PITCH_LOW_LIMIT = -PI; // Минимальное значение pitch при использовании совместно с PITCH_OFFSET_N
    constexpr float ORIGIN_OFFSET = -0.03f; // Фактор влияния инерции на положение ствола (чем меньше, тем масштабней инерция)
    constexpr float ORIGIN_OFFSET_AIM = -0.02f; // (Для прицеливания)
    constexpr float TENDTO_SPEED = 5.f; // Скорость нормализации положения ствола
    constexpr float TENDTO_SPEED_AIM = 10.f; // (Для прицеливания)

    inertion_data.m_pitch_offset_r = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_right", PITCH_OFFSET_R);
    inertion_data.m_pitch_offset_n = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_up", PITCH_OFFSET_N);
    inertion_data.m_pitch_offset_d = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_forward", PITCH_OFFSET_D);
    inertion_data.m_pitch_low_limit = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_up_low_limit", PITCH_LOW_LIMIT);

    inertion_data.m_origin_offset = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_origin_offset", ORIGIN_OFFSET);
    inertion_data.m_origin_offset_aim = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_zoom_origin_offset", ORIGIN_OFFSET_AIM);
    inertion_data.m_tendto_speed = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_tendto_speed", TENDTO_SPEED);
    inertion_data.m_tendto_speed_aim = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_zoom_tendto_speed", TENDTO_SPEED_AIM);
    //--#SM+# End--
}

// OP22: то же, что CHudItem::Load читает из hud-секции (инерция, покачивание, коэффициент пересчёта)
void CHudItem::op22_LoadHudBase()
{
    if (!hud_sect.size())
        return;
    if (pSettings->line_exist(hud_sect, "allow_inertion"))
        EnableHudInertion(pSettings->r_bool(hud_sect, "allow_inertion"));
    if (pSettings->line_exist(hud_sect, "allow_bobbing"))
        allow_bobbing = pSettings->r_bool(hud_sect, "allow_bobbing");
    hud_recalc_koef = READ_IF_EXISTS(pSettings, r_float, hud_sect, "hud_recalc_koef", 1.35f);

    inertion_data.m_pitch_offset_r = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_right", 0.0f);
    inertion_data.m_pitch_offset_n = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_up", 0.0f);
    inertion_data.m_pitch_offset_d = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_forward", 0.02f);
    inertion_data.m_pitch_low_limit = READ_IF_EXISTS(pSettings, r_float, hud_sect, "pitch_offset_up_low_limit", -PI);
    inertion_data.m_origin_offset = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_origin_offset", -0.03f);
    inertion_data.m_origin_offset_aim = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_zoom_origin_offset", -0.02f);
    inertion_data.m_tendto_speed = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_tendto_speed", 5.f);
    inertion_data.m_tendto_speed_aim = READ_IF_EXISTS(pSettings, r_float, hud_sect, "inertion_zoom_tendto_speed", 10.f);
}

void CHudItem::PlaySound(HUD_SOUND& hud_snd, const Fvector& position, bool overlap) { HUD_SOUND::PlaySound(hud_snd, position, object().H_Root(), !!GetHUDmode(), false, overlap); }

void CHudItem::net_Destroy() { m_dwStateTime = 0; }

BOOL CHudItem::net_Spawn(CSE_Abstract* DC) { return TRUE; }

void CHudItem::renderable_Render(u32 context_id, IRenderable* root)
{
    UpdateXForm();

    const bool _hud_render = root && root->renderable_HUD() && GetHUDmode();
    if (_hud_render && !IsHidden())
        return;

    if (!object().H_Parent() || (!_hud_render && !IsHidden()))
    {
        on_renderable_Render(context_id, root);
    }
    else if (object().H_Parent())
    {
        const CInventoryOwner* owner = smart_cast<CInventoryOwner*>(object().H_Parent());
        VERIFY(owner);
        const CInventoryItem* self = smart_cast<CInventoryItem*>(this);
        if (owner->attached(self))
            on_renderable_Render(context_id, root);
    }
}

bool CHudItem::Action(s32 cmd, u32 flags) { return false; }

void CHudItem::SwitchState(u32 S)
{
    SetNextState(S); // Very-very important line of code!!! :)

    if (object().Local() && !object().getDestroy())
    {
        // !!! Just single entry for given state !!!
        NET_Packet P;
        object().u_EventGen(P, GE_WPN_STATE_CHANGE, object().ID());
        P.w_u8(u8(S));
        object().u_EventSend(P);
    }
}

void CHudItem::OnEvent(NET_Packet& P, u16 type)
{
    switch (type)
    {
    case GE_WPN_STATE_CHANGE: {
        u8 S;
        P.r_u8(S);
        OnStateSwitch(u32(S), GetState());
    }
    break;
    }
}

void CHudItem::OnStateSwitch(u32 S, u32 oldState)
{
    m_dwStateTime = 0;
    SetState(S);
    if (object().Remote())
        SetNextState(S);

    if (S == eHidden)
    {
        m_nearwall_last_hud_fov = m_base_fov > 0.0f ? m_base_fov : psHUD_FOV_def;
        SprintType = false;
    }
    else if (S == eSprintStart)
        PlayAnimSprintStart();
    else if (S == eSprintEnd)
        PlayAnimSprintEnd();
    else if (S != eIdle)
        SprintType = false;

    g_player_hud->updateMovementLayerState();
}

bool CHudItem::Activate(bool now)
{
    Show(now);
    OnActiveItem();
    return true;
}

void CHudItem::Deactivate(bool now)
{
    Hide(now);
    OnHiddenItem();
}

void CHudItem::UpdateCL()
{
    m_dwStateTime += Device.dwTimeDelta;

    if (m_current_motion_def)
    {
        if (m_bStopAtEndAnimIsRunning)
        {
            const xr_vector<motion_marks>& marks = m_current_motion_def->marks;
            if (!marks.empty())
            {
                float motion_prev_time = ((float)m_dwMotionCurrTm - (float)m_dwMotionStartTm) / 1000.0f;
                float motion_curr_time = ((float)Device.dwTimeGlobal - (float)m_dwMotionStartTm) / 1000.0f;

                xr_vector<motion_marks>::const_iterator it = marks.begin();
                xr_vector<motion_marks>::const_iterator it_e = marks.end();
                for (; it != it_e; ++it)
                {
                    const motion_marks& M = (*it);
                    if (M.is_empty())
                        continue;

                    const motion_marks::interval* Iprev = M.pick_mark(motion_prev_time);
                    const motion_marks::interval* Icurr = M.pick_mark(motion_curr_time);
                    if (Iprev == nullptr && Icurr != nullptr /* || M.is_mark_between(motion_prev_time, motion_curr_time)*/)
                    {
                        OnMotionMark(m_startedMotionState, M);
                    }
                }
            }

            m_dwMotionCurrTm = Device.dwTimeGlobal;
            if (m_dwMotionCurrTm > m_dwMotionEndTm)
            {
                m_current_motion_def = nullptr;
                m_dwMotionStartTm = 0;
                m_dwMotionEndTm = 0;
                m_dwMotionCurrTm = 0;
                m_bStopAtEndAnimIsRunning = false;
                OnAnimationEnd(m_startedMotionState);
            }
        }
    }

    AllowHudBobbing((psActorFlags.test(AF_WEAPON_BOBBING) && allow_bobbing) || (g_actor && g_actor->PsyAuraAffect));

    TimeLockAnimation();
}

void CHudItem::OnH_A_Chield() {}

void CHudItem::OnH_B_Chield()
{
    StopCurrentAnimWithoutCallback();

    m_nearwall_last_hud_fov = m_base_fov > 0.0f ? m_base_fov : psHUD_FOV_def;
}

void CHudItem::OnH_B_Independent(bool just_before_destroy)
{
    StopHUDSounds();
    UpdateXForm();

    m_nearwall_last_hud_fov = m_base_fov > 0.0f ? m_base_fov : psHUD_FOV_def;
}

void CHudItem::OnH_A_Independent()
{
    if (HudItemData())
        g_player_hud->detach_item(this);
    StopCurrentAnimWithoutCallback();
}

void CHudItem::on_b_hud_detach() {}

void CHudItem::on_a_hud_attach()
{
    if (m_current_motion_def)
    {
        PlayHUDMotion_noCB(m_current_motion, false);
#ifdef DEBUG
        //		Msg("continue playing [%s][%d]",m_current_motion.c_str(), Device.dwFrame);
#endif // #ifdef DEBUG
    }
    else
    {
#ifdef DEBUG
        //		Msg("no active motion");
#endif // #ifdef DEBUG
    }
}

u32 CHudItem::PlayHUDMotion(const char* M, const bool bMixIn, const u32 state, const bool randomAnim, float speed)
{
    auto Wpn = g_player_hud->attached_item(0);
    auto Det = g_player_hud->attached_item(1);

    if (Det && Det->m_parent_hud_item != this && Wpn && Wpn->m_parent_hud_item == this && (smart_cast<CWeapon*>(this) || smart_cast<CMissile*>(this)) &&
        Det->m_parent_hud_item->GetState() == eIdle)
    {
        if (strstr(M, "anm_") && !strstr(M, "idle"))
        { 
            //с айдловыми анимациями слишком много багов
            string128 det_anm_name;
            xr_strconcat(det_anm_name, "anm_lefthand_", Det->m_parent_hud_item->world_sect.c_str(), "_wpn_", M + 4);
            if (Det->m_parent_hud_item->AnimationExist(det_anm_name))
                Det->m_parent_hud_item->PlayHUDMotion(det_anm_name, true, Det->m_parent_hud_item->GetState());
        }
    }

    // Msg("~~[%s] Playing motion [%s] for [%s]", __FUNCTION__, M.c_str(), HudSection().c_str());
    u32 anim_time = PlayHUDMotion_noCB(M, bMixIn, randomAnim, speed);
    if (anim_time > 0)
    {
        m_bStopAtEndAnimIsRunning = true;
        m_dwMotionStartTm = Device.dwTimeGlobal;
        m_dwMotionCurrTm = m_dwMotionStartTm;
        m_dwMotionEndTm = m_dwMotionStartTm + anim_time;
        m_startedMotionState = state;
    }
    else
        m_bStopAtEndAnimIsRunning = false;

    return anim_time;
}

u32 CHudItem::PlayHUDMotion(std::initializer_list<const char*> Ms, const bool bMixIn, const u32 state, const bool randomAnim, float speed)
{
    for (const auto* M : Ms)
        if (AnimationExist(M))
            return PlayHUDMotion(M, bMixIn, state, randomAnim, speed);
    /*
    xr_string dbg_anim_name;
    for (const auto* M : Ms) {
        dbg_anim_name += M;
        dbg_anim_name += ", ";
    }
    Msg("~~[%s] Motions [%s] not found for [%s]", __FUNCTION__, dbg_anim_name.c_str(), HudSection().c_str());
    */
    return 0;
}

u32 CHudItem::PlayHUDMotion_noCB(const shared_str& motion_name, const bool bMixIn, const bool randomAnim, float speed)
{
    m_current_motion = motion_name;

    if (HudItemData())
    {
        return HudItemData()->anim_play(motion_name, bMixIn, m_current_motion_def, randomAnim, speed);
    }
    else
    {
        return g_player_hud->motion_length(motion_name, HudSection(), m_current_motion_def, speed);
    }
}

void CHudItem::StopCurrentAnimWithoutCallback()
{
    m_dwMotionStartTm = 0;
    m_dwMotionEndTm = 0;
    m_dwMotionCurrTm = 0;
    m_bStopAtEndAnimIsRunning = false;
    m_current_motion_def = nullptr;
    m_dwStateTime = 0;
}

BOOL CHudItem::GetHUDmode()
{
    if (object().H_Parent())
    {
        CActor* A = smart_cast<CActor*>(object().H_Parent());
        return (A && A->HUDview() && HudItemData());
    }
    else
        return FALSE;
}

void CHudItem::PlayBlendAnm(LPCSTR name, float speed, float power, bool stop_old)
{
    u8 part = (object().cast_weapon()->IsZoomed() ? 2 : (g_player_hud->attached_item(1) ? 0 : 2));

    if (stop_old)
        g_player_hud->StopBlendAnm(name, true);
    g_player_hud->PlayBlendAnm(name, part, speed, power, false);
}

void CHudItem::PlayAnimIdle()
{
    if (TryPlayAnimIdle())
        return;

    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion({(wpn && wpn->IsMisfire()) ?
                       "anm_idle_jammed" :
                       ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "anm_idle_empty" : "nullptr"),
                   "anim_idle", "anm_idle"},
                  true, GetState());
}

void CHudItem::PlayAnimSprintStart()
{
    auto wpn = smart_cast<CWeapon*>(this);
    string128 guns_sprint_start_anm;
    xr_strconcat(guns_sprint_start_anm, "anm_idle_sprint_start",
                 (wpn && wpn->IsMisfire()) ?
                     "_jammed" :
                     ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "_empty" : ""),
                 (wpn && wpn->IsGrenadeLauncherAttached()) ? (wpn && wpn->IsGrenadeMode() ? "_g" : "_w_gl") : "");
    if (AnimationExist(guns_sprint_start_anm))
        PlayHUDMotion(guns_sprint_start_anm, true, GetState());
    else
    {
        SprintType = true;
        SwitchState(eIdle);
    }
}

void CHudItem::PlayAnimSprintEnd()
{
    auto wpn = smart_cast<CWeapon*>(this);
    string128 guns_sprint_end_anm;
    xr_strconcat(guns_sprint_end_anm, "anm_idle_sprint_end",
                 (wpn && wpn->IsMisfire()) ?
                     "_jammed" :
                     ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "_empty" : ""),
                 (wpn && wpn->IsGrenadeLauncherAttached()) ? (wpn && wpn->IsGrenadeMode() ? "_g" : "_w_gl") : "");
    if (AnimationExist(guns_sprint_end_anm))
        PlayHUDMotion(guns_sprint_end_anm, true, GetState());
    else
    {
        SprintType = false;
        SwitchState(eIdle);
    }
}

bool CHudItem::TryPlayAnimIdle()
{
    if (!IsZoomed() || !smart_cast<CWeapon*>(this))
    {
        if (auto pActor = smart_cast<CActor*>(object().H_Parent()))
        {
            const u32 State = pActor->get_state();
            if (State & mcSprint)
            {
                if (!SprintType)
                {
                    SwitchState(eSprintStart);
                    return true;
                }
                PlayAnimIdleSprint();
                return true;
            }
            else if (SprintType)
            {
                SwitchState(eSprintEnd);
                return true;
            }
            else if ((State & mcAnyMove) && AnmIdleMovingAllowed())
            {
                if (!(State & mcCrouch))
                {
                    if (State & mcAccel) //Ходьба медленная (SHIFT)
                        PlayAnimIdleMovingSlow();
                    else
                        PlayAnimIdleMoving();
                    return true;
                }
                else if (State & mcAccel) //Ходьба в присяде (CTRL+SHIFT)
                {
                    PlayAnimIdleMovingCrouchSlow();
                    return true;
                }
                else
                {
                    PlayAnimIdleMovingCrouch();
                    return true;
                }
            }
        }
    }
    return false;
}

/*void CHudItem::PlayAnimBore()
{
    PlayHUDMotion({ "anim_idle", "anm_bore" }, true, GetState());
}*/

bool CHudItem::AnimationExist(const char* anim_name) const
{
    if (HudItemData())
    {
        string256 anim_name_r;
        bool is_16x9 = UI()->is_widescreen();
        u16 attach_place_idx = HudItemData()->m_attach_place_idx;
        xr_sprintf(anim_name_r, "%s%s", anim_name, (attach_place_idx == 1 && is_16x9) ? "_16x9" : "");
        player_hud_motion* anm = HudItemData()->find_motion(anim_name_r);
        if (anm)
            return true;
    }
    else // Third person
    {
        const CMotionDef* temp_motion_def;
        if (g_player_hud->motion_length(anim_name, HudSection(), temp_motion_def) > 100)
            return true;
    }
#ifdef DEBUG
    Msg("~ [WARNING] ------ Animation [%s] does not exist in [%s]", anim_name, HudSection().c_str());
#endif
    return false;
}

void CHudItem::PlayAnimIdleMoving()
{
    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion(
        {(wpn && wpn->IsMisfire()) ?
             "anm_idle_moving_jammed" :
             ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "anm_idle_moving_empty" : "nullptr"),
         "anm_idle_moving", "anm_idle", "anim_idle_moving", "anim_idle"},
        true, GetState());
}

void CHudItem::PlayAnimIdleMovingSlow()
{
    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion(
        {(wpn && wpn->IsMisfire()) ?
             "anm_idle_moving_slow_jammed" :
             ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "anm_idle_moving_slow_empty" : "nullptr"),
         "anm_idle_moving_slow", "anm_idle_moving_slow", "anm_idle", "anim_idle_moving", "anim_idle"},
        true, GetState());
}

void CHudItem::PlayAnimIdleMovingCrouch()
{
    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion({(wpn && wpn->IsMisfire()) ?
                       "anm_idle_moving_crouch_jammed" :
                       ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "anm_idle_moving_crouch_empty" :
                                                                                                                                                    "nullptr"),
                   "anm_idle_moving_crouch", "anm_idle_moving", "anm_idle", "anim_idle_moving", "anim_idle"},
                  true, GetState());
}

void CHudItem::PlayAnimIdleMovingCrouchSlow()
{
    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion({(wpn && wpn->IsMisfire()) ? "anm_idle_moving_crouch_slow_jammed" :
                                               ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ?
                                                    "anm_idle_moving_crouch_slow_empty" :
                                                    "nullptr"),
                   "anm_idle_moving_crouch_slow", "anm_idle_moving_crouch", "anm_idle_moving", "anm_idle", "anim_idle_moving", "anim_idle"},
                  true, GetState());
}

void CHudItem::PlayAnimIdleSprint()
{
    auto wpn = smart_cast<CWeapon*>(this);
    PlayHUDMotion(
        {(wpn && wpn->IsMisfire()) ?
             "anm_idle_sprint_jammed" :
             ((wpn && ((wpn->GetAmmoElapsed() == 0 && !wpn->IsGrenadeMode()) || (wpn->GetAmmoElapsed2() == 0 && wpn->IsGrenadeMode()))) ? "anm_idle_sprint_empty" : "nullptr"),
         "anm_idle_sprint", "anm_idle", "anim_idle_sprint", "anim_idle"},
        true, GetState());
}

bool CHudItem::NeedBlendAnm()
{
    u32 state = GetState();
    return (state != eIdle && state != eHidden);
}

void CHudItem::OnMovementChanged(ACTOR_DEFS::EMoveCommand cmd)
{
    if (GetState() == eIdle && !m_bStopAtEndAnimIsRunning)
    {
        PlayAnimIdle();
        ResetSubStateTime();
    }
}

attachable_hud_item* CHudItem::HudItemData() const
{
    attachable_hud_item* hi = nullptr;
    if (!g_player_hud)
        return hi;

    hi = g_player_hud->attached_item(0);
    if (hi && hi->m_parent_hud_item == this)
        return hi;

    hi = g_player_hud->attached_item(1);
    if (hi && hi->m_parent_hud_item == this)
        return hi;

    return nullptr;
}

bool CHudItem::used_cop_fire_point() const
{
    auto hd = HudItemData();
    return hd && hd->m_measures.useCopFirePoint;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Сделано на базе увиденного в старом скрипте от Shoker.
// Перевёл его в движок и доработал под современные реалии для OGSR Engine: KRodin
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TODO: Желательно бы сделать визуальную настройку всех этих офсетов через hud_adjust
// TODO: Придумать более годный способ получения полиции и направления для коллизии болтов, детекторов, биноклей и ножей. У стволов есть shoot_point - там легко считать от него, в
// отличие от.
// TODO: Придумать какой-то способ получения нужной позиции для рейтрейса. Сейчас сделал не оптиматьно, по сути худ считается два раза за фрейм (не полностью, но такое решение всё
// равно выглядит плохо)
//              Эти костыли для двойного рассчета можно найти по skip_updated_frame, need_update_collision
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CHudItem::CollisionAllowed() const
{
    //Если выкл реалистичный прицел или у ствола ТЧ-стайл фейр поинт или ствол в режиме зума - коллизия работать не будет.
    return m_nearwall_on && psHUD_Flags.test(HUD_CROSSHAIR_HARD) && used_cop_fire_point();
}

void CHudItem::UpdateCollision(Fmatrix& trans)
{
    if (!CollisionAllowed())
        return;

    skip_updated_frame = Device.dwFrame;

    collide::rq_result RQ{nullptr, m_nearwall_dist_max * 1.1f, -1};
    const collide::ray_defs RD{GetPositionForCollision(), GetDirectionForCollision(), RQ.range, CDB::OPT_CULL, collide::rqtBoth};
    collide::rq_results RQR;

    Level().ObjectSpace.RayQuery(
        RQR, RD,
        [](collide::rq_result& result, LPVOID params) {
            //Копипаст из  CHUDTarget
            auto RQ = reinterpret_cast<collide::rq_result*>(params);

            if (result.O)
            {
                if (auto* e = smart_cast<CEntityAlive*>(result.O); e && e->g_Alive())
                    return TRUE;

                //if (smart_cast<CCustomShell*>(result.O))
                //    return TRUE;

                *RQ = result;
                return FALSE;
            }

            CDB::TRI* T = Level().ObjectSpace.GetStaticTris() + result.element;
            SGameMtl* mtl = GMLib.GetMaterialByIdx(T->material);
            if (mtl->Flags.is(SGameMtl::flPassable))
                return TRUE;

            *RQ = result;
            return FALSE;
        },
        &RQ, nullptr, Level().CurrentEntity());

    saved_rq_range = RQ.range;
    clamp(RQ.range, m_nearwall_dist_min, RQ.range);

    float dist_coef{};
    if (RQ.range <= m_nearwall_dist_max)
        dist_coef = 1.f - (RQ.range - m_nearwall_dist_min) / (m_nearwall_dist_max - m_nearwall_dist_min);

    const float fStepPerUpd = Device.fTimeDelta / m_nearwall_hud_offset_speed; // Величина изменение фактора смещения коллизии худа

    auto curr_offs = Fvector{}.mul(m_nearwall_target_hud_offset, dist_coef), curr_rot = Fvector{}.mul(m_nearwall_target_hud_rotate, dist_coef);

    // Плавный переход между смещением от коллизии и прицеливания
    curr_offs.mul(!IsZoomed());
    curr_rot.mul(!IsZoomed());

    //auto wpn = smart_cast<CWeapon*>(this);
    //curr_offs.mul(wpn && !wpn->LoweredActive);
    //curr_rot.mul(wpn && !wpn->LoweredActive);

    if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251): ствол у стены поднимается/опускается с разгоном и торможением
    {
        op22_Ease(m_nearwall_last_pos, m_op22_soft_v[5][0], curr_offs, m_nearwall_hud_offset_speed * 1.5f, 0.9f);
        op22_Ease(m_nearwall_last_rot, m_op22_soft_v[5][1], curr_rot, m_nearwall_hud_offset_speed * 1.5f, 0.9f);
    }
    else
    {
        m_op22_soft_v[5][0].set(0.f, 0.f, 0.f);
        m_op22_soft_v[5][1].set(0.f, 0.f, 0.f);
        if (!curr_offs.similar(m_nearwall_last_pos, EPS))
            m_nearwall_last_pos.lerp(m_nearwall_last_pos, curr_offs, fStepPerUpd);
        if (!curr_rot.similar(m_nearwall_last_rot, EPS))
            m_nearwall_last_rot.lerp(m_nearwall_last_rot, curr_rot, fStepPerUpd);
    }

    Fmatrix hud_rotation;
    hud_rotation.identity();
    hud_rotation.rotateX(m_nearwall_last_rot.x);

    Fmatrix hud_rotation_y;
    hud_rotation_y.identity();
    hud_rotation_y.rotateY(m_nearwall_last_rot.y);
    hud_rotation.mulA_43(hud_rotation_y);

    hud_rotation_y.identity();
    hud_rotation_y.rotateZ(m_nearwall_last_rot.z);
    hud_rotation.mulA_43(hud_rotation_y);

    hud_rotation.translate_over(m_nearwall_last_pos);
    trans.mulB_43(hud_rotation);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float g_op22_hud_inertion = 100.f; // OP22: op22_console.cpp
float g_op22_hud_inertion_spring = 1.f; // OP22 (27.11): инерция 2-го порядка, 0 - прежняя (op22_console.cpp)
float g_op22_hud_steps = 1.f; // OP22 (27.11): толчок рук на шаге, 0 - выкл
float g_op22_hud_hit = 1.f; // OP22 (27.11): толчок рук от попадания, 0 - выкл
float g_op22_ads_arc = 1.f; // OP22 (27.11): прицеливание по дуге и досыл, 0 - выкл
// OP22 (27.11, 3.251): живость без смены анимаций - походка от шагов, мягкие переходы поз, крен при повороте, точка опоры
float g_op22_hud_gait = 1.f; // походка от шагов вместо синусоиды покачивания оружия, 0 - прежнее покачивание
float g_op22_hud_soft = 1.f; // мягкие (пружинные) переходы стрейфа/наклона/прыжка/приседа/ходьбы/стены, 0 - прежние
float g_op22_hud_bank = 1.f; // крен оружия в сторону поворота мышью, 0 - выкл
float g_op22_hud_pivot = 1.f; // повороты рук вокруг плеча/кисти, а не глаза, 0 - вокруг глаза

// OP22: процедурная отдача и «дыхание» модели оружия в руках (22.11, решение пользователя; отключаемо).
// Только вид модели худа: камера, разброс и пули - как в ОП (отдача камеры on_weapon_shot_start не меняется).
float g_op22_wpn_recoil = 1.f; // множитель отдачи модели, 0 - выкл (op22_console.cpp)
float g_op22_wpn_breath = 1.f; // множитель «дыхания» модели, 0 - выкл

namespace
{
constexpr float OP22_REC_W = 13.f; // собственная частота пружины возврата, рад/с (период ~0,5 с)
constexpr float OP22_REC_K = OP22_REC_W * OP22_REC_W;
constexpr float OP22_REC_C = 2.f * 0.55f * OP22_REC_W; // затухание 0,55: лёгкий перелёт

void op22_spring(Fvector& x, Fvector& v, float dt)
{
    // полунеявный Эйлер с подшагами по 8 мс - устойчиво и при низком FPS
    const int n = _max(1, iCeil(dt / 0.008f));
    const float h = dt / n;
    for (int i = 0; i < n; ++i)
    {
        Fvector a;
        a.set(x).mul(-OP22_REC_K);
        a.mad(v, -OP22_REC_C);
        v.mad(a, h);
        x.mad(v, h);
    }
}
} // namespace

namespace
{
IC float op22_smooth01(float x)
{
    clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

// плавный одномерный градиентный шум (-1..1): у каждой оси свой seed - движения не повторяются
IC float op22_grad(int i, int seed)
{
    u32 hsh = u32(i) * 374761393u + u32(seed) * 668265263u;
    hsh = (hsh ^ (hsh >> 13)) * 1274126177u;
    return float(hsh & 0xffff) / 32767.5f - 1.f;
}

IC float op22_noise(float x, int seed)
{
    const int i = iFloor(x);
    const float f = x - float(i);
    const float u = f * f * f * (f * (f * 6.f - 15.f) + 10.f);
    const float n0 = op22_grad(i, seed) * f, n1 = op22_grad(i + 1, seed) * (f - 1.f);
    return 2.f * (n0 + (n1 - n0) * u); // ~ -1..1
}
} // namespace

void CHudItem::op22_OnHudAnim(const char* anm)
{
    if (anm)
    {
        xr_strcpy(m_op22_ev_anm, anm); // трасса
        m_op22_ev |= 2;
    }
    // anm = nullptr - покой/прицел/ходьба: поза возвращается
    if (!anm || g_op22_hud_variety <= 0.f)
    {
        m_op22_var_tpos.set(0.f, 0.f, 0.f);
        m_op22_var_trot.set(0.f, 0.f, 0.f);
        return;
    }
    const float v = _min(g_op22_hud_variety, 2.f);
    m_op22_var_tpos.set(::Random.randFs(0.004f), ::Random.randFs(0.003f), ::Random.randFs(0.005f)).mul(v);
    m_op22_var_trot.set(deg2rad(::Random.randFs(1.2f)), deg2rad(::Random.randFs(1.0f)), deg2rad(::Random.randFs(2.0f))).mul(v);
}

void CHudItem::op22_HudStep(int leg, float power)
{
    m_op22_ev |= 1;
    u32 ms = 0;
    if (const CActor* pA = Actor())
        ms = pA->MovingState();

    // OP22 (27.11, 3.251): походка идёт от настоящих шагов - фаза сбрасывается на ударе пятки, период - по интервалу
    // между шагами, сила каждого шага своя (люди не шагают одинаково)
    {
        const float t = Device.fTimeGlobal, d = t - m_op22_gait_last;
        if (d > 0.18f && d < 1.2f)
            m_op22_gait_period = m_op22_gait_period * 0.5f + d * 0.5f;
        m_op22_gait_last = t;
        m_op22_gait_phase = 0.f;
        m_op22_gait_side = (leg & 1) ? 1.f : -1.f;
        m_op22_gait_amp = 1.f + ::Random.randFs(0.15f);
        ++m_op22_gait_n;
        // дыхание на бегу ложится на шаг (вдох - на каждый второй шаг), если дыхание сбито
        if (g_op22_body_state > 0.f && (m_op22_gait_n & 1) == 0)
        {
            const float c = _min(1.f, m_op22_excite * g_op22_body_state + ((ms & mcSprint) ? 0.6f : 0.f));
            const float ph = m_op22_breath_phase > 0.5f ? m_op22_breath_phase - 1.f : m_op22_breath_phase;
            m_op22_breath_phase -= ph * 0.3f * c;
            if (m_op22_breath_phase < 0.f)
                m_op22_breath_phase += 1.f;
        }
    }

    if (g_op22_hud_steps <= 0.f)
        return;
    float s = _min(power, 1.f) * g_op22_hud_steps * (g_op22_hud_gait > 0.f ? 0.5f : 1.f); // с походкой - только «вес» удара
    if (ms & mcSprint)
        s *= 1.35f;
    else if (ms & mcCrouch)
        s *= 0.5f;
    if (IsZoomed())
        s *= 0.3f;
    const float side = (leg & 1) ? 1.f : -1.f; // опорная нога: правая/левая
    m_op22_rec_pos_v.y -= 0.045f * s; // руки чуть «проседают» на опору
    m_op22_rec_pos_v.x += 0.010f * s * side;
    m_op22_rec_rot_v.z += deg2rad(0.9f) * s * side;
    m_op22_rec_rot_v.x += deg2rad(0.35f) * s; // ствол на миг вниз
}

void CHudItem::op22_HudHitKick(const Fvector& dir, float power)
{
    m_op22_ev |= 4;
    if (g_op22_hud_hit <= 0.f || power <= 0.f)
        return;
    // направление удара в осях камеры: руки и ствол уводит по удару
    const float x = dir.dotproduct(Device.vCameraRight), y = dir.dotproduct(Device.vCameraTop), z = dir.dotproduct(Device.vCameraDirection);
    float s = _min(0.25f + power * 1.5f, 1.2f) * g_op22_hud_hit * (IsZoomed() ? 0.5f : 1.f);
    m_op22_rec_pos_v.x += 0.06f * x * s;
    m_op22_rec_pos_v.y += 0.04f * y * s - 0.02f * s;
    m_op22_rec_pos_v.z += 0.06f * z * s;
    m_op22_rec_rot_v.y += deg2rad(6.f) * x * s;
    m_op22_rec_rot_v.x += deg2rad(4.f) * s; // ствол сбивает вниз
    m_op22_rec_rot_v.z += deg2rad(::Random.randFs(6.f)) * s;
}

void CHudItem::op22_HudRecoilKick(float strength)
{
    m_op22_ev |= 8;
    if (g_op22_wpn_recoil <= 0.f || strength <= 0.f)
        return;
    // OP22 (27.11): очередь - первый выстрел резче (стойка «ломается»), дальше плечо устаёт и оседает (m_op22_sag)
    const float burst = m_op22_burst;
    m_op22_burst += 1.f;
    m_op22_sag += 1.f;
    strength *= burst < 0.5f ? 1.12f : 1.f + 0.05f * _min(burst - 1.f, 8.f);
    // в прицеливании слабее, чтобы прицел не уходил из глаз
    const float s = strength * g_op22_wpn_recoil * (IsZoomed() ? 0.35f : 1.f) * OP22_REC_W * 1.6f; // пик ~ величине толчка
    // смещение, м (оси худа: x вправо, y вверх, z вперёд): назад в плечо, чуть вверх и вбок
    m_op22_rec_pos_v.z -= 0.012f * s;
    m_op22_rec_pos_v.y += 0.002f * s;
    m_op22_rec_pos_v.x += ::Random.randFs(0.0015f) * s;
    // поворот, рад (rotateX: минус - ствол вверх), случайно вбок и с завалом
    m_op22_rec_rot_v.x -= deg2rad(1.2f) * s;
    m_op22_rec_rot_v.y += deg2rad(::Random.randFs(0.45f)) * s;
    m_op22_rec_rot_v.z += deg2rad(::Random.randFs(0.9f)) * s;
}

void CHudItem::op22_UpdateProcedural(Fvector& offs, Fvector& rot, bool aiming, u32 moving_state)
{
    float dt = Device.fTimeDelta;
    clamp(dt, 0.f, 0.05f);

    op22_spring(m_op22_rec_pos, m_op22_rec_pos_v, dt);
    op22_spring(m_op22_rec_rot, m_op22_rec_rot_v, dt);
    // предел на длинную очередь из тяжёлого ствола
    clamp(m_op22_rec_pos.x, -0.03f, 0.03f);
    clamp(m_op22_rec_pos.y, -0.03f, 0.03f);
    clamp(m_op22_rec_pos.z, -0.08f, 0.03f);
    const float rmax = deg2rad(10.f);
    clamp(m_op22_rec_rot.x, -rmax, rmax);
    clamp(m_op22_rec_rot.y, -rmax, rmax);
    clamp(m_op22_rec_rot.z, -rmax, rmax);
    offs.add(m_op22_rec_pos);
    rot.add(m_op22_rec_rot);

    // OP22 (27.11): очередь - плечо оседает с каждым выстрелом и отпускает после паузы
    m_op22_burst *= std::exp(-dt / 0.35f);
    m_op22_sag *= std::exp(-dt / 0.7f);
    if (g_op22_wpn_recoil > 0.f)
    {
        const float sag = _min(m_op22_sag, 8.f) * g_op22_wpn_recoil * (aiming ? 0.4f : 1.f);
        offs.y -= 0.0011f * sag;
        offs.z -= 0.0014f * sag;
        rot.x += deg2rad(0.10f) * sag; // ствол опускается
    }

    // OP22 (27.11): прицеливание по дуге - на середине перехода ствол уходит чуть вниз и в сторону с завалом, в конце
    // досылается в плечо; начало и конец перехода - как прежде (дуга на краях равна нулю)
    if (g_op22_ads_arc > 0.f)
    {
        const float zf = m_fZoomRotationFactor;
        const float arc = _sin(PI * zf) * g_op22_ads_arc;
        offs.y -= 0.010f * arc;
        offs.x -= 0.004f * arc;
        rot.z += deg2rad(3.f) * arc;
        if (m_op22_prev_zoom < 0.999f && zf >= 0.999f)
        {
            m_op22_rec_pos_v.z -= 0.05f * g_op22_ads_arc; // досыл в плечо
            m_op22_rec_rot_v.x -= deg2rad(0.6f) * g_op22_ads_arc;
        }
        m_op22_prev_zoom = zf;
    }

    // OP22 (27.11, 3.251): крен в сторону поворота мышью (как оружие в руке при резком довороте корпуса) и «усилие» руки:
    // после резкого поворота/остановки тремор на миг сильнее
    {
        const Fvector& d = Device.vCameraDirection;
        float wy = 0.f;
        if (dt > EPS_L && m_op22_prev_dir.square_magnitude() > 0.5f)
        {
            Fvector cr;
            cr.crossproduct(m_op22_prev_dir, d);
            wy = cr.dotproduct(Device.vCameraTop) / dt; // рад/с, + вправо
            clamp(wy, -12.f, 12.f);
        }
        m_op22_prev_dir.set(d);
        if (dt > EPS_L)
        {
            m_op22_turn += (wy - m_op22_turn) * (1.f - std::exp(-dt / 0.05f));
            float tgt = 0.f;
            if (g_op22_hud_bank > 0.f)
            {
                tgt = -m_op22_turn * 0.016f * g_op22_hud_bank * (aiming ? 0.3f : 1.f);
                clamp(tgt, -deg2rad(4.f), deg2rad(4.f));
            }
            const float w = PI_MUL_2 * 2.6f, kk = w * w, c = 2.f * 0.7f * w;
            const int n = _max(1, iCeil(dt / 0.004f));
            const float h = dt / n;
            for (int i = 0; i < n; ++i)
            {
                m_op22_bank_v += (kk * (tgt - m_op22_bank) - c * m_op22_bank_v) * h;
                m_op22_bank += m_op22_bank_v * h;
            }
            const float e = _min(1.f, _abs(m_op22_turn) / 5.f);
            if (e > m_op22_effort)
                m_op22_effort += (e - m_op22_effort) * (1.f - std::exp(-dt / 0.15f));
            else
                m_op22_effort *= std::exp(-dt / 1.2f);
        }
        rot.z += m_op22_bank;
    }

    // OP22 (27.11): живое «дыхание» вместо фигуры Лиссажу - дыхательный цикл с паузой, медленный дрейф и тремор плавным
    // шумом (без регулярности, которую ловит глаз); сила - от состояния тела (op22_body_state): сбитое дыхание после
    // бега, усталость, здоровье, перегруз. Только вид модели: меткость и разброс - как в ОП.
    // 3.251: всё из одной модели тела - присед устойчивее, рука в прицеле устаёт, при вскидке дыхание задерживается,
    // блуждание руки гасится прерывистыми поправками (как рука ловит цель), а не плавно.
    if (g_op22_wpn_breath > 0.f && smart_cast<CWeapon*>(this))
    {
        m_op22_breath_t += dt;
        const float t = m_op22_breath_t;
        const float bs = g_op22_body_state;
        float fatigue = 0.f, wound = 0.f;
        if (CActor* pA = Actor())
        {
            if (moving_state & mcSprint)
                m_op22_excite += dt / 4.f; // ~4 с бега - дыхание сбито полностью
            else
                m_op22_excite -= m_op22_excite * (1.f - std::exp(-dt / 10.f)); // успокаивается за ~10 с
            clamp(m_op22_excite, 0.f, 1.f);
            fatigue = (1.f - pA->conditions().GetPower()) * bs;
            const float load = pA->inventory().TotalWeight() / _max(1.f, pA->MaxCarryWeight());
            if (load > 1.f)
                fatigue += _min(1.f, load - 1.f) * 0.5f * bs;
            wound = _sqr(1.f - pA->conditions().GetHealth()) * bs;
        }
        const float excite = m_op22_excite * bs;
        float k = g_op22_wpn_breath * (aiming ? 0.5f : 1.f);
        if (moving_state & mcAnyMove)
            k *= 0.5f;

        // тело: присед устойчивее; рука, долго держащая прицел, устаёт (~12 с), отдыхает за ~4 с
        float kc = 1.f;
        if (bs > 0.f)
        {
            if (moving_state & mcCrouch)
                kc = (moving_state & mcAccel) ? 0.6f : 0.75f;
            if (aiming)
                m_op22_ads_hold = _min(1.f, m_op22_ads_hold + dt / 12.f);
            else
                m_op22_ads_hold = _max(0.f, m_op22_ads_hold - dt / 4.f);
            // вскидка: на 1-2,5 с дыхание задерживается (после бега - короче), затем мягко возвращается
            const bool full = m_fZoomRotationFactor >= 0.999f;
            if (full && !m_op22_full_prev)
                m_op22_breath_hold_t = (2.5f - 1.5f * _min(excite, 1.f)) * (1.f + ::Random.randFs(0.2f));
            if (!aiming)
                m_op22_breath_hold_t = 0.f;
            m_op22_full_prev = full;
            m_op22_breath_hold_t = _max(0.f, m_op22_breath_hold_t - dt);
        }
        else
        {
            m_op22_ads_hold = 0.f;
            m_op22_breath_hold_t = 0.f;
        }
        const float bm_t = m_op22_breath_hold_t > 0.f ? 0.3f : 1.f;
        m_op22_breath_mul += (bm_t - m_op22_breath_mul) * (1.f - std::exp(-dt / (bm_t < m_op22_breath_mul ? 0.35f : 0.9f)));
        const float hold = 1.f + 0.7f * m_op22_ads_hold * bs;

        // дыхание: вдох ~40 % (ствол чуть поднимается), выдох ~45 %, пауза ~15 %; период плавает от вдоха к вдоху
        m_op22_breath_phase += dt / m_op22_breath_period * (0.25f + 0.75f * m_op22_breath_mul);
        if (m_op22_breath_phase >= 1.f)
        {
            m_op22_breath_phase -= 1.f;
            m_op22_breath_period = (4.3f - 2.6f * excite) * (1.f + ::Random.randFs(0.12f));
        }
        const float ph = m_op22_breath_phase;
        const float br = ph < 0.4f ? op22_smooth01(ph / 0.4f) : (ph < 0.85f ? 1.f - op22_smooth01((ph - 0.4f) / 0.45f) : 0.f);
        const float ab = k * (1.f + 1.8f * excite + 0.8f * fatigue) * m_op22_breath_mul;
        offs.y += 0.0016f * ab * br;
        rot.x -= deg2rad(0.22f) * ab * br; // минус - ствол вверх

        // дрейф: медленное блуждание рук (шум, не синус)
        const float ad = k * (1.f + 1.5f * fatigue + 0.6f * excite) * kc * hold;
        const float motor = _min(_max(g_op22_hud_motor, 0.f), 2.f);
        const float dscale = 1.f + 1.2f * _min(motor, 1.f); // поправки гасят часть блуждания - видимый размах как прежде
        float D[4] = {0.0010f * ad * dscale * op22_noise(t * 0.33f, 11), 0.0007f * ad * dscale * op22_noise(t * 0.29f, 23),
                      deg2rad(0.20f) * ad * dscale * op22_noise(t * 0.31f, 37), deg2rad(0.14f) * ad * dscale * op22_noise(t * 0.27f, 41)};
        // моторика: рука не держит ствол идеально ровно и не плывёт плавно - она копит ошибку и раз в 0,3-0,6 с
        // (в прицеле - чаще и точнее) делает короткую поправку с колоколом скорости (профиль минимального рывка)
        if (motor > 0.f)
        {
            if (m_op22_mc_dur > 0.f)
            {
                m_op22_mc_t += dt;
                float u = m_op22_mc_t / m_op22_mc_dur;
                clamp(u, 0.f, 1.f);
                const float sj = u * u * u * (10.f + u * (-15.f + 6.f * u));
                for (int i = 0; i < 4; ++i)
                    m_op22_mc_c[i] = m_op22_mc_c0[i] + (m_op22_mc_c1[i] - m_op22_mc_c0[i]) * sj;
                if (u >= 1.f)
                    m_op22_mc_dur = 0.f;
            }
            m_op22_mc_next -= dt;
            if (m_op22_mc_next <= 0.f && m_op22_mc_dur <= 0.f)
            {
                const float g = aiming ? ::Random.randF(0.85f, 1.f) : ::Random.randF(0.6f, 0.9f); // недоводка, как у руки
                for (int i = 0; i < 4; ++i)
                {
                    m_op22_mc_c0[i] = m_op22_mc_c[i];
                    m_op22_mc_c1[i] = D[i] * g * _min(motor, 1.f);
                }
                m_op22_mc_t = 0.f;
                m_op22_mc_dur = ::Random.randF(0.16f, 0.26f);
                m_op22_mc_next = (aiming ? ::Random.randF(0.2f, 0.35f) : ::Random.randF(0.3f, 0.6f)) / _max(motor, 0.5f);
            }
        }
        else
        {
            for (int i = 0; i < 4; ++i)
                m_op22_mc_c[i] = 0.f;
            m_op22_mc_dur = 0.f;
        }
        offs.x += D[0] - m_op22_mc_c[0];
        offs.y += D[1] - m_op22_mc_c[1];
        rot.y += D[2] - m_op22_mc_c[2];
        rot.x += D[3] - m_op22_mc_c[3];

        // тремор ~8-10 Гц: у отдохнувшего почти незаметен, растёт с усталостью, ранами, после бега и резкого поворота
        const float at = g_op22_wpn_breath * (0.15f + 1.1f * fatigue + 1.6f * wound + 0.7f * excite + 0.5f * m_op22_effort * bs) * kc *
            (1.f + 0.8f * m_op22_ads_hold * bs);
        rot.x += deg2rad(0.035f) * at * op22_noise(t * 8.7f, 53);
        rot.y += deg2rad(0.035f) * at * op22_noise(t * 9.6f, 67);
    }

    // OP22 (27.11): манера действия - у каждой перезарядки/доставания своя небольшая поза рук (op22_OnHudAnim), мягко
    // входит и выходит; время анимаций не меняется
    {
        const float w = PI_MUL_2 * 2.2f, kk = w * w, c = 2.f * 0.9f * w;
        const int n = _max(1, iCeil(dt / 0.004f));
        const float h = dt / n;
        for (int s = 0; s < n; ++s)
        {
            Fvector a;
            a.sub(m_op22_var_tpos, m_op22_var_pos).mul(kk).mad(m_op22_var_pos_v, -c);
            m_op22_var_pos_v.mad(a, h);
            m_op22_var_pos.mad(m_op22_var_pos_v, h);
            a.sub(m_op22_var_trot, m_op22_var_rot).mul(kk).mad(m_op22_var_rot_v, -c);
            m_op22_var_rot_v.mad(a, h);
            m_op22_var_rot.mad(m_op22_var_rot_v, h);
        }
        const float vk = aiming ? 0.3f : 1.f;
        offs.mad(m_op22_var_pos, vk);
        rot.mad(m_op22_var_rot, vk);
    }

    // OP22 23.11: подавление огнём - частая дрожь модели оружия (видимая, меткость не меняется; op22_suppression.h)
    if (g_op22_suppress > 0.f && g_op22_suppress_cfg.sway > 0.f && smart_cast<CWeapon*>(this))
    {
        const float s = g_op22_suppress * g_op22_suppress_cfg.sway * (aiming ? 0.6f : 1.f);
        const float tg = Device.fTimeGlobal;
        offs.x += 0.0010f * s * _sin(tg * PI_MUL_2 * 2.3f);
        offs.y += 0.0009f * s * _sin(tg * PI_MUL_2 * 3.1f + 0.7f);
        rot.x += deg2rad(0.35f) * s * _sin(tg * PI_MUL_2 * 2.7f + 1.3f);
        rot.y += deg2rad(0.30f) * s * _sin(tg * PI_MUL_2 * 1.9f + 0.4f);
    }

    // OP22 (27.11, 3.251): все эти повороты рука делает вокруг опоры - приклада в плече или кисти, а не вокруг глаза:
    // дульный срез ходит, а задняя часть почти стоит; смещение, которое даёт поворот вокруг опоры, добавляется к offs
    if (g_op22_hud_pivot > 0.f && rot.square_magnitude() > 1e-12f)
    {
        Fvector P;
        P.lerp(m_op22_pivot[0], m_op22_pivot[1], m_fZoomRotationFactor);
        P.mul(_min(g_op22_hud_pivot, 2.f));
        Fmatrix R, Rp;
        R.identity();
        R.rotateX(rot.x);
        Rp.identity();
        Rp.rotateY(rot.y);
        R.mulA_43(Rp);
        Rp.identity();
        Rp.rotateZ(rot.z);
        R.mulA_43(Rp);
        Fvector RP;
        R.transform_dir(RP, P);
        offs.add(P).sub(RP);
    }
}

// OP22 (27.11, 3.251): мягкий переход позы - пружина почти без перелёта (zeta) вместо линейной подтяжки: разгон и
// торможение; время - как у прежнего перехода (T), op22_hud_soft больше - мягче
void CHudItem::op22_Ease(Fvector& cur, Fvector& vel, const Fvector& tgt, float T, float zeta)
{
    float dt = Device.fTimeDelta;
    clamp(dt, 0.f, 0.05f);
    const float w = 2.15f / _max(T * g_op22_hud_soft, 0.02f), kk = w * w, c = 2.f * zeta * w;
    const int n = _max(1, iCeil(dt / 0.004f));
    const float h = dt / n;
    for (int i = 0; i < n; ++i)
    {
        Fvector a;
        a.sub(tgt, cur).mul(kk).mad(vel, -c);
        vel.mad(a, h);
        cur.mad(vel, h);
    }
}

void CHudItem::UpdateInertion(Fmatrix& trans)
{
    if (HudInertionEnabled() && HudInertionAllowed())
    {
        Fmatrix xform;
        Fvector& origin = trans.c;
        xform = trans;

        // calc difference
        Fvector diff_dir;
        diff_dir.sub(xform.k, inert_st_last_dir);

        // clamp by PI_DIV_2
        Fvector last;
        last.normalize_safe(inert_st_last_dir);
        const float dot = last.dotproduct(xform.k);
        if (dot < EPS)
        {
            Fvector v0;
            v0.crossproduct(inert_st_last_dir, xform.k);
            inert_st_last_dir.crossproduct(xform.k, v0);
            diff_dir.sub(xform.k, inert_st_last_dir);
            op22_inert_vel.set(0.f, 0.f, 0.f); // OP22
        }

        // tend to forward
        float _tendto_speed, _origin_offset;
        if (GetCurrentHudOffsetIdx() > 0)
        { // Худ в режиме "Прицеливание"
            float factor = GetInertionFactor();
            _tendto_speed = inertion_data.m_tendto_speed_aim - (inertion_data.m_tendto_speed_aim - inertion_data.m_tendto_speed) * factor;
            _origin_offset = inertion_data.m_origin_offset_aim - (inertion_data.m_origin_offset_aim - inertion_data.m_origin_offset) * factor;
        }
        else
        { // Худ в режиме "От бедра"
            _tendto_speed = inertion_data.m_tendto_speed;
            _origin_offset = inertion_data.m_origin_offset;
        }

        // Фактор силы инерции
        const float power_factor = GetInertionPowerFactor();
        _tendto_speed *= power_factor;
        // OP22: настройка «Инерция рук», проценты; скорость возврата не трогаем, иначе при 0 направление копилось бы
        _origin_offset *= power_factor * g_op22_hud_inertion * 0.01f;

        if (g_op22_hud_inertion_spring > 0.f)
        {
            // OP22 (27.11): инерция 2-го порядка - ствол догоняет взгляд пружиной (затухание 0,72) и при резкой остановке
            // поворота чуть перелетает и возвращается, как оружие в руке; время догона - как у прежней (m_tendto_speed)
            const float w = _tendto_speed * 1.6f / g_op22_hud_inertion_spring, k = w * w, c = 2.f * (0.72f + 0.08f * _min(_max(g_op22_hud_motor, 0.f), 1.f)) * w;
            float dt = Device.fTimeDelta;
            clamp(dt, 0.f, 0.05f);
            const int n = _max(1, iCeil(dt / 0.004f));
            const float h = dt / n;
            for (int s = 0; s < n; ++s)
            {
                Fvector a;
                a.sub(xform.k, inert_st_last_dir).mul(k);
                a.mad(op22_inert_vel, -c);
                op22_inert_vel.mad(a, h);
                inert_st_last_dir.mad(op22_inert_vel, h);
            }
        }
        else
        {
            op22_inert_vel.set(0.f, 0.f, 0.f);
            inert_st_last_dir.mad(diff_dir, _tendto_speed * Device.fTimeDelta);
        }
        origin.mad(diff_dir, -_origin_offset); // Инвертировал

        // pitch compensation
        float pitch = angle_normalize_signed(xform.k.getP());

        pitch *= GetInertionFactor();

        // Отдаление\приближение
        origin.mad(xform.k, -pitch * inertion_data.m_pitch_offset_d);

        // Сдвиг в противоположную часть экрана
        origin.mad(xform.i, -pitch * inertion_data.m_pitch_offset_r);

        // Подьём\опускание
        clamp(pitch, inertion_data.m_pitch_low_limit, PI);
        origin.mad(xform.j, -pitch * inertion_data.m_pitch_offset_n);
    }
}

// Обновление координат текущего худа
void CHudItem::UpdateHudAdditional(Fmatrix& trans, const bool need_update_collision)
{
    if (!need_update_collision)
        return;

    UpdateInertion(trans);

    Fvector summary_offset{}, summary_rotate{};

    attachable_hud_item* hi = HudItemData();
    u8 idx = GetCurrentHudOffsetIdx();
    const bool b_aiming = idx != hud_item_measures::m_hands_offset_type_normal;
    Fvector zr_offs = hi->m_measures.m_hands_offset[hud_item_measures::m_hands_offset_pos][idx];
    Fvector zr_rot = hi->m_measures.m_hands_offset[hud_item_measures::m_hands_offset_rot][idx];

    //============= Поворот ствола во время аима =============//
    {
        const float factor = Device.fTimeDelta / m_fZoomRotateTime;

        // I AM DEAD - Dirty hack from delayed exit from aim
        if (IsZoomed())
            m_fZoomRotationFactor += factor * 2.f;
        else
            m_fZoomRotationFactor -= factor * 2.f;

        clamp(m_fZoomRotationFactor, 0.f, 1.f);

        if (!zr_offs.similar(current_difference[0], EPS))
            current_difference[0].lerp(current_difference[0], zr_offs, factor * 2.f);

        if (!zr_rot.similar(current_difference[1], EPS))
            current_difference[1].lerp(current_difference[1], zr_rot, factor * 2.f);

        summary_offset.add(current_difference[0]);
    }
    //====================================================//

    auto pActor = smart_cast<const CActor*>(object().H_Parent());
    const u32 iMovingState = pActor->MovingState();
    idx = b_aiming ? 1ui8 : 0ui8;

    //============= Боковой стрейф с оружием =============//
    {
        const bool bEnabled = m_strafe_offset[2][idx].x;
        if (bEnabled)
        {
            float fStrafeMaxTime = m_strafe_offset[2][idx].y; // Макс. время в секундах, за которое произойдет смещение худа при стрейфах

            if (fStrafeMaxTime <= EPS)
                fStrafeMaxTime = 0.01f;

            const float fStepPerUpd = Device.fTimeDelta / fStrafeMaxTime; // Величина изменение фактора смещения худа при стрейфах

            Fvector current_moving_offs{}, current_moving_rot{};

            if (iMovingState & mcLStrafe) // Двигаемся влево
            {
                current_moving_offs.set(-m_strafe_offset[0][idx]);
                current_moving_rot.set(-m_strafe_offset[1][idx]);
            }
            else if (iMovingState & mcRStrafe) // Двигаемся вправо
            {
                current_moving_offs.set(m_strafe_offset[0][idx]);
                current_moving_rot.set(m_strafe_offset[1][idx]);
            }
            else // Двигаемся в любом другом направлении
            {
                current_moving_offs.set(Fvector{});
                current_moving_rot.set(Fvector{});
            }

            current_moving_rot.mul(-PI / 180.f); // Преобразуем углы в радианы

            if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251)
            {
                op22_Ease(current_strafe[0], m_op22_soft_v[0][0], current_moving_offs, fStrafeMaxTime, 0.8f);
                op22_Ease(current_strafe[1], m_op22_soft_v[0][1], current_moving_rot, fStrafeMaxTime, 0.8f);
            }
            else
            {
                m_op22_soft_v[0][0].set(0.f, 0.f, 0.f);
                m_op22_soft_v[0][1].set(0.f, 0.f, 0.f);
                if (!current_moving_offs.similar(current_strafe[0], EPS))
                    current_strafe[0].lerp(current_strafe[0], current_moving_offs, fStepPerUpd);
                if (!current_moving_rot.similar(current_strafe[1], EPS))
                    current_strafe[1].lerp(current_strafe[1], current_moving_rot, fStepPerUpd);
            }

            summary_offset.add(current_strafe[0]);
            summary_rotate.add(current_strafe[1]);
        }
    }

    //=============== Эффекты прыжка ===============//
    {
        const bool bEnabled = m_jump_offset[2][idx].x;
        if (bEnabled)
        {
            float fJumpMaxTime = m_jump_offset[2][idx].y; // Макс. время в секундах, за которое произойдет смещение худа при прыжке

            if (fJumpMaxTime <= EPS)
                fJumpMaxTime = 0.01f;

            const float fStepPerUpd = Device.fTimeDelta / fJumpMaxTime; // Величина изменение фактора смещения худа при прыжке

            Fvector current_jump_offs{}, current_jump_rot{};

            if (iMovingState & mcJump) // Прыжок
            {
                current_jump_offs.set(m_jump_offset[0][idx]);
                current_jump_rot.set(m_jump_offset[1][idx]);
            }
            else if (iMovingState & mcFall) // Полет
            {
                current_jump_offs.set(m_fall_offset[0][idx]);
                current_jump_rot.set(m_fall_offset[1][idx]);
            }
            else if (iMovingState & mcLanding || iMovingState & mcLanding2) // Полет
            {
                current_jump_offs.set(m_landing_offset[0][idx]);
                current_jump_rot.set(m_landing_offset[1][idx]);
            }
            else
            {
                current_jump_offs.set(Fvector{});
                current_jump_rot.set(Fvector{});
            }

			float koef = iMovingState & mcLanding2 ? 1.3 : 1.0;
            current_jump_offs.mul(koef);
            current_jump_rot.mul(koef);
            current_jump_rot.mul(-PI / 180.f); // Преобразуем углы в радианы

            if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251)
            {
                op22_Ease(current_jump[0], m_op22_soft_v[1][0], current_jump_offs, fJumpMaxTime, 0.6f);
                op22_Ease(current_jump[1], m_op22_soft_v[1][1], current_jump_rot, fJumpMaxTime, 0.6f);
            }
            else
            {
                m_op22_soft_v[1][0].set(0.f, 0.f, 0.f);
                m_op22_soft_v[1][1].set(0.f, 0.f, 0.f);
                if (!current_jump_offs.similar(current_jump[0], EPS))
                    current_jump[0].lerp(current_jump[0], current_jump_offs, fStepPerUpd);
                if (!current_jump_rot.similar(current_jump[1], EPS))
                    current_jump[1].lerp(current_jump[1], current_jump_rot, fStepPerUpd);
            }

            summary_offset.add(current_jump[0]);
            summary_rotate.add(current_jump[1]);
        }
    }

    //=============== Эффекты наклонов ===================//
    {
        const bool bEnabled = m_lookout_offset[2][idx].x;
        if (bEnabled)
        {
            float fLookoutMaxTime = m_lookout_offset[2][idx].y; // Макс. время в секундах, за которое мы наклонимся из центрального положения
            if (fLookoutMaxTime <= EPS)
                fLookoutMaxTime = 0.01f;

            const float fStepPerUpd = Device.fTimeDelta / fLookoutMaxTime; // Величина изменение фактора поворота

            float koef{1.f};
            if ((iMovingState & mcCrouch) && (iMovingState & mcAccel))
                koef = 0.5; // во сколько раз менять амплитуду при полном присяде
            else if (iMovingState & mcCrouch)
                koef = 0.75; // во сколько раз менять амплитуду при присяде

            Fvector current_lookout_offs{}, current_lookout_rot{};

            if ((iMovingState & mcLLookout) && !(iMovingState & mcRLookout)) // Выглядываем влево
            {
                current_lookout_offs.set(-m_lookout_offset[0][idx]);
                current_lookout_rot.set(-m_lookout_offset[1][idx]);
            }
            else if ((iMovingState & mcRLookout) && !(iMovingState & mcLLookout)) // Выглядываем вправо
            {
                current_lookout_offs.set(m_lookout_offset[0][idx]);
                current_lookout_rot.set(m_lookout_offset[1][idx]);
            }
            else
            {
                current_lookout_offs.set(Fvector{});
                current_lookout_rot.set(Fvector{});
            }

            current_lookout_offs.mul(koef);
            current_lookout_rot.mul(koef);
            current_lookout_rot.mul(-PI / 180.f); // Преобразуем углы в радианы

            if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251)
            {
                op22_Ease(current_lookout[0], m_op22_soft_v[2][0], current_lookout_offs, fLookoutMaxTime, 0.85f);
                op22_Ease(current_lookout[1], m_op22_soft_v[2][1], current_lookout_rot, fLookoutMaxTime, 0.85f);
            }
            else
            {
                m_op22_soft_v[2][0].set(0.f, 0.f, 0.f);
                m_op22_soft_v[2][1].set(0.f, 0.f, 0.f);
                if (!current_lookout_offs.similar(current_lookout[0], EPS))
                    current_lookout[0].lerp(current_lookout[0], current_lookout_offs, fStepPerUpd);
                if (!current_lookout_rot.similar(current_lookout[1], EPS))
                    current_lookout[1].lerp(current_lookout[1], current_lookout_rot, fStepPerUpd);
            }

            summary_offset.add(current_lookout[0]);
            summary_rotate.add(current_lookout[1]);
        }
    }

    //=============== Эффекты стойки ===================//
    {
        const bool bEnabled = m_move_offset[2].x;
        if (bEnabled)
        {
            float fMoveMaxTime = m_move_offset[2].y; // Макс. время в секундах, за которое мы наклонимся из центрального положения
            if (fMoveMaxTime <= EPS)
                fMoveMaxTime = 0.01f;

            const float fStepPerUpd = Device.fTimeDelta / fMoveMaxTime; // Величина изменение фактора поворота

            float koef{};
            if ((iMovingState & mcCrouch) && (iMovingState & mcAccel))
                koef = 1.0; // во сколько раз менять амплитуду при полном присяде
            else if (iMovingState & mcCrouch)
                koef = 0.5; // во сколько раз менять амплитуду при присяде

            Fvector current_move_offs{}, current_move_rot{};

            if (iMovingState & mcCrouch) // Выглядываем влево
            {
                current_move_offs.set(m_move_offset[0]);
                current_move_rot.set(m_move_offset[1]);
            }
            else
            {
                current_move_offs.set(Fvector{});
                current_move_rot.set(Fvector{});
            }

            auto pda = smart_cast<CPda*>(this);
            auto missile = smart_cast<CMissile*>(this);

            current_move_offs.mul(koef);
            current_move_rot.mul(koef);
            current_move_offs.mul(!IsZoomed());
            current_move_rot.mul(!IsZoomed());
            current_move_offs.mul(!pda && !missile);
            current_move_rot.mul(!pda && !missile);
            current_move_rot.mul(-PI / 180.f); // Преобразуем углы в радианы

            if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251)
            {
                op22_Ease(current_move[0], m_op22_soft_v[3][0], current_move_offs, fMoveMaxTime, 0.9f);
                op22_Ease(current_move[1], m_op22_soft_v[3][1], current_move_rot, fMoveMaxTime, 0.9f);
            }
            else
            {
                m_op22_soft_v[3][0].set(0.f, 0.f, 0.f);
                m_op22_soft_v[3][1].set(0.f, 0.f, 0.f);
                if (!current_move_offs.similar(current_move[0], EPS))
                    current_move[0].lerp(current_move[0], current_move_offs, fStepPerUpd);
                if (!current_move_rot.similar(current_move[1], EPS))
                    current_move[1].lerp(current_move[1], current_move_rot, fStepPerUpd);
            }

            summary_offset.add(current_move[0]);
            summary_rotate.add(current_move[1]);
        }
    }

    //=============== Эффекты ходьбы ===================//
    {
        const bool bEnabled = m_walk_offset[2].x;
        if (bEnabled)
        {
            float fWalkMaxTime = m_walk_offset[2].y; // Макс. время в секундах, за которое мы наклонимся из центрального положения
            if (fWalkMaxTime <= EPS)
                fWalkMaxTime = 0.01f;

            const float fStepPerUpd = Device.fTimeDelta / fWalkMaxTime; // Величина изменение фактора поворота

            float koef{1.f};
            if ((iMovingState & mcCrouch) && (iMovingState & mcAccel))
                koef = 0.5; // во сколько раз менять амплитуду при полном присяде
            else if (iMovingState & mcCrouch)
                koef = 0.7; // во сколько раз менять амплитуду при присяде

            Fvector current_walk_offs{}, current_walk_rot{};

            if (iMovingState & mcFwd)
            {
                current_walk_offs.set(m_walk_offset[0]);
                current_walk_rot.set(m_walk_offset[1]);
            }
            else if (iMovingState & mcBack)
            {
                current_walk_offs.set(m_walk_offset[0].x, m_walk_offset[0].y, -m_walk_offset[0].z);
                current_walk_rot.set(m_walk_offset[1]);
            }
            else
            {
                current_walk_offs.set(Fvector{});
                current_walk_rot.set(Fvector{});
            }

            auto pda = smart_cast<CPda*>(this);
            auto missile = smart_cast<CMissile*>(this);

            current_walk_offs.mul(koef);
            current_walk_rot.mul(koef);
            current_walk_offs.mul(!IsZoomed());
            current_walk_rot.mul(!IsZoomed());
            current_walk_offs.mul(!pda && !missile);
            current_walk_rot.mul(!pda && !missile);
            current_walk_rot.mul(-PI / 180.f); // Преобразуем углы в радианы

            if (g_op22_hud_soft > 0.f) // OP22 (27.11, 3.251)
            {
                op22_Ease(current_walk[0], m_op22_soft_v[4][0], current_walk_offs, fWalkMaxTime, 0.85f);
                op22_Ease(current_walk[1], m_op22_soft_v[4][1], current_walk_rot, fWalkMaxTime, 0.85f);
            }
            else
            {
                m_op22_soft_v[4][0].set(0.f, 0.f, 0.f);
                m_op22_soft_v[4][1].set(0.f, 0.f, 0.f);
                if (!current_walk_offs.similar(current_walk[0], EPS))
                    current_walk[0].lerp(current_walk[0], current_walk_offs, fStepPerUpd);
                if (!current_walk_rot.similar(current_walk[1], EPS))
                    current_walk[1].lerp(current_walk[1], current_walk_rot, fStepPerUpd);
            }

            summary_offset.add(current_walk[0]);
            summary_rotate.add(current_walk[1]);
        }
    }

    //=============== OP22: процедурная отдача и «дыхание» ===============//
    Fvector op22_offs{}, op22_rot{};
    op22_UpdateProcedural(op22_offs, op22_rot, b_aiming, iMovingState);
    summary_offset.add(op22_offs);

    //================ Применение эффектов ===============//
    {
        // поворот с сохранением смещения by Zander
        Fvector _angle{}, _pos{trans.c};
        trans.getHPB(_angle);
        _angle.add(-summary_rotate);

        // Msg("##[%s] summary_rotate: [%f,%f,%f]", __FUNCTION__, summary_rotate.x, summary_rotate.y, summary_rotate.z);
        trans.setHPB(_angle.x, _angle.y, _angle.z);
        trans.c = _pos;

        Fmatrix hud_rotation;
        hud_rotation.identity();

        //================ Прицеливание ===============//
        hud_rotation.rotateX(current_difference[1].x);

        Fmatrix hud_rotation_part;
        hud_rotation_part.identity();
        hud_rotation_part.rotateY(current_difference[1].y);
        hud_rotation.mulA_43(hud_rotation_part);

        hud_rotation_part.identity();
        hud_rotation_part.rotateZ(current_difference[1].z);
        hud_rotation.mulA_43(hud_rotation_part);

        // OP22: поворот процедурной отдачи и «дыхания» - в осях худа, как поворот при прицеливании
        hud_rotation_part.identity();
        hud_rotation_part.rotateX(op22_rot.x);
        hud_rotation.mulA_43(hud_rotation_part);
        hud_rotation_part.identity();
        hud_rotation_part.rotateY(op22_rot.y);
        hud_rotation.mulA_43(hud_rotation_part);
        hud_rotation_part.identity();
        hud_rotation_part.rotateZ(op22_rot.z);
        hud_rotation.mulA_43(hud_rotation_part);

        //Msg("--[%s] summary_offset: [%f,%f,%f]", __FUNCTION__, summary_offset.x, summary_offset.y, summary_offset.z);

        hud_rotation.translate_over(summary_offset);
        trans.mulB_43(hud_rotation);
    }
    //====================================================//

    UpdateCollision(trans);

    if (g_op22_hud_trace > 0.f || m_op22_ev) // OP22 (27.11, 3.251): трасса худа
        op22_TraceWrite(trans, op22_offs, op22_rot, iMovingState);
}

float CHudItem::GetHudFov()
{
    // Рассчитываем HUD FOV от бедра (с учётом упирания в стены)
    if (m_nearwall_on)
    {
        // Получаем расстояние от камеры до точки в прицеле
        float dist = CollisionAllowed() ? saved_rq_range : HUD().GetCurrentRayQuery().range;

        // Интерполируем расстояние в диапазон от 0 (min) до 1 (max)
        clamp(dist, m_nearwall_dist_min, m_nearwall_dist_max);
        const float fDistanceMod = ((dist - m_nearwall_dist_min) / (m_nearwall_dist_max - m_nearwall_dist_min)); // 0.f ... 1.f

        // Плавно высчитываем итоговый FOV от бедра
        float src = m_nearwall_speed_mod * Device.fTimeDelta;
        clamp(src, 0.f, 1.f);

        const float fTrgFov = (IsZoomed() ? m_nearwall_target_aim_hud_fov : m_nearwall_target_hud_fov) +
            fDistanceMod * ((m_base_fov > 0.0f ? m_base_fov : psHUD_FOV_def) - (IsZoomed() ? m_nearwall_target_aim_hud_fov : m_nearwall_target_hud_fov));
        m_nearwall_last_hud_fov = m_nearwall_last_hud_fov * (1 - src) + fTrgFov * src;
    }

    return m_nearwall_last_hud_fov;
}

constexpr const char* BOBBING_SECT = "wpn_bobbing_effector";
constexpr float SPEED_REMINDER = 5.f;
CHudItem::CWeaponBobbing::CWeaponBobbing(CHudItem* parent) : parent_hud_item(parent)
{
    fTime = 0.f;
    fReminderFactor = 0.f;
    is_limping = false;

    m_fAmplitudeController = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "controller_amplitude", 0.003f);
    m_fAmplitudeRun = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "run_amplitude", 0.0075f);
    m_fAmplitudeWalk = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "walk_amplitude", 0.005f);
    m_fAmplitudeLimp = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "limp_amplitude", 0.011f);

    m_fSpeedRun = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "run_speed", 6.74f);
    m_fSpeedWalk = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "walk_speed", 6.26f);
    m_fSpeedLimp = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "limp_speed", 4.6f);

    m_fCrouchFactor = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "crouch_k", 0.75f);
    m_fZoomFactor = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "zoom_k", 1.f);
    m_fScopeZoomFactor = READ_IF_EXISTS(pSettings, r_float, BOBBING_SECT, "scope_zoom_k", m_fZoomFactor);
}

void CHudItem::CWeaponBobbing::CheckState()
{
    dwMState = Actor()->get_state();
    is_limping = Actor()->conditions().IsLimping();
    m_bZoomMode = Actor()->IsZoomAimingMode();
    fTime += Device.fTimeDelta;
}

void CHudItem::CWeaponBobbing::Update(Fmatrix& m, Fmatrix& m2)
{
    CheckState();

    // mmccxvii: Тряска рук при воздействии контролера
    const bool ControllerCondition = Actor()->PsyAuraAffect;

    // OP22 (27.11, 3.251): походка от настоящих шагов (op22_hud_gait) вместо синусоиды; тряска от контролёра - прежняя
    if (g_op22_hud_gait > 0.f && !ControllerCondition)
    {
        float k = (dwMState & ACTOR_DEFS::mcCrouch) ? m_fCrouchFactor : 1.f;
        if (m_bZoomMode)
        {
            auto wpn = smart_cast<CWeapon*>(parent_hud_item);
            k *= (wpn && wpn->IsScopeAttached() && !wpn->IsGrenadeMode()) ? m_fScopeZoomFactor : m_fZoomFactor;
        }
        const float A = isActorAccelerated(dwMState, m_bZoomMode) ? m_fAmplitudeRun : (is_limping ? m_fAmplitudeLimp : m_fAmplitudeWalk);
        fReminderFactor = 0.f; // при выключении - прежнее покачивание с нуля
        parent_hud_item->op22_GaitBob(m, m2, dwMState, A * k);
        return;
    }

    if ((dwMState & ACTOR_DEFS::mcAnyMove) || ControllerCondition)

    {
        if (fReminderFactor < 1.f)
            fReminderFactor += SPEED_REMINDER * Device.fTimeDelta;
        else
            fReminderFactor = 1.f;
    }
    else
    {
        if (fReminderFactor > 0.f)
            fReminderFactor -= SPEED_REMINDER * Device.fTimeDelta;
        else
            fReminderFactor = 0.f;
    }

    if (!fsimilar(fReminderFactor, 0))
    {
        float k = (dwMState & ACTOR_DEFS::mcCrouch) ? m_fCrouchFactor : 1.f;
        float k2 = k;

        if (m_bZoomMode)
        {
            float zoom_factor = m_fZoomFactor;

            auto wpn = smart_cast<CWeapon*>(parent_hud_item);
            if (wpn && wpn->IsScopeAttached() && !wpn->IsGrenadeMode())
                zoom_factor = m_fScopeZoomFactor;

            k2 *= zoom_factor;
        }

        float A, ST;

        // mmccxvii: Тряска рук при воздействии контролера
        if (ControllerCondition)
        {
            A = m_fAmplitudeController * k;
            ST = m_fSpeedRun * 10 * fTime * k;
        }
        else if (isActorAccelerated(dwMState, m_bZoomMode))
        {
            A = m_fAmplitudeRun * k2;
            ST = m_fSpeedRun * fTime * k;
        }
        else if (is_limping)
        {
            A = m_fAmplitudeLimp * k2;
            ST = m_fSpeedLimp * fTime * k;
        }
        else
        {
            A = m_fAmplitudeWalk * k2;
            ST = m_fSpeedWalk * fTime * k;
        }

        float _sinA = _abs(_sin(ST) * A) * fReminderFactor;

        // mmccxvii: Тряска рук при воздействии контролера
        if (ControllerCondition)
            _sinA *= ::Random.randF(-1.0f, 1.0f);

        float _cosA = _cos(ST) * A * fReminderFactor;

        // применяем к матрице положения рук

        Fvector dangle;
        Fmatrix R, mR;

        dangle.x = _cosA;
        dangle.z = _cosA;
        dangle.y = _sinA;

        R.setHPB(dangle.x, dangle.y, dangle.z);

        m.c.y += _sinA;

        mR.mul(m, R);

        m.k.set(mR.k);
        m.j.set(mR.j);

        m2.c.y += _sinA;

        mR.mul(m2, R);

        m2.k.set(mR.k);
        m2.j.set(mR.j);
    }
}

// OP22 (27.11, 3.251): походка - фаза идёт от шагов актора (op22_HudStep: удар пятки = 0), форма шага человеческая:
// внизу на ударе пятки, вверху на середине опоры; вбок - к опорной ноге (период - два шага), крен и разворот плеч
// запаздывают за боковым ходом; у каждого шага своя сила. Вход/выход из ходьбы - пружиной, перескок фазы сглажен.
void CHudItem::op22_GaitBob(Fmatrix& m, Fmatrix& m2, u32 mstate, float amp)
{
    float dt = Device.fTimeDelta;
    clamp(dt, 0.f, 0.05f);
    const bool moving = (mstate & mcAnyMove) != 0;
    {
        const float w = 9.f, tgt = moving ? 1.f : 0.f;
        const int n = _max(1, iCeil(dt / 0.004f));
        const float h = dt / n;
        for (int i = 0; i < n; ++i)
        {
            m_op22_gait_on_v += (w * w * (tgt - m_op22_gait_on) - 2.f * w * m_op22_gait_on_v) * h;
            m_op22_gait_on += m_op22_gait_on_v * h;
        }
    }
    if (moving)
    {
        m_op22_gait_phase += dt / _max(m_op22_gait_period, 0.2f);
        if (m_op22_gait_phase >= 1.f) // шаг без звука (нет материала) - походка не встаёт
        {
            m_op22_gait_phase -= 1.f;
            m_op22_gait_side = -m_op22_gait_side;
            m_op22_gait_amp = 1.f + ::Random.randFs(0.15f);
        }
    }
    const float on = _max(0.f, m_op22_gait_on);
    const float a = amp * m_op22_gait_amp * on * g_op22_hud_gait;
    const float p = m_op22_gait_phase, sd = m_op22_gait_side;
    const float c2 = _cos(PI_MUL_2 * (p + 0.06f * _sin(PI_MUL_2 * p))); // быстрее вниз, медленнее вверх
    Fvector tpos, trot;
    tpos.set(0.55f * a * sd * _sin(PI * p), -0.9f * a * c2, 0.f);
    trot.set(0.5f * a * c2, // ствол на миг клюёт на ударе пятки (rotateX: плюс - вниз)
             0.7f * a * sd * _sin(PI * (p - 0.3f)), // разворот плеч
             1.3f * a * sd * _sin(PI * (p - 0.15f))); // крен к опорной ноге
    {
        const float w = PI_MUL_2 * 6.f, kk = w * w, c = 2.f * 0.9f * w;
        const int n = _max(1, iCeil(dt / 0.004f));
        const float h = dt / n;
        for (int i = 0; i < n; ++i)
        {
            Fvector q;
            q.sub(tpos, m_op22_gait_out[0]).mul(kk).mad(m_op22_gait_out_v[0], -c);
            m_op22_gait_out_v[0].mad(q, h);
            m_op22_gait_out[0].mad(m_op22_gait_out_v[0], h);
            q.sub(trot, m_op22_gait_out[1]).mul(kk).mad(m_op22_gait_out_v[1], -c);
            m_op22_gait_out_v[1].mad(q, h);
            m_op22_gait_out[1].mad(m_op22_gait_out_v[1], h);
        }
    }
    if (m_op22_gait_out[0].square_magnitude() < 1e-14f && m_op22_gait_out[1].square_magnitude() < 1e-12f)
        return;

    Fmatrix R, Rp;
    R.identity();
    R.rotateX(m_op22_gait_out[1].x);
    Rp.identity();
    Rp.rotateY(m_op22_gait_out[1].y);
    R.mulA_43(Rp);
    Rp.identity();
    Rp.rotateZ(m_op22_gait_out[1].z);
    R.mulA_43(Rp);
    Fvector P{}, RP;
    if (g_op22_hud_pivot > 0.f)
        P.lerp(m_op22_pivot[0], m_op22_pivot[1], m_fZoomRotationFactor).mul(_min(g_op22_hud_pivot, 2.f));
    R.transform_dir(RP, P);
    R.c.set(m_op22_gait_out[0]).add(P).sub(RP);
    Fmatrix t;
    t.mul_43(R, m);
    m.set(t);
    t.mul_43(R, m2);
    m2.set(t);
}

// OP22 (27.11, 3.251): трасса худа для подстройки по измерению: op22_hud_trace 1 - пишет каждый кадр строку CSV в
// userdata/logs/op22_hud_trace_<время>.csv, 0 - закрывает файл. Только основной предмет в руках.
namespace
{
IWriter* g_op22_trace_w{};
u32 g_op22_trace_frame{u32(-1)}, g_op22_trace_lines{};
}

void CHudItem::op22_TraceWrite(const Fmatrix& trans, const Fvector& offs, const Fvector& rot, u32 mstate)
{
    const u32 ev = m_op22_ev;
    m_op22_ev = 0;
    if (g_op22_hud_trace <= 0.f || g_op22_trace_lines > 400000)
    {
        if (g_op22_trace_w)
        {
            FS.w_close(g_op22_trace_w);
            g_op22_trace_w = nullptr;
            Msg("[OP22 trace] closed, %u lines", g_op22_trace_lines);
        }
        if (g_op22_hud_trace <= 0.f)
            g_op22_trace_lines = 0;
        return;
    }
    CActor* pA = Actor();
    if (!pA || pA->inventory().ActiveItem() != m_item || g_op22_trace_frame == Device.dwFrame)
        return;
    g_op22_trace_frame = Device.dwFrame;
    if (!g_op22_trace_w)
    {
        const std::time_t tt = std::time(nullptr);
        string64 ts;
        std::strftime(ts, sizeof(ts), "%y%m%d_%H%M%S", std::localtime(&tt));
        string_path name;
        xr_sprintf(name, "op22_hud_trace_%s.csv", ts);
        g_op22_trace_w = FS.w_open("$logs$", name);
        if (!g_op22_trace_w)
        {
            g_op22_hud_trace = 0.f;
            return;
        }
        Msg("[OP22 trace] writing %s", name);
        extern float g_op22_wpn_breath, g_op22_wpn_recoil;
        string512 hdr;
        xr_sprintf(hdr, "# smooth=%.2f motor=%.2f gait=%.2f soft=%.2f bank=%.2f pivot=%.2f breath=%.2f body=%.2f variety=%.2f steps=%.2f hit=%.2f ads_arc=%.2f recoil=%.2f "
                        "inert=%.0f inert_spring=%.2f cam_follow=%.2f",
                   g_op22_hud_smooth, g_op22_hud_motor, g_op22_hud_gait, g_op22_hud_soft, g_op22_hud_bank, g_op22_hud_pivot, g_op22_wpn_breath,
                   g_op22_body_state, g_op22_hud_variety, g_op22_hud_steps, g_op22_hud_hit, g_op22_ads_arc, g_op22_wpn_recoil, g_op22_hud_inertion,
                   g_op22_hud_inertion_spring, g_op22_cam_follow);
        g_op22_trace_w->w_string(hdr);
        xr_sprintf(hdr, "# section=%s", m_item ? m_item->object().cNameSect().c_str() : "?");
        g_op22_trace_w->w_string(hdr);
        g_op22_trace_w->w_string("t,dt,state,moving,zf,ev,anim,px,py,pz,yaw,pitch,roll,ox,oy,oz,rx,ry,rz,gait_y,gait_x,gait_roll,gait_phase,bank,turn,"
                                 "strafe_roll,breath_mul,ads_hold,excite,effort,bone_raw_x,bone_raw_y,bone_raw_z,bone_x,bone_y,bone_z");
    }
    // поза худа относительно камеры: положение (мм) и углы (град)
    Fvector d;
    d.sub(trans.c, Device.vCameraPosition);
    const Fvector &R = Device.vCameraRight, &U = Device.vCameraTop, &F = Device.vCameraDirection;
    const float px = d.dotproduct(R) * 1000.f, py = d.dotproduct(U) * 1000.f, pz = d.dotproduct(F) * 1000.f;
    const float yaw = rad2deg(atan2f(trans.k.dotproduct(R), trans.k.dotproduct(F)));
    const float pitch = rad2deg(atan2f(trans.k.dotproduct(U), trans.k.dotproduct(F)));
    const float roll = rad2deg(atan2f(-trans.j.dotproduct(R), trans.j.dotproduct(U)));
    Fvector braw{}, bf{};
    if (const op22_bone_filter* f = g_op22_trace_ref.f; f && g_op22_trace_ref.bone < f->count && f->raw.size() == f->count)
    {
        braw.set(f->raw[g_op22_trace_ref.bone]).mul(1000.f);
        bf.set(f->pos[g_op22_trace_ref.bone]).mul(1000.f);
    }
    string1024 line;
    xr_sprintf(line,
               "%.4f,%.4f,%u,%d,%.3f,%u,%s,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%."
               "3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
               Device.fTimeGlobal, Device.fTimeDelta, GetState(), (mstate & mcAnyMove) ? ((mstate & mcSprint) ? 2 : 1) : 0, m_fZoomRotationFactor, ev,
               (ev & 2) ? m_op22_ev_anm : "", px, py, pz, yaw, pitch, roll, offs.x * 1000.f, offs.y * 1000.f, offs.z * 1000.f, rad2deg(rot.x),
               rad2deg(rot.y), rad2deg(rot.z), m_op22_gait_out[0].y * 1000.f, m_op22_gait_out[0].x * 1000.f, rad2deg(m_op22_gait_out[1].z),
               m_op22_gait_phase, rad2deg(m_op22_bank), rad2deg(m_op22_turn), rad2deg(current_strafe[1].z), m_op22_breath_mul, m_op22_ads_hold,
               m_op22_excite, m_op22_effort, braw.x, braw.y, braw.z, bf.x, bf.y, bf.z);
    g_op22_trace_w->w_string(line);
    if ((++g_op22_trace_lines & 255) == 0)
        g_op22_trace_w->flush();
}

void CHudItem::GetBoneOffsetPosDir(const shared_str& bone_name, Fvector& dest_pos, Fvector& dest_dir, const Fvector& offset)
{
    const u16 bone_id = HudItemData()->m_model->LL_BoneID(bone_name);
    ASSERT_FMT(bone_id != BI_NONE, "!![%s] bone [%s] not found in weapon [%s]", __FUNCTION__, bone_name.c_str(), world_sect.c_str());
    Fmatrix& fire_mat = HudItemData()->m_model->LL_GetTransform(bone_id);
    fire_mat.transform_tiny(dest_pos, offset);
    HudItemData()->m_item_transform.transform_tiny(dest_pos);
    dest_pos.add(Device.vCameraPosition);
    dest_dir.set(0.f, 0.f, 1.f);
    HudItemData()->m_item_transform.transform_dir(dest_dir);
}

extern ENGINE_API float psHUD_FOV;

void CHudItem::CorrectDirFromWorldToHud(Fvector& worldPos)
{
    Fmatrix hud_project;
    hud_project.build_projection(deg2rad(psHUD_FOV <= 1.f ? psHUD_FOV * Device.fFOV : psHUD_FOV), Device.fASPECT, HUD_VIEWPORT_NEAR,
                                 g_pGamePersistent->Environment().CurrentEnv->far_plane);

    Device.mView.transform_dir(worldPos);
    hud_project.transform_dir(worldPos);

    Fmatrix{Device.mProject}.invert().transform_dir(worldPos);
    Fmatrix{Device.mView}.invert().transform_dir(worldPos);
}

void CHudItem::TimeLockAnimation()
{
    const u32 state = GetState();
    if ((state == eDeviceSwitch || state == eReload) && GetHUDmode())
    {
        string128 anm_time_param;
        xr_strconcat(anm_time_param, "lock_time_end_", m_current_motion.c_str());
        const float time = READ_IF_EXISTS(pSettings, r_float, HudSection(), anm_time_param, 0) * 1000.f; // Читаем с конфига время анимации (например, lock_time_end_anm_reload)
        const float current_time = Device.dwTimeGlobal - m_dwMotionStartTm;
        if (time && current_time >= time)
        {
            if (state == eDeviceSwitch)
            {
                DeviceUpdate();
            }
            else if (state == eReload)
            {
                if (auto wpn = smart_cast<CWeapon*>(this))
                    wpn->update_visual_bullet_textures();
            }
        }
    }
}

void CHudItem::OnAnimationEnd(u32 state)
{
    switch (state)
    {
    case eSprintStart:
        SprintType = true;
        SwitchState(eIdle);
        break;
    case eSprintEnd:
        SprintType = false;
        SwitchState(eIdle);
        break;
    }
}

bool CHudItem::AnmIdleMovingAllowed() const { return !HudBobbingAllowed() || Actor()->PsyAuraAffect; }

#include "stdafx.h"
#include "actor.h"
#include "weapon.h"
#include "mercuryball.h"
#include "inventory.h"
#include "hudmanager.h"
#include "UsableScriptObject.h"
#include "customzone.h"
#include "../xr_3da/GameMtlLib.h"
#include "ui/UIMainIngameWnd.h"
#include "Grenade.h"
#include "clsid_game.h"

#include "game_cl_base.h"
#include "Level.h"
#include "op22_pickup_marks.h" // OP22

op22_pickup_mark_cfg_t g_op22_pickup_mark_cfg; // OP22
xr_vector<op22_pickup_mark_t> g_op22_pickup_marks; // OP22

// OP22 23.11: подавление огнём (op22_suppression.h)
#include "op22_suppression.h"
#include "../xr_3da/CameraManager.h"
#include "../xr_3da/EffectorPP.h"
#include "ActorEffector.h" // CActorCameraManager

op22_suppress_cfg_t g_op22_suppress_cfg;
float g_op22_suppress = 0.f;
static float op22_suppress_t = -100.f; // время последнего близкого промаха
static constexpr EEffectorPPType OP22_SUPPRESS_PP = static_cast<EEffectorPPType>(0x0F22);

class COp22SuppressPP : public CEffectorPP
{
public:
    COp22SuppressPP() : CEffectorPP(OP22_SUPPRESS_PP, 100000.f, true) {}
    BOOL Valid() override { return g_op22_suppress > .001f; } // спало - камера удаляет постэффект, проход постпроцесса больше не нужен
    BOOL Process(SPPInfo& pp) override
    {
        SPPInfo to = pp_identity;
        to.gray = g_op22_suppress_cfg.gray;
        to.blur = g_op22_suppress_cfg.blur;
        pp.lerp(pp_identity, to, std::clamp(g_op22_suppress, 0.f, 1.f));
        return TRUE;
    }
};

void op22_suppress_near_miss(u32 shooter_id)
{
    CActor* a = Actor();
    if (!a || !a->g_Alive() || shooter_id == a->ID() || g_op22_suppress_cfg.add <= 0.f)
        return;
    g_op22_suppress = _min(1.f, g_op22_suppress + g_op22_suppress_cfg.add);
    op22_suppress_t = Device.fTimeGlobal;
}

void op22_suppress_update()
{
    CActor* a = Actor();
    if (!a || !a->g_Alive())
    {
        g_op22_suppress = 0.f;
        return;
    }
    if (g_op22_suppress <= 0.f)
        return;
    if (Device.fTimeGlobal - op22_suppress_t > g_op22_suppress_cfg.hold)
        g_op22_suppress = _max(0.f, g_op22_suppress - g_op22_suppress_cfg.decay * Device.fTimeDelta);
    if (g_op22_suppress > .001f && (g_op22_suppress_cfg.gray > 0.f || g_op22_suppress_cfg.blur > 0.f) && !a->Cameras().GetPPEffector(OP22_SUPPRESS_PP))
        a->Cameras().AddPPEffector(xr_new<COp22SuppressPP>());
}

#define PICKUP_INFO_COLOR 0xFFDDDDDD
// AAAAAA

void CActor::feel_touch_new(CObject* O) {}

void CActor::feel_touch_delete(CObject* O)
{
    CPhysicsShellHolder* sh = smart_cast<CPhysicsShellHolder*>(O);
    if (sh && sh->character_physics_support())
        m_feel_touch_characters--;
}

BOOL CActor::feel_touch_contact(CObject* O)
{
    CInventoryItem* item = smart_cast<CInventoryItem*>(O);
    CInventoryOwner* inventory_owner = smart_cast<CInventoryOwner*>(O);

    if (item && item->Useful() && !item->object().H_Parent())
        return TRUE;

    if (inventory_owner && inventory_owner != smart_cast<CInventoryOwner*>(this))
    {
        CPhysicsShellHolder* sh = smart_cast<CPhysicsShellHolder*>(O);
        if (sh && sh->character_physics_support())
            m_feel_touch_characters++;
        return TRUE;
    }

    return (FALSE);
}

BOOL CActor::feel_touch_on_contact(CObject* O)
{
    CCustomZone* custom_zone = smart_cast<CCustomZone*>(O);
    if (!custom_zone)
        return (TRUE);

    Fsphere sphere;
    sphere.P = Position();
    sphere.R = EPS_L;
    if (custom_zone->inside(sphere))
        return (TRUE);

    return (FALSE);
}

void CActor::PickupModeOn() { m_bPickupMode = true; }

void CActor::PickupModeOff() { m_bPickupMode = false; }

ICF static BOOL info_trace_callback(collide::rq_result& result, LPVOID params)
{
    BOOL& bOverlaped = *(BOOL*)params;
    if (result.O)
    {
        if (Level().CurrentEntity() == result.O)
        {
            // ignore self-actor
            return TRUE;
        }
        else
        {
            if (!Core.Features.test(xrCore::Feature::pickup_check_overlaped))
                return TRUE;
        }
    }
    else
    {
        //получить треугольник и узнать его материал
        CDB::TRI* T = Level().ObjectSpace.GetStaticTris() + result.element;
        const auto* mtl = GMLib.GetMaterialByIdx(T->material);
        if (mtl->Flags.is(SGameMtl::flPassable))
            return TRUE;
        // возможно это сетка-рабица и через нее можно брать предметы
        else if (fsimilar(mtl->fVisTransparencyFactor, 1.0f, EPS) && fsimilar(mtl->fShootFactor, 1.0f, EPS) && mtl->Flags.is(SGameMtl::flSuppressWallmarks))
            return TRUE;
    }
    bOverlaped = TRUE;
    return FALSE;
}

BOOL CActor::CanPickItem(const CFrustum& frustum, const Fvector& from, CObject* item)
{
    if (!item->getVisible())
        return FALSE;

    if (smart_cast<CObject*>(ObjectWeLookingAt()) == item)
        return TRUE;

    BOOL bOverlaped = FALSE;
    Fvector dir, to;
    item->Center(to);
    float range = dir.sub(to, from).magnitude();
    if (range > 0.25f)
    {
        if (frustum.testSphere_dirty(to, item->Radius()))
        {
            dir.div(range);
            collide::ray_defs RD(from, dir, range, CDB::OPT_CULL, collide::rqtBoth);
            VERIFY(!fis_zero(RD.dir.square_magnitude()));
            RQR.r_clear();
            Level().ObjectSpace.RayQuery(RQR, RD, info_trace_callback, &bOverlaped, NULL, item);
        }
    }
    return !bOverlaped;
}

void CActor::PickupModeUpdate()
{
    g_op22_pickup_marks.clear(); // OP22: метки - только пока зажата F
    if (!m_bPickupMode)
        return;

    //. ????? GetNearest ?????
    feel_touch_update(Position(), /*inventory().GetTakeDist()*/ m_fPickupInfoRadius);

    CFrustum frustum;
    frustum.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB | FRUSTUM_P_FAR);

    // OP22 23.11: метки GAMMA вместо названий - предметы в радиусе меток, только видимые (та же проверка лучом, что у названий ОП).
    // Поиск и лучи видимости - 10 раз в секунду (в месте с сотней предметов луч на каждый в каждом кадре стоил миллисекунды),
    // положение и расстояние меток - каждый кадр. Первый кадр после нажатия F - сразу полный поиск.
    if (g_op22_pickup_mark_cfg.radius > 0.f)
    {
        constexpr u32 OP22_MARK_REFRESH_MS = 100;
        static xr_vector<u32> op22_visible; // OP22 (id32)
        static u32 op22_next_scan{}, op22_last_frame{};
        const bool op22_first = Device.dwFrame != op22_last_frame + 1;
        op22_last_frame = Device.dwFrame;
        if (op22_first || Device.dwTimeGlobal >= op22_next_scan)
        {
            op22_next_scan = Device.dwTimeGlobal + OP22_MARK_REFRESH_MS;
            op22_visible.clear();
            static xr_vector<CObject*> op22_near;
            op22_near.clear();
            Level().ObjectSpace.GetNearest(op22_near, Position(), g_op22_pickup_mark_cfg.radius, this);
            for (CObject* o : op22_near)
            {
                CInventoryItem* item = smart_cast<CInventoryItem*>(o);
                if (!item || !item->Useful() || item->object().H_Parent())
                    continue;
                if (!CanPickItem(frustum, Device.vCameraPosition, o))
                    continue;
                op22_visible.push_back(o->ID());
            }
        }
        for (const u32 id : op22_visible) // OP22 (id32)
        {
            CObject* o = Level().Objects.net_Find(id);
            CInventoryItem* item = o && !o->getDestroy() ? smart_cast<CInventoryItem*>(o) : nullptr;
            if (!item || item->object().H_Parent()) // подобран или удалён между поисками
                continue;
            op22_pickup_mark_t m;
            o->Center(m.p);
            m.d = Device.vCameraPosition.distance_to(m.p);
            g_op22_pickup_marks.push_back(m);
        }
        return;
    }

    //. slow (ray-query test)
    for (xr_vector<CObject*>::iterator it = feel_touch.begin(); it != feel_touch.end(); it++)
        if (CanPickItem(frustum, Device.vCameraPosition, *it))
            PickupInfoDraw(*it);
}

#include "../xr_3da/camerabase.h"
#include "script_game_object.h"

void CActor::PickupModeUpdate_COD()
{
    if (Level().CurrentViewEntity() != this)
        return;

    if (!g_Alive() || eacFirstEye != cam_active)
    {
        HUD().GetUI()->UIMainIngameWnd->SetPickUpItem(NULL);
        return;
    }

    //подбирание объекта
    if (inventory().m_pTarget 
        && inventory().m_pTarget->Useful() 
        && m_pUsableObject 
        && m_pUsableObject->nonscript_usable() &&
        !Level().m_feel_deny.is_object_denied(smart_cast<CGameObject*>(inventory().m_pTarget)))
    {
        CInventoryItem* pNearestItem = inventory().m_pTarget;
        if (m_bPickupMode)
        {
            bool allow_pickup = true;

            if (pSettings->line_exist("engine_callbacks", "actor_on_item_before_pickup"))
            {
                std::string on_item_before_pickup = pSettings->r_string("engine_callbacks", "actor_on_item_before_pickup");

                // Tronex: ability to prevent item picking up if the export returns false
                luabind::functor<bool> func;
                if (ai().script_engine().functor(on_item_before_pickup.c_str(), func))
                {
                    allow_pickup = func(pNearestItem->cast_game_object()->lua_game_object());
                }
            }

            PickupModeOff();

            if (allow_pickup)
            {
                // подбирание объекта
                Game().SendPickUpEvent(ID(), pNearestItem->object().ID());
                pNearestItem = nullptr;
            }
        }

        HUD().GetUI()->UIMainIngameWnd->SetPickUpItem(pNearestItem);

        return;
    }

    CFrustum frustum;
    frustum.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB | FRUSTUM_P_FAR);

    //---------------------------------------------------------------------------
    ISpatialResult.clear();
    g_SpatialSpace->q_frustum(ISpatialResult, 0, STYPE_COLLIDEABLE, frustum);
    //---------------------------------------------------------------------------

    float maxlen = 1000.0f;

    CInventoryItem* pNearestItem = NULL;
    for (u32 o_it = 0; o_it < ISpatialResult.size(); o_it++)
    {
        ISpatial* spatial = ISpatialResult[o_it];
        CInventoryItem* pIItem = smart_cast<CInventoryItem*>(spatial->dcast_CObject());
        if (0 == pIItem)
            continue;

        if (pIItem->object().H_Parent() != NULL)
            continue;

        if (!pIItem->CanTake())
            continue;

        if (pIItem->object().CLS_ID == CLSID_OBJECT_G_RPG7 || pIItem->object().CLS_ID == CLSID_OBJECT_G_FAKE)
            continue;

        CGrenade* pGrenade = smart_cast<CGrenade*>(spatial->dcast_CObject());
        if (pGrenade && !pGrenade->Useful())
            continue;

        CMissile* pMissile = smart_cast<CMissile*>(spatial->dcast_CObject());
        if (pMissile && !pMissile->Useful())
            continue;

        Fvector A, B, tmp;
        pIItem->object().Center(A);
        if (A.distance_to_sqr(Position()) > 4)
            continue;

        tmp.sub(A, cam_Active()->vPosition);
        B.mad(cam_Active()->vPosition, cam_Active()->vDirection, tmp.dotproduct(cam_Active()->vDirection));
        float len = B.distance_to_sqr(A);
        if (len > 1)
            continue;

        if (maxlen > len && !pIItem->object().getDestroy())
        {
            maxlen = len;
            pNearestItem = pIItem;
        }
    }

    if (pNearestItem)
    {
        CFrustum frustum;
        frustum.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB | FRUSTUM_P_FAR);
        if (!CanPickItem(frustum, Device.vCameraPosition, &pNearestItem->object()))
            pNearestItem = NULL;
    }

    if (pNearestItem && pNearestItem->cast_game_object())
    {
        if (Level().m_feel_deny.is_object_denied(pNearestItem->cast_game_object()))
            pNearestItem = NULL;
    }

    if (pNearestItem)
    {
        if (m_bPickupMode)
        {
            bool allow_pickup = true;

            if (pSettings->line_exist("engine_callbacks", "actor_on_item_before_pickup"))
            {
                std::string on_item_before_pickup = pSettings->r_string("engine_callbacks", "actor_on_item_before_pickup");

                // Tronex: ability to prevent item picking up if the export returns false
                luabind::functor<bool> func;
                if (ai().script_engine().functor(on_item_before_pickup.c_str(), func))
                {
                    allow_pickup = func(pNearestItem->cast_game_object()->lua_game_object());
                }
            }

            PickupModeOff();

            if (allow_pickup)
            {
                // подбирание объекта
                Game().SendPickUpEvent(ID(), pNearestItem->object().ID());
                pNearestItem = nullptr;
            }
        }
    }

    HUD().GetUI()->UIMainIngameWnd->SetPickUpItem(pNearestItem);
};

void CActor::PickupInfoDraw(CObject* object)
{
    LPCSTR draw_str = NULL;

    CInventoryItem* item = smart_cast<CInventoryItem*>(object);
    //.	CInventoryOwner* inventory_owner = smart_cast<CInventoryOwner*>(object);
    //.	VERIFY(item || inventory_owner);
    if (!item)
        return;

    Fmatrix res;
    res.mul(Device.mFullTransform, object->XFORM());
    Fvector4 v_res;
    Fvector shift;

    draw_str = item->Name /*Complex*/ ();
    shift.set(0, 0, 0);

    res.transform(v_res, shift);

    if (v_res.z < 0 || v_res.w < 0)
        return;
    if (v_res.x < -1.f || v_res.x > 1.f || v_res.y < -1.f || v_res.y > 1.f)
        return;

    float x = (1.f + v_res.x) / 2.f * (Device.dwWidth);
    float y = (1.f - v_res.y) / 2.f * (Device.dwHeight);

    HUD().Font().pFontLetterica16Russian->SetAligment(CGameFont::alCenter);
    HUD().Font().pFontLetterica16Russian->SetColor(PICKUP_INFO_COLOR);
    HUD().Font().pFontLetterica16Russian->Out(x, y, draw_str);
}

void CActor::feel_sound_new(CObject* who, int type, CSound_UserDataPtr user_data, const Fvector& Position, float power)
{
    if (who == this)
        m_snd_noise = _max(m_snd_noise, power);
}

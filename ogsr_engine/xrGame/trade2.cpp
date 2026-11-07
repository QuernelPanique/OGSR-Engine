#include "stdafx.h"
#include "trade.h"
#include "actor.h"
#include "ai/stalker/ai_stalker.h"
#include "ai/trader/ai_trader.h"
#include "artifact.h"
#include "inventory.h"
#include "xrmessages.h"
#include "character_info.h"
#include "relation_registry.h"
#include "level.h"
#include "script_callback_ex.h"
#include "script_game_object.h"
#include "game_object_space.h"
#include "trade_parameters.h"
#include "Weapon.h"
#include "WeaponAmmo.h"
#include "WeaponMagazinedWGrenade.h"
#include "CustomOutfit.h"

bool CTrade::CanTrade()
{
    CEntity* pEntity;

    m_nearest.clear();
    Level().ObjectSpace.GetNearest(m_nearest, pThis.base->Position(), 2.f, NULL);
    if (!m_nearest.empty())
    {
        for (u32 i = 0, n = m_nearest.size(); i < n; ++i)
        {
            // Может ли объект торговать
            pEntity = smart_cast<CEntity*>(m_nearest[i]);
            if (pEntity && !pEntity->g_Alive())
                return false;
            if (SetPartner(pEntity))
                break;
        }
    }

    if (!pPartner.base)
        return false;

    // Объект рядом
    float dist = pPartner.base->Position().distance_to(pThis.base->Position());
    if (dist < 0.5f || dist > 4.5f)
    {
        RemovePartner();
        return false;
    }

    // Объект смотрит на меня
    float yaw, pitch;
    float yaw2, pitch2;

    pThis.base->Direction().getHP(yaw, pitch);
    pPartner.base->Direction().getHP(yaw2, pitch2);
    yaw = angle_normalize(yaw);
    yaw2 = angle_normalize(yaw2);

    float Res = rad2deg(_abs(yaw - yaw2) < PI ? _abs(yaw - yaw2) : PI_MUL_2 - _abs(yaw - yaw2));
    if (Res < 165.f || Res > 195.f)
    {
        RemovePartner();
        return false;
    }

    return true;
}

void CTrade::TransferItem(CInventoryItem* pItem, bool bBuying, bool change_money)
{
    // сумма сделки учитывая ценовой коэффициент
    // актер цену не говорит никогда, все делают за него
    u32 dwTransferMoney = GetItemPrice(pItem, bBuying);

    if (bBuying)
    {
        pPartner.inv_owner->on_before_sell(pItem);
        pThis.inv_owner->on_before_buy(pItem);
    }
    else
    {
        pThis.inv_owner->on_before_sell(pItem);
        pPartner.inv_owner->on_before_buy(pItem);
    }

    CGameObject* O1 = smart_cast<CGameObject*>(pPartner.inv_owner);
    CGameObject* O2 = smart_cast<CGameObject*>(pThis.inv_owner);

    if (!bBuying)
        swap(O1, O2);

    NET_Packet P;
    O1->u_EventGen(P, GE_TRADE_SELL, O1->ID());
    P.w_u16(pItem->object().ID());
    O1->u_EventSend(P);

    if (change_money)
    {
        if (bBuying)
            pPartner.inv_owner->set_money(pPartner.inv_owner->get_money() + dwTransferMoney, false);
        else
            pThis.inv_owner->set_money(pThis.inv_owner->get_money() + dwTransferMoney, false);
    }

    // взять у партнера
    O2->u_EventGen(P, GE_TRADE_BUY, O2->ID());
    P.w_u16(pItem->object().ID());
    O2->u_EventSend(P);

    if (change_money)
    {
        if (bBuying)
            pThis.inv_owner->set_money(pThis.inv_owner->get_money() - dwTransferMoney, false);
        else
            pPartner.inv_owner->set_money(pPartner.inv_owner->get_money() - dwTransferMoney, false);
    }

    if ((pPartner.type == TT_ACTOR) || (pThis.type == TT_ACTOR))
    {
        bool bDir = (pThis.type != TT_ACTOR) && bBuying;
        Actor()->callback(GameObject::eTradeSellBuyItem)(pItem->object().lua_game_object(), bDir, dwTransferMoney);
    }
}

CInventory& CTrade::GetTradeInv(SInventoryOwner owner)
{
    R_ASSERT(TT_NONE != owner.type);

    // return ((TT_TRADER == owner.type) ? (*owner.inv_owner->m_trade_storage) : (owner.inv_owner->inventory()));
    return owner.inv_owner->inventory();
}

CTrade* CTrade::GetPartnerTrade() { return pPartner.inv_owner->GetTrade(); }

CInventory* CTrade::GetPartnerInventory() { return &GetTradeInv(pPartner); }

CInventoryOwner* CTrade::GetPartner() { return pPartner.inv_owner; }

u32 CTrade::GetItemPrice(PIItem pItem, bool b_buying) const
{
    // computing relation factor
    float relation_factor;

    CHARACTER_GOODWILL attitude = RELATION_REGISTRY().GetAttitude(pPartner.inv_owner, pThis.inv_owner);

    if (NO_GOODWILL == attitude)
        relation_factor = 0.f;
    else
        relation_factor = float(attitude + 1000.f) / 2000.f;

    clamp(relation_factor, 0.f, 1.f);

    bool buying = true;
    bool is_actor = (pThis.type == TT_ACTOR) || (pPartner.type == TT_ACTOR);
    if (is_actor)
    {
        buying = b_buying;
    }

    // OP22: the price is built as in xrGame.dll of OP (CTrade::GetItemPrice, 0x180919210): the item and every part of it
    // go by their own line in the list of the trader. Decisions 07.11 in DECISIONS.md.
    auto& trade_params = pThis.inv_owner->trade_parameters();
    const auto is_enabled = [&](const shared_str& section) {
        return buying ? trade_params.enabled(CTradeParameters::action_buy(0), section) : trade_params.enabled(CTradeParameters::action_sell(0), section);
    };
    const auto action_factor = [&](const shared_str& section) -> float {
        if (!is_enabled(section))
            return 0.f;
        const CTradeFactors& trade_factors =
            buying ? trade_params.factors(CTradeParameters::action_buy(0), section) : trade_params.factors(CTradeParameters::action_sell(0), section);

        float factor;
        if (trade_factors.friend_factor() <= trade_factors.enemy_factor())
            factor = trade_factors.friend_factor() + (trade_factors.enemy_factor() - trade_factors.friend_factor()) * (1.f - relation_factor);
        else
            factor = trade_factors.enemy_factor() + (trade_factors.friend_factor() - trade_factors.enemy_factor()) * relation_factor;

        clamp(factor, _min(trade_factors.enemy_factor(), trade_factors.friend_factor()), _max(trade_factors.enemy_factor(), trade_factors.friend_factor()));
        return factor;
    };

    // OP22: a forbidden item is worth nothing as a whole. OP zeroes only its own part and pays for the addons (decision 07.11)
    const shared_str& item_section = pItem->object().cNameSect();
    if (!is_enabled(item_section))
        return 0;

    // own cost without addons, ammo by the share of the box
    float base_cost = (float)pItem->CInventoryItem::Cost();
    if (const auto* ammo = smart_cast<CWeaponAmmo*>(pItem))
        base_cost = base_cost / (float)ammo->m_boxSize * (float)ammo->m_boxCurr;

    const auto section_cost = [](const shared_str& section) { return READ_IF_EXISTS(pSettings, r_float, section.c_str(), "cost", 0.f); };
    const auto addon_part = [&](const shared_str& addon) { return addon.size() ? section_cost(addon) * action_factor(addon) : 0.f; };
    const auto ammo_part = [&](const xr_vector<CCartridge>& magazine, int count) -> float {
        if (magazine.empty() || count <= 0)
            return 0.f;
        const shared_str& ammo = magazine.back().m_ammoSect;
        return section_cost(ammo) / READ_IF_EXISTS(pSettings, r_float, ammo.c_str(), "box_size", 1.f) * (float)count * action_factor(ammo);
    };
    const auto condition_factor = [](float condition) { return powf(condition * 0.9f + 0.1f, 0.75f); };

    float parts_cost = 0.f;
    if (auto* wpn = smart_cast<CWeapon*>(pItem))
    {
        if (wpn->IsScopeAttached() && wpn->ScopeAttachable())
            parts_cost += addon_part(wpn->GetScopeName());
        if (wpn->IsSilencerAttached() && wpn->SilencerAttachable())
            parts_cost += addon_part(wpn->GetSilencerName());
        if (wpn->IsGrenadeLauncherAttached() && wpn->GrenadeLauncherAttachable())
            parts_cost += addon_part(wpn->GetGrenadeLauncherName());
        parts_cost += ammo_part(wpn->m_magazine, wpn->GetAmmoElapsed());
        if (auto* wgl = smart_cast<CWeaponMagazinedWGrenade*>(wpn))
            parts_cost += ammo_part(wgl->m_magazine2, (int)wgl->m_magazine2.size());
    }
    else if (auto* outfit = smart_cast<CCustomOutfit*>(pItem))
    {
        // the battery of the exoskeleton lives in op22_exo.script, OP keeps it in CExoOutfit
        luabind::functor<LPCSTR> f_sect;
        luabind::functor<float> f_charge;
        if (ai().script_engine().functor("op22_exo.panel_section", f_sect) && ai().script_engine().functor("op22_exo.panel_charge", f_charge))
        {
            LPCSTR battery = f_sect(outfit->lua_game_object());
            if (battery && battery[0] && xr_strcmp(battery, "-") != 0)
            {
                const shared_str section(battery);
                parts_cost += section_cost(section) * action_factor(section) * condition_factor(f_charge(outfit->lua_game_object()));
            }
        }
    }

    // computing deficit_factor
    float deficit_factor = 1.f;
    if (Core.Features.test(xrCore::Feature::use_trade_deficit_factor))
    {
        deficit_factor = pThis.inv_owner->deficit_factor(pItem->object().cNameSect());

        clamp(deficit_factor, READ_IF_EXISTS(pSettings, r_float, "trade", "min_deficit_factor", 1), READ_IF_EXISTS(pSettings, r_float, "trade", "max_deficit_factor", 1));
    }

    const float original_result = base_cost * condition_factor(pItem->GetCondition()) * action_factor(item_section) + parts_cost;

    // total price calculation
    float result = original_result * deficit_factor;

    const char* price_callback = b_buying ? "trade_get_buy_price" : "trade_get_sell_price";

    // use some script discounts
    if (pSettings->line_exist("engine_callbacks", price_callback))
    {
        const char* callback = pSettings->r_string("engine_callbacks", price_callback);
        if (luabind::functor<float> lua_function; ai().script_engine().functor(callback, lua_function))
        {
            result = lua_function(smart_cast<const CGameObject*>(pThis.inv_owner)->lua_game_object(), // trader
                                  pItem->object().cNameSect().c_str(), // item section
                                  result, // total price calculated by engine
                                  original_result, // price without deficit_factor
                                  deficit_factor // current deficit_factor
            );
        }
    }

    return iFloor(result);
}

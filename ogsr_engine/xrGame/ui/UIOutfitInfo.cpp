#include "StdAfx.h"
#include "UIOutfitInfo.h"
#include "UIXmlInit.h"
#include "UIStatic.h"
#include "UIScrollView.h"
#include "../actor.h"
#include "../CustomOutfit.h"
#include "../string_table.h"
#include "../op22_options.h" // OP22

CUIOutfitInfo::CUIOutfitInfo() {}

CUIOutfitInfo::~CUIOutfitInfo()
{
    for (u32 i = _item_start; i < _max_item_index; ++i)
    {
        CUIStatic* _s = m_items[i];
        xr_delete(_s);
    }
}

LPCSTR _imm_names[] = {
    "health_restore_speed", "radiation_restore_speed", "satiety_restore_speed", "thirst_restore_speed", "power_restore_speed", "bleeding_restore_speed", "psy_health_restore_speed",

    "burn_immunity",        "shock_immunity",          "strike_immunity",       "wound_immunity",       "radiation_immunity",  "telepatic_immunity",     "chemical_burn_immunity",
    "explosion_immunity",   "fire_wound_immunity",  "jump_speed_delta",
};

LPCSTR _imm_st_names[] = {
    "ui_inv_health",
    "ui_inv_radiation",
    "ui_inv_satiety",
    "ui_inv_thirst",
    "ui_inv_power",
    "ui_inv_bleeding",
    "ui_inv_psy_health",

    "ui_inv_outfit_burn_protection",
    "ui_inv_outfit_shock_protection",
    "ui_inv_outfit_strike_protection",
    "ui_inv_outfit_wound_protection",
    "ui_inv_outfit_radiation_protection",
    "ui_inv_outfit_telepatic_protection",
    "ui_inv_outfit_chemical_burn_protection",
    "ui_inv_outfit_explosion_protection",
    "ui_inv_outfit_fire_wound_protection",
    "ui_inv_jump_speed_delta",
};

LPCSTR _actor_param_names[] = {"satiety_health_v", "radiation_v", "satiety_v", "thirst_v", "satiety_power_v", "wound_incarnation_v", "psy_health_v"};

void CUIOutfitInfo::InitFromXml(CUIXml& xml_doc)
{
    LPCSTR _base = "outfit_info";

    string256 _buff;
    CUIXmlInit::InitWindow(xml_doc, _base, 0, this);

    m_listWnd = xr_new<CUIScrollView>();
    m_listWnd->SetAutoDelete(true);
    AttachChild(m_listWnd);
    strconcat(sizeof(_buff), _buff, _base, ":scroll_view");
    CUIXmlInit::InitScrollView(xml_doc, _buff, 0, m_listWnd);

    for (u32 i = _item_start; i < _max_item_index; ++i)
    {
        strconcat(sizeof(_buff), _buff, _base, ":static_", _imm_names[i]);

        if (xml_doc.NavigateToNode(_buff, 0))
        {
            m_items[i] = xr_new<CUIStatic>();
            CUIStatic* _s = m_items[i];
            _s->SetAutoDelete(false);
            CUIXmlInit::InitStatic(xml_doc, _buff, 0, _s);

            // OP22: две колонки значений, как immunities_list ОП: правые края 68 % и 90 % ширины строки
            for (int c = 0; c < 2; ++c)
            {
                CUIStatic* v = xr_new<CUIStatic>();
                v->SetAutoDelete(true);
                const float left = _s->GetWidth() * (c ? 0.66f : 0.48f);
                v->SetWndRect(Frect().set(left, 0.f, left + _s->GetWidth() * (c ? 0.24f : 0.2f), _s->GetHeight())); // lt + rb
                v->SetFont(_s->GetFont());
                v->SetTextComplexMode(true);
                v->SetTextAlignment(CGameFont::alRight);
                _s->AttachChild(v);
                m_op22_vals[i][c] = v;
            }
        }
    }
}

float CUIOutfitInfo::GetArtefactParam(ActorRestoreParams params, u32 i)
{
    float r = 0;
    switch (i)
    {
    case _item_health_restore_speed: r = params.HealthRestoreSpeed; break;
    case _item_radiation_restore_speed: r = params.RadiationRestoreSpeed; break;
    case _item_satiety_restore_speed: r = params.SatietyRestoreSpeed; break;
    case _item_thirst_restore_speed: r = params.ThirstRestoreSpeed; break;
    case _item_power_restore_speed: r = params.PowerRestoreSpeed; break;
    case _item_bleeding_restore_speed: r = params.BleedingRestoreSpeed; break;
    case _item_psy_health_restore_speed: r = params.PsyHealthRestoreSpeed; break;
    }
    return r;
}

#include "script_game_object.h"

// OP22: значение в колонке: пусто при нуле, цвет по знаку
static void op22_set_val(CUIStatic* st, float val, LPCSTR fmt, LPCSTR suffix, bool bad_plus)
{
    if (fis_zero(val, 0.05f))
    {
        st->SetText("");
        return;
    }
    const bool good = bad_plus ? val < 0.f : val > 0.f;
    string64 buf;
    sprintf_s(buf, "%s", good ? "%c[green]" : "%c[red]");
    const size_t len = xr_strlen(buf);
    sprintf_s(buf + len, sizeof(buf) - len, fmt, val);
    strcat_s(buf, suffix);
    st->SetText(buf);
}

void CUIOutfitInfo::Update(CCustomOutfit* outfit)
{
    auto artefactEffects = Actor()->ActiveArtefactsOnBelt();
    const float cond = outfit ? outfit->GetCondition() : 0.f;

    m_listWnd->Clear(false); // clear existing items and do not scroll to top

    const u32 imm = _max_item_index1;
    // OP22: порядок и строки как панель ОП (FUN_180afca30): сытости и выносливости она не показывает
    static const u32 order[] = {imm + ALife::eHitTypeExplosion, imm + ALife::eHitTypeWound,     imm + ALife::eHitTypeBurn,
                                imm + ALife::eHitTypeFireWound, imm + ALife::eHitTypeRadiation, imm + ALife::eHitTypeStrike,
                                imm + ALife::eHitTypeTelepatic, imm + ALife::eHitTypeChemicalBurn, imm + ALife::eHitTypeShock,
                                _item_jump_speed_delta,         _item_health_restore_speed,     _item_bleeding_restore_speed,
                                _item_radiation_restore_speed,  _item_psy_health_restore_speed};
    const bool modificators = op22_opt("gp_show_modificators", true);

    for (u32 i : order)
    {
        CUIStatic* _s = m_items[i];
        if (!_s)
            continue;

        float col1 = 0.f, col2 = 0.f;
        LPCSTR fmt = "%+3.0f", sfx = "%";
        bool bad_plus = false;

        if (i >= imm && i < _item_jump_speed_delta)
        {
            const ALife::EHitType ht = ALife::EHitType(i - imm);
            col1 = (1.f - (outfit ? outfit->GetDefHitTypeProtection(ht) : 1.f)) * 100.f;
            col2 = (1.f - Actor()->HitArtefactsOnBelt(1.0f, ht)) * 100.f;
        }
        else if (!modificators)
            continue;
        else if (i == _item_jump_speed_delta)
        {
            col2 = Actor()->op22_belt_jump();
            fmt = "%+.1f";
            sfx = "m";
        }
        else
        {
            // пояс: сумма минус костюм (его вклад - с множителем состояния, как в обновлении актора); костюм: * 1000
            const float _actor_val = READ_IF_EXISTS(pSettings, r_float, "actor_condition", _actor_param_names[i], 0.f);
            if (fis_zero(_actor_val))
                continue;
            float outfit_v = 0.f;
            if (outfit)
            {
                if (i == _item_health_restore_speed)
                    outfit_v = outfit->m_fHealthRestoreSpeed;
                else if (i == _item_bleeding_restore_speed)
                    outfit_v = outfit->m_fBleedingRestoreSpeed;
                else if (i == _item_radiation_restore_speed)
                    outfit_v = outfit->RadiationRestoreSpeed();
                else
                    outfit_v = outfit->PsyHealthRestoreSpeed();
                outfit_v *= cond;
            }
            col2 = (GetArtefactParam(artefactEffects, i) - outfit_v) / _actor_val;
            col1 = i == _item_psy_health_restore_speed ? 0.f : outfit_v * 1000.f;
            if (i == _item_radiation_restore_speed)
            {
                sfx = *CStringTable().translate("ui_inv_radiation_units");
                bad_plus = true;
            }
            else
                col2 *= 100.f;
            if (i == _item_bleeding_restore_speed)
            {
                col1 *= -1.f;
                col2 *= -1.f;
                bad_plus = true;
            }
        }

        if (fis_zero(col1, 0.05f) && fis_zero(col2, 0.05f))
            continue;

        _s->SetText(*CStringTable().translate(_imm_st_names[i]));
        op22_set_val(m_op22_vals[i][0], col1, fmt, sfx, bad_plus);
        op22_set_val(m_op22_vals[i][1], col2, fmt, sfx, bad_plus);

        m_listWnd->AddWindow(_s, false);
    }

    if (pSettings->line_exist("engine_callbacks", "ui_actor_info_callback"))
    {
        const char* callback = pSettings->r_string("engine_callbacks", "ui_actor_info_callback");
        if (luabind::functor<void> lua_function; ai().script_engine().functor(callback, lua_function))
        {
            lua_function(m_listWnd, outfit ? outfit->lua_game_object() : nullptr);
        }
    }
}

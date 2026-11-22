// UIMainIngameWnd.h:  окошки-информация в игре
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "UIProgressBar.h"
#include "UIGameLog.h"

#include "../alife_space.h"

#include "UICarPanel.h"
#include "UIMotionIcon.h"
#include "../hudsound.h"
#include "../script_export_space.h"
#include "../../xr_3da/op22_uixf.h" // OP22

struct GAME_NEWS_DATA;

class CUIPdaMsgListItem;
class CLAItem;
class CUIZoneMap;
class CUIArtefactPanel;
class CUIScrollView;
class CActor;
class CWeapon;
class CMissile;
class CInventoryItem;
class CObject;

class CUIMainIngameWnd : public CUIWindow
{
public:
    CUIMainIngameWnd();
    virtual ~CUIMainIngameWnd();

    virtual void Init();
    virtual void Draw();
    void op22_draw_under(); // OP22: GAMMA 2.0 - подложка HUD (static_health op22_under) до пользовательских окон Lua
    bool m_op22_under{};
    float m_op22_icon_gap{-1.f}; // OP22: GAMMA 2.0 - значок патрона слева от текста названия с зазором (static_wpn_icon op22_before_text)
    virtual void Update();

    bool OnKeyboardPress(int dik);

protected:
    CUIStatic UIStaticHealth;
    CUIStatic UIStaticArmor;
    CUIStatic UIStaticQuickHelp;
    CUIProgressBar UIHealthBar;
    CUIProgressBar UIArmorBar;
    CUICarPanel UICarPanel;
    CUIMotionIcon UIMotionIcon;
    CUIZoneMap* UIZoneMap;

    //иконка, показывающая количество активных PDA
    CUIStatic UIPdaOnline;

    //изображение оружия
    CUIStatic UIWeaponBack;
    CUIStatic UIWeaponSignAmmo;
    CUIStatic UIWeaponIcon;

    Frect UIWeaponIcon_rect;

public:
    CUIStatic* GetPDAOnline() { return &UIPdaOnline; };
    CUIZoneMap* GetUIZoneMap() { return UIZoneMap; }

protected:
    // 5 статиков для отображения иконок:
    // - сломанного оружия
    // - радиации
    // - ранения
    // - голода
    // - усталости
    CUIStatic UIWeaponJammedIcon;
    CUIStatic UIExoChargeIcon; // OP22: заряд экзоскелета
    // OP22: панель быстрых слотов ОП 2.2, разметка - узел quickslots_panel скина HUD
    struct Op22QuickSlot
    {
        CUIWindow* back{};
        CUIStatic* icon{};
        CUIStatic* key{};
        CUIStatic* count{};
    };
    Op22QuickSlot m_op22_qs[5]{};
    CUIWindow* m_op22_qs_wnd{};
    float m_op22_qs_icon_k{1.f};
    Fvector2 m_op22_qs_cell{};
    u32 m_op22_qs_time{};
    void op22_qs_init(CUIXml& xml);
    void op22_qs_update();
    float m_op22_exo_min{}, m_op22_exo_max{1.f}; // OP22
    CUIStatic UIRadiaitionIcon;
    CUIStatic UIWoundIcon;
    CUIStatic UIStarvationIcon;
    CUIStatic UIPsyHealthIcon;
    CUIStatic UIInvincibleIcon;
    CUIStatic UIThirstIcon;
    //	CUIStatic			UISleepIcon;
    //	CUIStatic			UIArtefactIcon;

    CUIScrollView* m_UIIcons{};
    // OP22: GAMMA 2.0 - значки состояния строкой (icons_scroll_view op22_horz="1" op22_gap), патроны двумя узлами
    bool m_op22_icons_horz{};
    float m_op22_icons_gap{};
    CUIStatic* m_op22_ammo_mag{};
    CUIStatic* m_op22_ammo_rest{};
public:
    // OP22: HUD GAMMA 2.0 (22.11, макет design/gamma2/hud_float_20261122): группы перспективы op22_uixf.h -
    // 1 - панель слева внизу (тряска от движения ГГ - shake="1"), 2 - счётчик патронов у оружия (3 с при доставании,
    // перезарядке, смене режима огня), 3 - мини-карта с подписью и биорадаром, 4 - лента сообщений. Игрок двигает,
    // масштабирует и красит группы редактором HUD (консоль op22_hud_edit, раскладка - op22_hud_layout)
    struct Op22XfCfg
    {
        bool on{}, shake{};
        u8 parent{};          // группа внутри другой (полосы панели): геометрия и раскладка родителя, свой только цвет
        u32 color{0xffffff};  // исходный цвет вершин группы (текстура белая; цвет игрока его заменяет)
        Frect r{};
        float ry{}, rx{}, rz{}, ox{}, oy{}, pox{}, poy{}, persp{1.f};
        shared_str name; // подпись в редакторе HUD
    };
    const Op22XfCfg& op22_xf_cfg(int id) const { return m_op22_xf_cfg[id]; }
    void op22_hud_edit_open();

protected:
    Op22XfCfg m_op22_xf_cfg[OP22_UIXF_GROUPS]{};
    class CUIOp22HudEdit* m_op22_edit{};
    CUIStatic* m_op22_panel{};
    bool m_op22_ammo_class{};
    float m_op22_text_tint{};
    u32 m_op22_mstate{};
    int m_op22_loop{}, m_op22_shot{};
    float m_op22_loop_t0{}, m_op22_shot_t0{}, m_op22_hit_seen{}, m_op22_blend_t0{-100.f};
    Fvector m_op22_pose{}, m_op22_blend_from{};
    float m_op22_ammo_t0{-100.f}, m_op22_ammo_req_seen{};
    u32 m_op22_ammo_state{u32(-1)};
    Fvector2 m_op22_wpn_s{};
    bool m_op22_wpn_s_ok{};
    void op22_xf_init(CUIXml& xml);
    void op22_xf_update();
    void op22_icons_layout();

public:
    CUIScrollView* op22_icons_view() const { return m_UIIcons; } // OP22: бустеры, болезнь, бессонница
    CUIArtefactPanel* m_artefactPanel{};

public:
    // Енумы соответсвующие предупреждающим иконкам
    enum EWarningIcons
    {
        ewiAll = 0,
        ewiWeaponJammed,
        ewiRadiation,
        ewiWound,
        ewiStarvation,
        ewiPsyHealth,
        ewiInvincible,
        ewiThirst,
        //		ewiSleep,
        //		ewiArtefact,
    };

    // Задаем цвет соответствующей иконке
    void SetWarningIconColor(EWarningIcons icon, const u32 cl);
    void TurnOffWarningIcon(EWarningIcons icon);

    // Пороги изменения цвета индикаторов, загружаемые из system.ltx
    typedef xr_map<EWarningIcons, xr_vector<float>> Thresholds;
    typedef Thresholds::iterator Thresholds_it;
    Thresholds m_Thresholds;

    // Енум перечисления возможных мигающих иконок
    enum EFlashingIcons
    {
        efiPdaTask = 0,
        efiMail
    };

    void SetFlashIconState_(EFlashingIcons type, bool enable);

    void AnimateContacts(bool b_snd);
    HUD_SOUND m_contactSnd;

    void ReceiveNews(GAME_NEWS_DATA* news);

protected:
    void SetWarningIconColor(CUIStatic* s, const u32 cl);
    void InitFlashingIcons(CUIXml* node);
    void DestroyFlashingIcons();
    void UpdateFlashingIcons();
    void UpdateActiveItemInfo();

    void SetAmmoIcon(const shared_str& seсt_name);

    // first - иконка, second - анимация
    DEF_MAP(FlashingIcons, EFlashingIcons, CUIStatic*);
    FlashingIcons m_FlashingIcons;

    //для текущего активного актера и оружия
    CActor* m_pActor;
    CWeapon* m_pWeapon;
    CMissile* m_pGrenade;
    CInventoryItem* m_pItem;

    // Отображение подсказок при наведении прицела на объект
    void RenderQuickInfos();

public:
    CUICarPanel& CarPanel() { return UICarPanel; };
    CUIMotionIcon& MotionIcon() { return UIMotionIcon; }
    void OnConnected();
    void reset_ui();

protected:
    CInventoryItem* m_pPickUpItem;
    CUIStatic UIPickUpItemIcon;

    float m_iPickUpItemIconX{};
    float m_iPickUpItemIconY{};
    float m_iPickUpItemIconWidth{};
    float m_iPickUpItemIconHeight{};

    void UpdatePickUpItem();

public:
    void SetPickUpItem(CInventoryItem* PickUpItem);
    void op22_net_relcase(CObject* object); // OP22

    DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(CUIMainIngameWnd)
#undef script_type_list
#define script_type_list save_type_list(CUIMainIngameWnd)

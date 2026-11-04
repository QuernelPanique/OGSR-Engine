////////////////////////////////////////////////////////////////////////////
// OP22: скриптовый API списков перетаскивания ОП 2.2 (CUIDragDropListEx, CUICellItem, create_cell_helper,
// CScriptXmlInit:InitDragDropListEx). Имена и сигнатуры - по регистрациям в xrGame.dll ОП (Ghidra, 03.11).
// Потребители в скриптах ОП: container_viewer (контейнер артефактов), charger_viewer, ui_storage_viewer (рюкзак),
// callback_ui (подсветка подходящих). Первая версия: подсветка подходящих, маски и аддоны на ячейке - заглушки.
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "UIDragDropListEx.h"
#include "UICellItem.h"
#include "UICellItemFactory.h"
#include "UICellCustomItems.h"
#include "UIIconParams.h"
#include "UIXmlInit.h"
#include "../ScriptXmlInit.h"
#include "../script_game_object.h"
#include "../inventory_item.h"
#include "../GameObject.h"
#include "../game_object_space.h"
#include "UIInventoryWnd.h"
#include "UICarBodyWnd.h"
#include "UITradeWnd.h"
#include "UIPropertiesBox.h"
#include "UIListBoxItem.h"
#include "inventory_space.h"
#include "../ai_space.h"
#include "../script_engine.h"
#include "../WeaponMagazined.h"
#include "../WeaponAmmo.h"

using namespace luabind;

// номера событий ячеек: cell_focus_receive/lost/selected - как у актора в OGSR (on_cell_item_*), cell_dbl_click -
// свой (прослойка op22_compat.script задаёт callback.cell_dbl_click = OP22_CB_CELL_DBL_CLICK)
constexpr int OP22_CB_CELL_DBL_CLICK = 200008;

// ячейка, построенная по секции (в контейнере лежат записи о предметах, не сами предметы)
class CUIOP22SectionCell : public CUICellItem
{
    typedef CUICellItem inherited;

public:
    shared_str m_section;
    float m_condition{1.f};

    CUIOP22SectionCell(LPCSTR section, float cond) : m_section(section), m_condition(cond)
    {
        CIconParams params(m_section);
        params.set_shader(this);
        m_grid_size.set(iFloor(params.grid_width + .5f), iFloor(params.grid_height + .5f));
    }
    virtual bool EqualTo(CUICellItem*) { return false; }
};

// данные ячейки для скриптов: id ячейки, условие
static xr_map<CUICellItem*, shared_str> g_cell_ids;

// список перетаскивания для скриптов: события ячеек в Lua
class CUIOP22ScriptDDList : public CUIDragDropListEx
{
    typedef CUIDragDropListEx inherited;

    struct cb_t
    {
        luabind::functor<bool> f;
        luabind::object o; // присваивается только настоящий объект: пустой luabind::object без состояния Lua - вылет
        bool has_obj{};
    };
    xr_map<int, cb_t> m_cb;
    CUICellItem* m_focused{};

public:
    CUICellItem* m_script_selected{};
    int m_ui_list_id{};

    CUIOP22ScriptDDList()
    {
        m_f_item_selected = fastdelegate::MakeDelegate(this, &CUIOP22ScriptDDList::on_selected);
        m_f_item_db_click = fastdelegate::MakeDelegate(this, &CUIOP22ScriptDDList::on_db_click);
        m_f_item_start_drag = fastdelegate::MakeDelegate(this, &CUIOP22ScriptDDList::on_start_drag);
        m_f_item_drop = fastdelegate::MakeDelegate(this, &CUIOP22ScriptDDList::on_drop);
    }

    // set_callback(тип, nil) ОП снимает обработчик (container_viewer:on_quit) - nil тоже приходит как functor
    static bool is_function(const luabind::functor<bool>& f)
    {
        if (!f.is_valid())
            return false;
        lua_State* L = ai().script_engine().lua();
        f.pushvalue();
        const bool ok = lua_isfunction(L, -1);
        lua_pop(L, 1);
        return ok;
    }

    void set_cb(int type, const luabind::functor<bool>& f, const luabind::object& o)
    {
        if (!is_function(f))
        {
            reset_cb(type);
            return;
        }
        cb_t& c = m_cb[type];
        c.f = f;
        c.has_obj = o.is_valid() && o.type() != LUA_TNIL;
        if (c.has_obj)
            c.o = o;
    }
    void set_cb(int type, const luabind::functor<bool>& f)
    {
        if (!is_function(f))
        {
            reset_cb(type);
            return;
        }
        cb_t& c = m_cb[type];
        c.f = f;
        c.has_obj = false;
    }
    void reset_cb(int type) { m_cb.erase(type); }

    bool fire(int type, CUICellItem* itm)
    {
        auto it = m_cb.find(type);
        if (it == m_cb.end())
            return false;
        if (it->second.has_obj)
            return it->second.f(it->second.o, itm);
        return it->second.f(itm);
    }

    bool on_selected(CUICellItem* itm)
    {
        fire(int(GameObject::eCellItemSelect), itm);
        m_script_selected = itm; // после колбэка: ОП-скрипт снимает выделение с прежней через GetSelectedCell
        return false;
    }
    bool on_db_click(CUICellItem* itm) { return fire(OP22_CB_CELL_DBL_CLICK, itm) || true; } // без переноса движком
    bool on_start_drag(CUICellItem*) { return true; } // перенос в скриптовых окнах ОП - двойным кликом
    bool on_drop(CUICellItem*) { return true; }

    virtual CUICellItem* RemoveItem(CUICellItem* itm, bool force_root)
    {
        if (itm == m_script_selected)
            m_script_selected = nullptr;
        if (itm == m_focused)
            m_focused = nullptr;
        return inherited::RemoveItem(itm, force_root);
    }

    virtual void Update()
    {
        inherited::Update();
        CUICellItem* over = nullptr;
        for (u32 i = 0, n = ItemsCount(); i < n; ++i)
        {
            CUICellItem* c = GetItemIdx(i);
            if (c && c->CursorOverWindow())
            {
                over = c;
                break;
            }
        }
        if (over != m_focused)
        {
            if (m_focused)
                fire(int(GameObject::eCellItemFocusLost), m_focused);
            m_focused = over;
            if (m_focused)
                fire(int(GameObject::eCellItemFocus), m_focused);
        }
    }
};

static CUIOP22ScriptDDList* script_list(CUIDragDropListEx* l) { return smart_cast<CUIOP22ScriptDDList*>(l); }

// --- CScriptXmlInit
CUIDragDropListEx* op22_xml_InitDragDropListEx(CScriptXmlInit* self, LPCSTR path, CUIWindow* parent)
{
    CUIOP22ScriptDDList* pWnd = xr_new<CUIOP22ScriptDDList>();
    CUIXmlInit::InitDragDropListEx(self->op22_xml(), path, 0, pWnd);
    pWnd->SetAutoDelete(true);
    if (parent)
        parent->AttachChild(pWnd);
    return pWnd;
}

// --- CUIDragDropListEx
static int dd_GetUIListId(CUIDragDropListEx* l) { auto s = script_list(l); return s ? s->m_ui_list_id : 0; }
static void dd_SetUIListId(CUIDragDropListEx* l, int id) { if (auto s = script_list(l)) s->m_ui_list_id = id; }
static void dd_SetCellHeight(CUIDragDropListEx* l, int v) { Ivector2 sz = l->CellSize(); sz.y = v; l->SetCellSize(sz); }
static void dd_SetCellWidth(CUIDragDropListEx* l, int v) { Ivector2 sz = l->CellSize(); sz.x = v; l->SetCellSize(sz); }
static void dd_SetCellSize1(CUIDragDropListEx* l, Ivector2 v) { l->SetCellSize(v); }
static void dd_SetCellSize2(CUIDragDropListEx* l, int x, int y) { l->SetCellSize(Ivector2{x, y}); }
static void dd_SetCellsRows(CUIDragDropListEx* l, int v) { Ivector2 c = l->CellsCapacity(); c.y = v; l->SetCellsCapacity(c); }
static void dd_SetCellsColumns(CUIDragDropListEx* l, int v) { Ivector2 c = l->CellsCapacity(); c.x = v; l->SetCellsCapacity(c); }
static void dd_SetCellsCapacity1(CUIDragDropListEx* l, Ivector2 v) { l->SetCellsCapacity(v); }
static void dd_SetCellsCapacity2(CUIDragDropListEx* l, int x, int y) { l->SetCellsCapacity(Ivector2{x, y}); }
static u32 dd_ItemsCount(CUIDragDropListEx* l) { return l->ItemsCount(); }

static CUICellItem* make_item_cell(CScriptGameObject* obj)
{
    if (!obj)
        return nullptr;
    CInventoryItem* itm = smart_cast<CInventoryItem*>(&obj->object());
    if (!itm)
        return nullptr;
    auto prev = itm->m_cell_item; // ячейку предмета держит окно инвентаря - не перехватываем
    CUICellItem* cell = create_cell_item(itm);
    itm->m_cell_item = prev;
    return cell;
}
static void dd_SetItemObj(CUIDragDropListEx* l, CScriptGameObject* obj)
{
    if (CUICellItem* c = make_item_cell(obj))
        l->SetItem(c);
}
static void dd_SetItemObj2(CUIDragDropListEx* l, CScriptGameObject* obj, bool) { dd_SetItemObj(l, obj); }
static void dd_SetItemCell(CUIDragDropListEx* l, CUICellItem* c)
{
    if (c)
        l->SetItem(c);
}
static void dd_Compact(CUIDragDropListEx* l) { l->Compact(); }
static void dd_ClearAll(CUIDragDropListEx* l) { l->ClearAll(true); }
static bool dd_HasFreeSpace(CUIDragDropListEx* l) { return l->op22_HasFreeSpace(Ivector2{1, 1}); }
static CUICellItem* dd_FindSimilar(CUIDragDropListEx*, CUICellItem*) { return nullptr; }
static CUICellItem* dd_RemoveItem(CUIDragDropListEx* l, CUICellItem* c) { return c ? l->RemoveItem(c, false) : nullptr; }
static void dd_noop(CUIDragDropListEx*) {}
static void dd_SetSuitable1(CUIDragDropListEx*, LPCSTR) {}
static void dd_SetSuitable2(CUIDragDropListEx*, const luabind::object&) {}
static CUICellItem* dd_GetSelectedCell(CUIDragDropListEx* l) { auto s = script_list(l); return s ? s->m_script_selected : nullptr; }
static CUICellItem* dd_GetItemIdx(CUIDragDropListEx* l, u32 idx, bool) { return idx < l->ItemsCount() ? l->GetItemIdx(idx) : nullptr; }
static int dd_GetItemPos(CUIDragDropListEx* l, CUICellItem* c)
{
    for (u32 i = 0, n = l->ItemsCount(); i < n; ++i)
        if (l->GetItemIdx(i) == c)
            return int(i);
    return -1;
}
static CUICellItem* dd_GetCellAt(CUIDragDropListEx*, Fvector2) { return nullptr; }
static CUICellItem* dd_GetCellByCellId(CUIDragDropListEx* l, LPCSTR id)
{
    for (u32 i = 0, n = l->ItemsCount(); i < n; ++i)
    {
        CUICellItem* c = l->GetItemIdx(i);
        auto it = g_cell_ids.find(c);
        if (it != g_cell_ids.end() && it->second == id)
            return c;
    }
    return nullptr;
}
static void dd_set_cb3(CUIDragDropListEx* l, int type, const luabind::functor<bool>& f, const luabind::object& o)
{
    if (auto s = script_list(l))
        s->set_cb(type, f, o);
}
static void dd_set_cb2(CUIDragDropListEx* l, int type, const luabind::functor<bool>& f)
{
    if (auto s = script_list(l))
        s->set_cb(type, f);
}
static void dd_set_cb1(CUIDragDropListEx* l, int type)
{
    if (auto s = script_list(l))
        s->reset_cb(type);
}

// --- CUICellItem
static u32 ci_ChildsCount(CUICellItem* c) { return c->ChildsCount(); }
static CUICellItem* ci_Child(CUICellItem* c, u32 i) { return i < c->ChildsCount() ? c->Child(i) : nullptr; }
static bool ci_HasChilds(CUICellItem* c) { return c->ChildsCount() > 0; }
static void ci_SetCellId(CUICellItem* c, LPCSTR id) { g_cell_ids[c] = id; }
static LPCSTR ci_GetCellId(CUICellItem* c)
{
    auto it = g_cell_ids.find(c);
    return it != g_cell_ids.end() ? it->second.c_str() : "";
}
static CUIDragDropListEx* ci_OwnerList(CUICellItem* c) { return c->OwnerList(); }
static void ci_SetFocused(CUICellItem*, bool) {}
static void ci_SetSelected(CUICellItem* c, bool b) { c->m_selected = b; }
static CInventoryItem* ci_GetInventoryItem(CUICellItem* c) { return static_cast<CInventoryItem*>(c->m_pData); }
static LPCSTR ci_GetCellSection(CUICellItem* c)
{
    if (auto s = smart_cast<CUIOP22SectionCell*>(c))
        return s->m_section.c_str();
    if (auto itm = static_cast<CInventoryItem*>(c->m_pData))
        return itm->object().cNameSect().c_str();
    return "";
}
static void ci_SetCellCondition(CUICellItem* c, float v)
{
    if (auto s = smart_cast<CUIOP22SectionCell*>(c))
        s->m_condition = v;
}
static float ci_GetCellCondition(CUICellItem* c)
{
    if (auto s = smart_cast<CUIOP22SectionCell*>(c))
        return s->m_condition;
    if (auto itm = static_cast<CInventoryItem*>(c->m_pData))
        return itm->GetCondition();
    return 1.f;
}
static void ci_noop(CUICellItem*) {}
static void ci_SetColor(CUICellItem* c, u32 clr) { c->SetTextureColor(clr); }
static u32 ci_GetColor(CUICellItem* c) { return c->GetTextureColor(); }
static void ci_SetMask(CUICellItem*, LPCSTR) {}
static void ci_AttachAccum(CUICellItem*, LPCSTR, float) {}
static void ci_SetChildsCount(CUICellItem*, int) {}

// --- create_cell_helper
static CUICellItem* cch_item_section(LPCSTR section) { return xr_new<CUIOP22SectionCell>(section, 1.f); }
static CUICellItem* cch_item_section_condition(LPCSTR section, float cond) { return xr_new<CUIOP22SectionCell>(section, cond); }


////////////////////////////////////////////////////////////////////////////
// OP22: колбэки окон инвентаря ОП 2.2 - функции из [maingame_ui] system.ltx (callback_ui.*). Ищутся при каждом
// вызове (ссылка на функцию стала бы висячей после перезапуска скриптов при загрузке сейва).
////////////////////////////////////////////////////////////////////////////
template <typename R>
static bool op22_ui_func(LPCSTR key, luabind::functor<R>& f)
{
    if (!pSettings->line_exist("maingame_ui", key))
        return false;
    LPCSTR name = pSettings->r_string("maingame_ui", key);
    if (!ai().script_engine().functor(name, f))
    {
        Msg("! ERROR function [%s] not exist for %s callback", name, key);
        return false;
    }
    return true;
}

// item_place_change ОП: REMOVE_FROM_RUCK 0, REMOVE_FROM_SLOT 1, REMOVE_FROM_BELT 2, PUT_TO_SLOT 3, PUT_TO_BELT 4, PUT_TO_RUCK 5
void op22_item_place_change(CGameObject* owner, CInventoryItem* item, int prev_place, int new_place)
{
    if (!owner || !item)
        return;
    luabind::functor<void> f;
    if (!op22_ui_func("on_item_place_change", f))
        return;
    auto call = [&](int t) { f(owner->lua_game_object(), item->object().lua_game_object(), t); };
    switch (prev_place)
    {
    case eItemPlaceRuck: call(0); break;
    case eItemPlaceSlot: call(1); break;
    case eItemPlaceBelt:
    case eItemPlaceBeltActor: call(2); break;
    default: break;
    }
    switch (new_place)
    {
    case eItemPlaceSlot: call(3); break;
    case eItemPlaceBelt:
    case eItemPlaceBeltActor: call(4); break;
    case eItemPlaceRuck: call(5); break;
    default: break;
    }
}

static CUICellItem* op22_cell_under_cursor(CUIDragDropListEx* l, CUICellItem* except)
{
    if (!l)
        return nullptr;
    for (u32 i = 0, n = l->ItemsCount(); i < n; ++i)
    {
        CUICellItem* c = l->GetItemIdx(i);
        if (c && c != except && c->CursorOverWindow())
            return c;
    }
    return nullptr;
}

// бросок в окне инвентаря: как в ОП (Ghidra, CUIInventoryWnd::OnItemDrop) - предмет на предмет в рюкзаке надевает аддон
// на оружие под курсором; затем callback_ui.on_cell_item_drop(окно, откуда, куда, перетаскиваемая, под курсором, обработано)
bool CUIInventoryWnd::OnItemDropOP22(CUICellItem* itm)
{
    CUIDragDropListEx* from = itm->OwnerList();
    CUIDragDropListEx* to = CUIDragDropListEx::m_drag_item ? CUIDragDropListEx::m_drag_item->BackList() : nullptr;
    CUICellItem* focused = op22_cell_under_cursor(to, itm);
    bool processed = false;
    // зарядка: патроны брошены на ствол, к которому подходят (в любом списке окна)
    if (focused && focused != itm)
    {
        auto ammo = smart_cast<CWeaponAmmo*>(static_cast<PIItem>(itm->m_pData));
        auto wpn = smart_cast<CWeaponMagazined*>(static_cast<PIItem>(focused->m_pData));
        if (ammo && wpn && wpn->op22_LoadAmmoFromUI(ammo->cNameSect()))
        {
            PlaySnd(eInvItemToSlot);
            InitInventory_delayed();
            processed = true;
        }
    }
    if (!processed && from && from == to && focused && GetType(to) == iwBag)
    {
        PIItem addon = static_cast<PIItem>(itm->m_pData);
        PIItem target = static_cast<PIItem>(focused->m_pData);
        if (addon && target && target->CanAttach(addon))
        {
            SetCurrentItem(itm);
            AttachAddon(target);
            processed = true;
        }
    }
    const bool res = processed ? true : OnItemDrop(itm);
    luabind::functor<void> f;
    if (op22_ui_func("on_cell_item_drop", f))
        f(this, from, to, itm, focused, processed);
    return res;
}

bool CUIInventoryWnd::OnItemSelectedOP22(CUICellItem* itm)
{
    const bool res = OnItemSelected(itm);
    luabind::functor<void> f;
    if (op22_ui_func("on_cell_after_select", f))
        f(this, itm, false);
    return res;
}

bool CUICarBodyWnd::OnItemSelectedOP22(CUICellItem* itm)
{
    const bool res = OnItemSelected(itm);
    luabind::functor<void> f;
    if (op22_ui_func("on_cell_after_select", f))
        f(this, itm, false);
    return res;
}

bool CUITradeWnd::OnItemSelectedOP22(CUICellItem* itm)
{
    const bool res = OnItemSelected(itm);
    luabind::functor<void> f;
    if (op22_ui_func("on_cell_after_select", f))
        f(this, itm, false);
    return res;
}

void CUIInventoryWnd::op22_PlaySnd(int a)
{
    if (a >= 0 && a < eInvSndMax)
        PlaySnd(eInventorySndAction(a));
}

// OP22: подсветка подходящих (callback_ui.on_cell_after_select ОП): ячейки с предметом нужной секции получают
// подсветку OGSR (m_select_armament - ею движок отмечает патроны к стволу); сбрасывается при следующем выборе
static void op22_collect_lists(CUIWindow* w, xr_vector<CUIDragDropListEx*>& out)
{
    for (CUIWindow* c : w->GetChildWndList())
    {
        if (auto l = smart_cast<CUIDragDropListEx*>(c))
            out.push_back(l);
        op22_collect_lists(c, out);
    }
}
static void op22_mark(CUIDragDropListEx* l, const xr_vector<shared_str>* sects, bool on)
{
    if (!l)
        return;
    for (u32 i = 0, n = l->ItemsCount(); i < n; ++i)
    {
        CUICellItem* c = l->GetItemIdx(i);
        auto it = static_cast<PIItem>(c->m_pData);
        if (!sects)
            c->m_select_armament = on;
        else if (it && std::find(sects->begin(), sects->end(), it->object().cNameSect()) != sects->end())
            c->m_select_armament = on;
    }
}
static void op22_sects(const luabind::object& o, xr_vector<shared_str>& out)
{
    if (o.type() == LUA_TSTRING)
    {
        out.emplace_back(luabind::object_cast<LPCSTR>(o));
        return;
    }
    if (o.type() != LUA_TTABLE)
        return;
    for (auto it = o.begin(); it != o.end(); ++it)
    {
        luabind::object v = *it;
        if (v.type() == LUA_TSTRING)
            out.emplace_back(luabind::object_cast<LPCSTR>(v));
    }
}
// список по типу ui_iw_list_types: в инвентаре - слот (номер слота ОП), 20 рюкзак, 21 пояс; иначе - все списки окна
static void op22_lists_of(CUIWindow* w, int type, xr_vector<CUIDragDropListEx*>& out)
{
    if (auto inv = smart_cast<CUIInventoryWnd*>(w); inv && type >= 0)
    {
        if (type < SLOTS_TOTAL)
            out.push_back(inv->GetSlotList(u8(type)));
        else if (type == 20)
            out.push_back(inv->op22_bag());
        else if (type == 21)
            out.push_back(inv->op22_belt());
        if (!out.empty())
            return;
    }
    op22_collect_lists(w, out);
}
template <class W>
static void wnd_clear_all(W* w)
{
    xr_vector<CUIDragDropListEx*> ls;
    op22_collect_lists(w, ls);
    for (auto l : ls)
        op22_mark(l, nullptr, false);
}

// методы окон ОП для скриптов
template <class W, int WT>
static int wnd_type(W*) { return WT; }
template <class W>
static void wnd_noop(W*) {}
template <class W>
static void wnd_play_none(W*, int) {}
static void wnd_inv_play(CUIInventoryWnd* w, int a) { w->op22_PlaySnd(a); }
template <class W>
static void wnd_suit_in2(W* w, int type, const luabind::object& o)
{
    xr_vector<shared_str> s;
    op22_sects(o, s);
    xr_vector<CUIDragDropListEx*> ls;
    op22_lists_of(w, type, ls);
    for (auto l : ls)
        op22_mark(l, &s, true);
}
template <class W>
static void wnd_suit_in1(W* w, int type, LPCSTR sect)
{
    xr_vector<shared_str> s{shared_str(sect)};
    xr_vector<CUIDragDropListEx*> ls;
    op22_lists_of(w, type, ls);
    for (auto l : ls)
        op22_mark(l, &s, true);
}
template <class W>
static void wnd_suit1(W* w, LPCSTR sect) { wnd_suit_in1(w, -1, sect); }
template <class W>
static void wnd_suit2(W* w, const luabind::object& o) { wnd_suit_in2(w, -1, o); }
template <class W>
static void wnd_clear_in(W* w, int type)
{
    xr_vector<CUIDragDropListEx*> ls;
    op22_lists_of(w, type, ls);
    for (auto l : ls)
        op22_mark(l, nullptr, false);
}

struct op22_enum_ui_windows_types {};
struct op22_enum_ui_iw_list_types {};
struct op22_enum_item_place_change {};

static void op22_register_windows(lua_State* L)
{
    module(L)[class_<CUIInventoryWnd, CUIDialogWnd>("CUIInventoryWnd")
                  .def("GetUIWindowType", &wnd_type<CUIInventoryWnd, 0>)
                  .def("PlaySnd", &wnd_inv_play)
                  .def("re_init", &wnd_noop<CUIInventoryWnd>)
                  .def("ClearAllSuitables", &wnd_clear_all<CUIInventoryWnd>)
                  .def("ClearSuitablesInList", &wnd_clear_in<CUIInventoryWnd>)
                  .def("SetSuitableBySection", &wnd_suit1<CUIInventoryWnd>)
                  .def("SetSuitableBySection", &wnd_suit2<CUIInventoryWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in1<CUIInventoryWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in2<CUIInventoryWnd>),
              class_<CUITradeWnd, CUIWindow>("CUITradeWnd")
                  .def("GetUIWindowType", &wnd_type<CUITradeWnd, 1>)
                  .def("PlaySnd", &wnd_play_none<CUITradeWnd>)
                  .def("re_init", &wnd_noop<CUITradeWnd>)
                  .def("ClearAllSuitables", &wnd_clear_all<CUITradeWnd>)
                  .def("ClearSuitablesInList", &wnd_clear_in<CUITradeWnd>)
                  .def("SetSuitableBySection", &wnd_suit1<CUITradeWnd>)
                  .def("SetSuitableBySection", &wnd_suit2<CUITradeWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in1<CUITradeWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in2<CUITradeWnd>),
              class_<CUICarBodyWnd, CUIDialogWnd>("CUICarBodyWnd")
                  .def("GetUIWindowType", &wnd_type<CUICarBodyWnd, 2>)
                  .def("PlaySnd", &wnd_play_none<CUICarBodyWnd>)
                  .def("re_init", &wnd_noop<CUICarBodyWnd>)
                  .def("ClearAllSuitables", &wnd_clear_all<CUICarBodyWnd>)
                  .def("ClearSuitablesInList", &wnd_clear_in<CUICarBodyWnd>)
                  .def("SetSuitableBySection", &wnd_suit1<CUICarBodyWnd>)
                  .def("SetSuitableBySection", &wnd_suit2<CUICarBodyWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in1<CUICarBodyWnd>)
                  .def("SetSuitableBySectionInList", &wnd_suit_in2<CUICarBodyWnd>),
              // значения - из регистрации xrGame.dll ОП (Ghidra 03.11)
              class_<op22_enum_ui_windows_types>("ui_windows_types")
                  .enum_("ui_windows_types")[(value("WT_UNKNOWN", -1), value("WT_INVENTORY", 0), value("WT_TRADE", 1), value("WT_CARBODY", 2))],
              class_<op22_enum_ui_iw_list_types>("ui_iw_list_types")
                  .enum_("ui_iw_list_types")[(value("LT_UNKNOWN", -1), value("LT_KNIFE", 0), value("LT_PISTOL", 1), value("LT_RIFLE", 2),
                                              value("LT_GRENADE", 3), value("LT_APPARATUS", 4), value("LT_BOLT", 5), value("LT_OUTFIT", 6),
                                              value("LT_PDA", 7), value("LT_DETECTOR_ARTS", 8), value("LT_TORCH", 9),
                                              value("LT_DETECTOR_ANOMS", 11), value("LT_PNV", 12), value("LT_SHOTGUN", 13),
                                              value("LT_BIODEV", 14), value("LT_BAG", 20), value("LT_BELT", 21), value("LT_TRADE_OUR_BAG", 22),
                                              value("LT_TRADE_OTHER_BAG", 23), value("LT_TRADE_OUR_TRADE", 24), value("LT_TRADE_OTHER_TRADE", 25),
                                              value("LT_CARBODY_OUR_TRADE", 26), value("LT_CARBODY_OTHER_BAG", 27))],
              class_<op22_enum_item_place_change>("item_place_change")
                  .enum_("item_place_change")[(value("REMOVE_FROM_RUCK", 0), value("REMOVE_FROM_SLOT", 1), value("REMOVE_FROM_BELT", 2),
                                               value("PUT_TO_SLOT", 3), value("PUT_TO_BELT", 4), value("PUT_TO_RUCK", 5))]];
}


////////////////////////////////////////////////////////////////////////////
// OP22: меню предмета ОП 2.2 - callback_ui.show_prop_box(окно, меню, предмет) убирает пункты (RemoveItemByTAG) и
// добавляет свои (InsertItem(позиция, текст, функция)); клик по своему пункту зовёт функция(окно, предмет)
////////////////////////////////////////////////////////////////////////////
constexpr u32 OP22_PROP_TAG_BASE = 0x4F500000;
// OP22: не статический объект: его деструктор шёл после закрытия Lua (вылет при выходе в lua_rawgeti)
static xr_vector<luabind::functor<void>>& g_prop_funcs = *xr_new<xr_vector<luabind::functor<void>>>();
static CUIInventoryWnd* g_prop_wnd = nullptr;

static void pb_InsertItem(CUIPropertiesBox* box, int pos, LPCSTR str, const luabind::functor<void>& f)
{
    g_prop_funcs.push_back(f);
    box->AddItem(str, nullptr, OP22_PROP_TAG_BASE + u32(g_prop_funcs.size() - 1));
    box->op22_MoveLastTo(pos); // позиция с 1 (callback_ui ОП)
}
static void pb_RemoveItemByTAG(CUIPropertiesBox* box, u32 tag)
{
    if (box->op22_has_tag(tag))
        box->RemoveItemByTAG(tag);
}

bool op22_show_prop_box(CUIInventoryWnd* wnd, CUIPropertiesBox* box, PIItem item)
{
    g_prop_funcs.clear();
    g_prop_wnd = wnd;
    if (!item)
        return false;
    luabind::functor<bool> f;
    if (!op22_ui_func("on_ui_show_prop_box", f))
        return false;
    const u32 before = box->GetItemsCount();
    f(wnd, box, item->object().lua_game_object());
    if (luabind::functor<void> exo; ai().script_engine().functor("op22_exo.show_prop_box", exo)) // OP22: снятие аккумулятора
        exo(wnd, box, item->object().lua_game_object());
    return !g_prop_funcs.empty() || box->GetItemsCount() != before;
}

void op22_prop_funcs_clear() { g_prop_funcs.clear(); }

bool op22_prop_box_clicked(CUIInventoryWnd* wnd, u32 tag, PIItem item)
{
    if (tag < OP22_PROP_TAG_BASE || tag - OP22_PROP_TAG_BASE >= g_prop_funcs.size())
        return false;
    if (item)
        g_prop_funcs[tag - OP22_PROP_TAG_BASE](wnd, item->object().lua_game_object());
    return true;
}

// методы ОП у CUIPropertiesBox регистрируются в UIPropertiesBox_script.cpp (класс уже там)
void op22_pb_InsertItem(CUIPropertiesBox* box, int pos, LPCSTR str, const luabind::functor<void>& f) { pb_InsertItem(box, pos, str, f); }
void op22_pb_RemoveItemByTAG(CUIPropertiesBox* box, u32 tag) { pb_RemoveItemByTAG(box, tag); }

void op22_register_dragdrop(lua_State* L)
{
    op22_register_windows(L);
    module(L)[class_<CUIDragDropListEx, CUIWindow>("CUIDragDropListEx")
                  .def("GetUIListId", &dd_GetUIListId)
                  .def("SetUIListId", &dd_SetUIListId)
                  .def("SetCellHeight", &dd_SetCellHeight)
                  .def("SetCellWidth", &dd_SetCellWidth)
                  .def("SetCellSize", &dd_SetCellSize1)
                  .def("SetCellSize", &dd_SetCellSize2)
                  .def("SetCellsRows", &dd_SetCellsRows)
                  .def("SetCellsColumns", &dd_SetCellsColumns)
                  .def("SetCellsCapacity", &dd_SetCellsCapacity1)
                  .def("SetCellsCapacity", &dd_SetCellsCapacity2)
                  .def("ItemsCount", &dd_ItemsCount)
                  .def("SetItem", &dd_SetItemObj)
                  .def("SetItem", &dd_SetItemObj2)
                  .def("SetItem", &dd_SetItemCell)
                  .def("Compact", &dd_Compact)
                  .def("ClearAll", &dd_ClearAll)
                  .def("HasFreeSpace", &dd_HasFreeSpace)
                  .def("FindSimilar", &dd_FindSimilar)
                  .def("RemoveItem", &dd_RemoveItem)
                  .def("ClearAllSuitables", &dd_noop)
                  .def("ClearAllHighLighted", &dd_noop)
                  .def("SetSuitableBySection", &dd_SetSuitable1)
                  .def("SetSuitableBySection", &dd_SetSuitable2)
                  .def("GetSelectedCell", &dd_GetSelectedCell)
                  .def("GetItemIdx", &dd_GetItemIdx)
                  .def("GetItemPos", &dd_GetItemPos)
                  .def("GetCellAt", &dd_GetCellAt)
                  .def("GetCellByCellId", &dd_GetCellByCellId)
                  .def("set_callback", &dd_set_cb3)
                  .def("set_callback", &dd_set_cb2)
                  .def("set_callback", &dd_set_cb1),

              class_<CUICellItem, CUIStatic>("CUICellItem")
                  .def("ChildsCount", &ci_ChildsCount)
                  .def("Child", &ci_Child)
                  .def("HasChilds", &ci_HasChilds)
                  .def("SetCellId", &ci_SetCellId)
                  .def("GetCellId", &ci_GetCellId)
                  .def("OwnerList", &ci_OwnerList)
                  .def("SetFocused", &ci_SetFocused)
                  .def("SetSelected", &ci_SetSelected)
                  .def("GetInventoryItem", &ci_GetInventoryItem)
                  .def("GetCellSection", &ci_GetCellSection)
                  .def("SetCellCondition", &ci_SetCellCondition)
                  .def("GetCellCondition", &ci_GetCellCondition)
                  .def("ForceConditionShow", &ci_noop)
                  .def("ForceConditionHide", &ci_noop)
                  .def("SetColor", &ci_SetColor)
                  .def("GetColor", &ci_GetColor)
                  .def("SetMask", &ci_SetMask)
                  .def("AttachScope", &ci_noop)
                  .def("DetachScope", &ci_noop)
                  .def("AttachSilencer", &ci_noop)
                  .def("DetachSilencer", &ci_noop)
                  .def("AttachGrenadeLauncher", &ci_noop)
                  .def("DetachGrenadeLauncher", &ci_noop)
                  .def("AttachAccum", &ci_AttachAccum)
                  .def("DetachAccum", &ci_noop)
                  .def("SetChildsCount", &ci_SetChildsCount)];

    module(L, "create_cell_helper")[def("item_section", &cch_item_section), def("item_section_condition", &cch_item_section_condition)];
}

#pragma once
#include "UIStatic.h"
#include "../UIStaticItem.h"

class CIconParams
{
    bool loaded{};

public:
    int icon_group{};
    float grid_x{}, grid_y{}, grid_width{}, grid_height{};
    bool show_condition{};
    int op22_ui_show_condition{-1}; // OP22: ключ ОП ui_show_condition, -1 - нет ключа
    shared_str name{}, section{};

    CIconParams() = default;
    CIconParams(const shared_str& section) { Load(section); }

    void Load(const shared_str& item_section);
    ui_shader& get_shader() const;
    Frect& original_rect() const;
    void set_shader(CUIStatic*);
    void set_shader(CUIStaticItem*);
};

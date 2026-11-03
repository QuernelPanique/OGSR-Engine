// OP22: скины интерфейса ОП 2.2 (CMultiUIConfigs движка ОП): HUD и инвентарь.
// Профили - подпапки $game_huds$ (skins_ui\huds) и $game_ui_inventory$ (skins_ui\inventory), по умолчанию -
// [default_profile] path из default_profile.ltx; текущий задают g_current_hud / g_current_inventory (xrGame).
// Файлы профиля подменяют config\ui\<файл> (xml окон и описаний текстур) и textures\<имя>.dds; инвентарь главнее HUD.
#pragma once

namespace op22_skins
{
enum EKind
{
    eHud = 0,
    eInventory = 1,
};
XRCORE_API LPCSTR alias(int kind);
XRCORE_API void set_current(int kind, LPCSTR name); // пустое - профиль по умолчанию
XRCORE_API LPCSTR current(int kind);
// config\ui\<file>: найдено в текущем профиле - alias и путь относительно него
XRCORE_API bool find_config_ui(LPCSTR file, LPCSTR& alias_out, string_path& rel_out);
// textures\<name>.dds (name без расширения); op22skin\<hud|inv>\<профиль>\<файл> - превью профиля
XRCORE_API bool find_texture(LPCSTR name, LPCSTR& alias_out, string_path& rel_out);
XRCORE_API void profiles(int kind, xr_vector<shared_str>& out);
XRCORE_API bool read_text(int kind, LPCSTR profile, LPCSTR file, xr_string& out); // description.txt профиля
} // namespace op22_skins

// OP22: скины интерфейса ОП 2.2 - см. op22_skins.h
#include "stdafx.h"
#include "op22_skins.h"

namespace op22_skins
{
static const char* g_alias[2] = {"$game_huds$", "$game_ui_inventory$"};
static xr_string g_cur[2];
static xr_string g_def[2];
static bool g_def_loaded[2]{};

LPCSTR alias(int k) { return g_alias[k & 1]; }

static bool has_alias(int k) { return FS.path_exist(g_alias[k]); }

static const xr_string& default_profile(int k)
{
    if (!g_def_loaded[k])
    {
        g_def_loaded[k] = true;
        string_path p;
        if (has_alias(k) && FS.exist(p, g_alias[k], "default_profile.ltx"))
        {
            CInifile ini(p);
            if (ini.section_exist("default_profile") && ini.line_exist("default_profile", "path"))
                g_def[k] = ini.r_string("default_profile", "path");
        }
    }
    return g_def[k];
}

void set_current(int k, LPCSTR name) { g_cur[k & 1] = name ? name : ""; }

void profiles(int k, xr_vector<shared_str>& out);

LPCSTR current(int k)
{
    k &= 1;
    if (!g_cur[k].empty())
    {
        // такого профиля нет - профиль по умолчанию (как в ОП)
        xr_vector<shared_str> all;
        profiles(k, all);
        for (const auto& n : all)
            if (!_stricmp(n.c_str(), g_cur[k].c_str()))
                return g_cur[k].c_str();
    }
    return default_profile(k).c_str();
}

static bool find_in(int k, LPCSTR rel, string_path& out_rel)
{
    LPCSTR c = current(k);
    if (!c || !*c || !has_alias(k))
        return false;
    xr_sprintf(out_rel, "%s\\%s", c, rel);
    string_path full;
    return !!FS.exist(full, g_alias[k], out_rel);
}

bool find_config_ui(LPCSTR file, LPCSTR& alias_out, string_path& rel_out)
{
    string_path r;
    xr_sprintf(r, "config\\ui\\%s", file);
    for (int k : {eInventory, eHud})
        if (find_in(k, r, rel_out))
        {
            alias_out = g_alias[k];
            return true;
        }
    return false;
}

bool find_texture(LPCSTR name, LPCSTR& alias_out, string_path& rel_out)
{
    if (!name || !*name)
        return false;
    static const char pfx[] = "op22skin\\";
    if (strstr(name, pfx) == name)
    {
        LPCSTR rest = name + sizeof(pfx) - 1; // hud\<профиль>\<файл> | inv\...
        const int k = (strstr(rest, "inv\\") == rest) ? eInventory : eHud;
        LPCSTR sub = strchr(rest, '\\');
        if (!sub || !has_alias(k))
            return false;
        xr_sprintf(rel_out, "%s.dds", sub + 1);
        string_path full;
        if (!FS.exist(full, g_alias[k], rel_out))
            return false;
        alias_out = g_alias[k];
        return true;
    }
    string_path r;
    xr_sprintf(r, "textures\\%s.dds", name);
    for (int k : {eInventory, eHud})
        if (find_in(k, r, rel_out))
        {
            alias_out = g_alias[k];
            return true;
        }
    return false;
}

void profiles(int k, xr_vector<shared_str>& out)
{
    out.clear();
    k &= 1;
    if (!has_alias(k))
        return;
    FS_FileSet fs;
    FS.file_list(fs, g_alias[k], FS_ListFolders | FS_RootOnly);
    for (const auto& f : fs)
    {
        xr_string n = f.name;
        while (!n.empty() && (n.back() == '\\' || n.back() == '/'))
            n.pop_back();
        if (!n.empty())
            out.emplace_back(n.c_str());
    }
}

bool read_text(int k, LPCSTR profile, LPCSTR file, xr_string& out)
{
    k &= 1;
    if (!has_alias(k))
        return false;
    string_path r;
    xr_sprintf(r, "%s\\%s", profile, file);
    IReader* F = FS.r_open(g_alias[k], r);
    if (!F)
        return false;
    out.assign((const char*)F->pointer(), F->length());
    FS.r_close(F);
    return true;
}
} // namespace op22_skins

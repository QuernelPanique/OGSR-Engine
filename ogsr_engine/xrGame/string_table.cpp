#include "stdafx.h"
#include "string_table.h"

#include "ui/xrUIXmlParser.h"
#include "xr_level_controller.h"
#include "MainMenu.h"

STRING_TABLE_DATA* CStringTable::pData{};
BOOL CStringTable::WriteErrorsToLog{};
extern const xr_token* GetLanguagesToken();

CStringTable::CStringTable() { Init(); }

void CStringTable::Destroy() { xr_delete(pData); }

shared_str CStringTable::GetLanguage() { return pData->m_sLanguage; }

void CStringTable::Init()
{
    if (pData)
        return;

    pData = xr_new<STRING_TABLE_DATA>();

    //имя языка, если не задано (NULL), то первый <text> в <string> в XML
    SetLanguage();

    LPCSTR S = pSettings->r_string("string_table", "files");
    if (S && S[0])
    {
        string128 xml_file;
        int count = _GetItemCount(S);
        for (int it = 0; it < count; ++it)
        {
            _GetItem(S, it, xml_file);
            Load(xml_file);
        }
    }

    if (pSettings->section_exist("string_table_files"))
    {
        CInifile::Sect& files = pSettings->r_section("string_table_files");
        for (const auto& i : files.Ordered_Data)
            Load(i.first.c_str());
    }
}

// OP22: тексты ОП 2.2 в UTF-8, шрифты - cp1251 (движок ОП перекодировал при загрузке). Явно в 1251, без
// системной локали: под Wine она может быть не русской.
static xr_string op22_utf8_to_1251(const char* s)
{
    const int wn = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (wn <= 0)
        return s;
    std::wstring w(wn, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), wn);
    const int n = WideCharToMultiByte(1251, 0, w.c_str(), -1, nullptr, 0, "?", nullptr);
    if (n <= 0)
        return s;
    xr_string r(n, '\0');
    WideCharToMultiByte(1251, 0, w.c_str(), -1, r.data(), n, "?", nullptr);
    r.resize(n - 1);
    return r;
}

void CStringTable::Load(LPCSTR xml_file)
{
    CUIXml uiXml;
    string_path xml_file_full;
    strconcat(sizeof(xml_file_full), xml_file_full, xml_file, ".xml");
    string_path _s;
    // OP22: язык не задан ([string_table] language пуст, как в ОП 2.2) - тексты лежат прямо в config\text
    if (pData->m_sLanguage.size())
        strconcat(sizeof(_s), _s, STRING_TABLE_PATH, "\\", *(pData->m_sLanguage));
    else
        xr_strcpy(_s, STRING_TABLE_PATH);

    bool xml_result = uiXml.Init(CONFIG_PATH, _s, xml_file_full);
    if (!xml_result)
        Debug.fatal(DEBUG_INFO, "string table xml file not found %s, for language %s", xml_file_full, *(pData->m_sLanguage));

    //общий список всех записей таблицы в файле
    int string_num = uiXml.GetNodesNum(uiXml.GetRoot(), "string");

    for (int i = 0; i < string_num; ++i)
    {
        LPCSTR string_name = uiXml.ReadAttrib(uiXml.GetRoot(), "string", i, "id", NULL);

        if (WriteErrorsToLog && pData->m_StringTable.contains(string_name))
            Msg("!![%s] duplicate string table id: [%s]", __FUNCTION__, string_name);

        LPCSTR string_text = uiXml.Read(uiXml.GetRoot(), "string:text", i, NULL);

        // OP22: в ОП 2.2 вместо <text> теги языков (<rus>, <eng>, <ukr>, <pln>, <fra>); язык - [language_default] default
        if (!string_text)
        {
            static string32 op_lang_path{};
            if (!op_lang_path[0])
            {
                LPCSTR lang = READ_IF_EXISTS(pSettings, r_string, "language_default", "default", "rus");
                xr_sprintf(op_lang_path, "string:%s", (lang && lang[0]) ? lang : "rus");
                Msg("~ [string_table] OP 2.2 language tags, using [%s]", op_lang_path);
            }
            string_text = uiXml.Read(uiXml.GetRoot(), op_lang_path, i, NULL);
            if (!string_text)
                string_text = uiXml.Read(uiXml.GetRoot(), "string:rus", i, NULL);
            if (!string_text)
                string_text = uiXml.Read(uiXml.GetRoot(), "string:eng", i, NULL);
            if (!string_text)
                string_text = ""; // пустая строка вместо вылета в ParseLine
        }

        VERIFY3(string_text, "string table entry does not has a text", string_name);

        xr_string op22_conv;
        if (string_text[0] && StringHasUTF8(string_text))
        {
            op22_conv = op22_utf8_to_1251(string_text);
            string_text = op22_conv.c_str();
        }

        STRING_VALUE str_val = ParseLine(string_text, string_name, true);

        pData->m_StringTable[string_name] = str_val;
    }
}

void CStringTable::ReloadLanguage()
{
    const char* lang = GetLanguagesToken()->name; // OP22: может быть nullptr, если язык не задан
    if (!strcmp(lang ? lang : "", pData->m_sLanguage.size() ? pData->m_sLanguage.c_str() : ""))
        return;

    Destroy();
    Init();

    if (g_pGamePersistent && MainMenu() && MainMenu()->IsActive())
    {
        MainMenu()->Activate(FALSE);
        MainMenu()->Activate(TRUE);
    }
}

void CStringTable::SetLanguage()
{
    pData->m_sLanguage = GetLanguagesToken()->name; // OP22: nullptr -> пустая строка
    if (!pData->m_sLanguage.size())
        Msg("~ [string_table] language not set - texts from [config\\%s] (OP 2.2 layout)", STRING_TABLE_PATH);
}

void CStringTable::ReparseKeyBindings()
{
    if (!pData)
        return;

    for (const auto& [key, val] : pData->m_string_key_binding)
        pData->m_StringTable[key] = ParseLine(val.c_str(), key.c_str(), false);
}

STRING_VALUE CStringTable::ParseLine(LPCSTR str, LPCSTR skey, bool bFirst)
{
    //	LPCSTR str = "1 $$action_left$$ 2 $$action_right$$ 3 $$action_left$$ 4";
    xr_string res;
    int k = 0;
    const char* b;
#define ACTION_STR "$$ACTION_"

    //.	int LEN				= (int)xr_strlen(ACTION_STR);
#define LEN 9

    string256 buff;
    string256 srcbuff;
    bool b_hit = false;

    while ((b = strstr(str + k, ACTION_STR)) != 0)
    {
        buff[0] = 0;
        srcbuff[0] = 0;
        res.append(str + k, b - str - k);
        const char* e = strstr(b + LEN, "$$");

        int len = (int)(e - b - LEN);

        strncpy_s(srcbuff, b + LEN, len);
        srcbuff[len] = 0;
        GetActionAllBinding(srcbuff, buff, sizeof(buff));
        res.append(buff, xr_strlen(buff));

        k = (int)(b - str);
        k += len;
        k += LEN;
        k += 2;
        b_hit = true;
    };

    if (k < (int)xr_strlen(str))
    {
        res.append(str + k);
    }

    if (b_hit && bFirst)
        pData->m_string_key_binding[skey] = str;

    return STRING_VALUE(res.c_str());
}

STRING_VALUE CStringTable::translate(const STRING_ID& str_id) const
{
    VERIFY(pData);

    STRING_VALUE res = pData->m_StringTable[str_id];

    if (!res)
    {
        if (WriteErrorsToLog && *str_id != nullptr && xr_strlen(*str_id) > 0)
            Msg("!![%s] [%s] has no entry!", __FUNCTION__, *str_id);
        return str_id;
    }

    return res;
}

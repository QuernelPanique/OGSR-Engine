#include "stdafx.h"
#include "PhraseScript.h"
#include "script_engine.h"
#include "ai_space.h"
#include "gameobject.h"
#include "script_game_object.h"
#include "infoportion.h"
#include "inventoryowner.h"
#include "ai_debug.h"
#include "ui/xrUIXmlParser.h"
#include "actor.h"

// OP22 (28.11 perf): кэш выражений диалога, по умолчанию используем свежие замыкания.
int g_op22_dialog_chunk_cache = 0;

namespace
{
char dialog_chunk_cache_key;

luabind::object compile_dialog_chunk(LPCSTR code)
{
    auto& scripts = ai().script_engine();
    luabind::functor<luabind::object> loadstring;
    scripts.functor("loadstring", loadstring);
    if (!g_op22_dialog_chunk_cache)
        return loadstring(code);

    // OP22 (28.11 perf): используем кэш только для исходного loadstring и текущего окружения VM.
    auto* L = scripts.lua();
    const int top = lua_gettop(L);
    lua_pushlightuserdata(L, &dialog_chunk_cache_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    const int cache = top + 1;
    lua_rawgeti(L, cache, 1);
    loadstring.pushvalue();
    const bool original_loader = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    if (!original_loader)
    {
        lua_settop(L, top);
        return loadstring(code);
    }

    lua_pushstring(L, code);
    lua_rawget(L, cache);
    if (lua_isfunction(L, -1))
    {
        lua_getfenv(L, -1);
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        const bool current_environment = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (current_environment)
        {
            luabind::detail::lua_reference ref;
            ref.set(L);
            lua_settop(L, top);
            return luabind::object(L, ref, true);
        }
    }
    lua_pop(L, 1);

    luabind::object chunk = loadstring(code);
    if (chunk.is_valid() && chunk.type() == LUA_TFUNCTION)
    {
        lua_pushstring(L, code);
        chunk.pushvalue();
        lua_rawset(L, cache);
    }
    lua_settop(L, top);
    return chunk;
}
} // namespace

// OP22 (28.11 perf): ключи и чанки принадлежат реестру и исчезают при закрытии VM.
void op22_init_dialog_chunk_cache(lua_State* L)
{
    lua_pushlightuserdata(L, &dialog_chunk_cache_key);
    lua_createtable(L, 1, 0);
    lua_getfield(L, LUA_GLOBALSINDEX, "loadstring");
    lua_rawseti(L, -2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

CPhraseScript::CPhraseScript() {}
CPhraseScript::~CPhraseScript() {}

//загрузка из XML файла
void CPhraseScript::Load(CUIXml* uiXml, XML_NODE* phrase_node)
{
    m_sScriptTextFunc = uiXml->Read(phrase_node, "script_text", 0, "");

    LoadSequence(uiXml, phrase_node, "precondition", m_Preconditions);
    LoadSequence(uiXml, phrase_node, "action", m_ScriptActions);

    LoadSequence(uiXml, phrase_node, "has_info", m_HasInfo);
    LoadSequence(uiXml, phrase_node, "dont_has_info", m_DontHasInfo);

    LoadSequence(uiXml, phrase_node, "give_info", m_GiveInfo);
    LoadSequence(uiXml, phrase_node, "disable_info", m_DisableInfo);
}

template <class T>
void CPhraseScript::LoadSequence(CUIXml* uiXml, XML_NODE* phrase_node, LPCSTR tag, T& str_vector)
{
    int tag_num = uiXml->GetNodesNum(phrase_node, tag);
    str_vector.clear();
    for (int i = 0; i < tag_num; i++)
    {
        LPCSTR tag_text = uiXml->Read(phrase_node, tag, i, NULL);
        str_vector.push_back(tag_text);
    }
}

bool CPhraseScript::CheckInfo(const CInventoryOwner* pOwner) const
{
    THROW(pOwner);

    for (u32 i = 0; i < m_HasInfo.size(); i++)
    {
#pragma todo("Andy->Andy how to check infoportion existence in XML ?")
        /*		INFO_INDEX	result = CInfoPortion::IdToIndex(m_HasInfo[i],NO_INFO_INDEX,true);
                if (result == NO_INFO_INDEX) {
                    ai().script_engine().script_log(eLuaMessageTypeError,"XML item not found : \"%s\"",*m_HasInfo[i]);
                    break;
                }
        */
        //.		if (!pOwner->HasInfo(m_HasInfo[i])) {
        if (!Actor()->HasInfo(m_HasInfo[i]))
        {
#ifdef DEBUG
            if (psAI_Flags.test(aiDialogs))
                Msg("----rejected: [%s] has info %s", pOwner->Name(), *m_HasInfo[i]);
#endif
            return false;
        }
    }

    for (u32 i = 0; i < m_DontHasInfo.size(); i++)
    {
        /*		INFO_INDEX	result = CInfoPortion::IdToIndex(m_DontHasInfo[i],NO_INFO_INDEX,true);
                if (result == NO_INFO_INDEX) {
                    ai().script_engine().script_log(eLuaMessageTypeError,"XML item not found : \"%s\"",*m_DontHasInfo[i]);
                    break;
                }
        */
        //.		if (pOwner->HasInfo(m_DontHasInfo[i])) {
        if (Actor()->HasInfo(m_DontHasInfo[i]))
        {
#ifdef DEBUG
            if (psAI_Flags.test(aiDialogs))
                Msg("----rejected: [%s] dont has info %s", pOwner->Name(), *m_DontHasInfo[i]);
#endif
            return false;
        }
    }
    return true;
}

void CPhraseScript::TransferInfo(const CInventoryOwner* pOwner) const
{
    THROW(pOwner);

    for (u32 i = 0; i < m_GiveInfo.size(); i++)
        //.		pOwner->TransferInfo(m_GiveInfo[i], true);
        Actor()->TransferInfo(m_GiveInfo[i], true);

    for (u32 i = 0; i < m_DisableInfo.size(); i++)
        //.		pOwner->TransferInfo(m_DisableInfo[i],false);
        Actor()->TransferInfo(m_DisableInfo[i], false);
}

bool CPhraseScript::Precondition(const CGameObject* pSpeakerGO, LPCSTR dialog_id, LPCSTR phrase_id) const
{
    bool predicate_result = true;

    if (!CheckInfo(smart_cast<const CInventoryOwner*>(pSpeakerGO)))
    {
#ifdef DEBUG
        if (psAI_Flags.test(aiDialogs))
            Msg("dialog [%s] phrase[%s] rejected by CheckInfo", dialog_id, phrase_id);
#endif
        return false;
    }

    for (const auto& Cond : Preconditions())
    {
        xr_string ConditionString(Cond.c_str()); //-V808
        if (luabind::functor<bool> lua_function; ai().script_engine().functor(ConditionString.c_str(), lua_function)) // Обычный функтор
            predicate_result = lua_function(pSpeakerGO->lua_game_object());
        else
        { // Функтор с аргументами
            ConditionString = "return " + ConditionString;
            luabind::object ret_obj = compile_dialog_chunk(ConditionString.c_str()); // OP22 (28.11 perf): берём скомпилированное условие.
            auto ret_func = luabind::object_cast<luabind::functor<luabind::object>>(ret_obj); // OP22: nil-safe, see below // Первое возвращённое loadstring значение должно быть функцией
            ASSERT_FMT_DBG(ret_func, "Loadstring returns nil for code: %s", ConditionString.c_str()); // Если это не функция, значит loadstring вернул nil и что-то пошло не так
            // Вызываем созданную функцию и передаём ей дефолтные аргументы. Они прилетят после аргументов, прописанных явно, если например сделать так:
            // <precondition>my_script.test_func(123, true, nil, ...)</precondition>
            // А если не указать '...' - дефолтные аргументы не будут переданы в функцию.
            if (ret_func)
            {
                // OP22: missing script function gives nil (dead references of OP dialogs): phrase is hidden, no cast error
                luabind::object res = ret_func(pSpeakerGO->lua_game_object());
                if (!res.is_valid() || res.type() != LUA_TBOOLEAN)
                {
                    Msg("! [OP22] dialog [%s] phrase [%s]: precondition [%s] returned no boolean - phrase hidden", dialog_id, phrase_id, Cond.c_str());
                    predicate_result = false;
                }
                else
                    predicate_result = luabind::object_cast<bool>(res);
            }
        }

        if (!predicate_result)
        {
#ifdef DEBUG
            if (psAI_Flags.test(aiDialogs))
                Msg("dialog [%s] phrase[%s] rejected by script predicate", dialog_id, phrase_id);
#endif
            break;
        }
    }
    return predicate_result;
}

void CPhraseScript::Action(const CGameObject* pSpeakerGO, LPCSTR dialog_id, LPCSTR /*phrase_id*/) const
{
    for (const auto& Act : Actions())
    {
        xr_string ActionString(Act.c_str()); //-V808
        if (luabind::functor<void> lua_function; ai().script_engine().functor(ActionString.c_str(), lua_function)) // Обычный функтор
            lua_function(pSpeakerGO->lua_game_object(), dialog_id);
        else
        { // Функтор с аргументами
            ActionString = "return " + ActionString;
            luabind::object ret_obj = compile_dialog_chunk(ActionString.c_str()); // OP22 (28.11 perf): берём скомпилированное действие.
            auto ret_func = luabind::object_cast<luabind::functor<void>>(ret_obj); // Первое возвращённое loadstring значение должно быть функцией
            ASSERT_FMT_DBG(ret_func, "Loadstring returns nil for code: %s", ActionString.c_str()); // Если это не функция, значит loadstring вернул nil и что-то пошло не так
            // Вызываем созданную функцию и передаём ей дефолтные аргументы. Они прилетят после аргументов, прописанных явно, если например сделать так:
            // <action>my_script.test_func(123, true, nil, ...)</action>
            // А если не указать '...' - дефолтные аргументы не будут переданы в функцию.
            if (ret_func)
                ret_func(pSpeakerGO->lua_game_object(), dialog_id);
        }
    }

    TransferInfo(smart_cast<const CInventoryOwner*>(pSpeakerGO));
}

bool CPhraseScript::Precondition(const CGameObject* pSpeakerGO1, const CGameObject* pSpeakerGO2, LPCSTR dialog_id, LPCSTR phrase_id, LPCSTR next_phrase_id) const
{
    bool predicate_result = true;

    if (!CheckInfo(smart_cast<const CInventoryOwner*>(pSpeakerGO1)))
    {
#ifdef DEBUG
        if (psAI_Flags.test(aiDialogs))
            Msg("dialog [%s] phrase[%s] rejected by CheckInfo", dialog_id, phrase_id);
#endif
        return false;
    }

    for (const auto& Cond : Preconditions())
    {
        xr_string ConditionString(Cond.c_str()); //-V808
        if (luabind::functor<bool> lua_function; ai().script_engine().functor(ConditionString.c_str(), lua_function)) // Обычный функтор
            predicate_result = lua_function(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id, next_phrase_id);
        else
        { // Функтор с аргументами
            ConditionString = "return " + ConditionString;
            luabind::object ret_obj = compile_dialog_chunk(ConditionString.c_str()); // OP22 (28.11 perf): берём скомпилированное условие.
            auto ret_func = luabind::object_cast<luabind::functor<luabind::object>>(ret_obj); // OP22: nil-safe, see below // Первое возвращённое loadstring значение должно быть функцией
            ASSERT_FMT_DBG(ret_func, "Loadstring returns nil for code: %s", ConditionString.c_str()); // Если это не функция, значит loadstring вернул nil и что-то пошло не так
            // Вызываем созданную функцию и передаём ей дефолтные аргументы. Они прилетят после аргументов, прописанных явно, если например сделать так:
            // <precondition>my_script.test_func(123, true, nil, ...)</precondition>
            // А если не указать '...' - дефолтные аргументы не будут переданы в функцию.
            if (ret_func)
            {
                // OP22: missing script function gives nil (dead references of OP dialogs): phrase is hidden, no cast error
                luabind::object res = ret_func(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id, next_phrase_id);
                if (!res.is_valid() || res.type() != LUA_TBOOLEAN)
                {
                    Msg("! [OP22] dialog [%s] phrase [%s]: precondition [%s] returned no boolean - phrase hidden", dialog_id, phrase_id, Cond.c_str());
                    predicate_result = false;
                }
                else
                    predicate_result = luabind::object_cast<bool>(res);
            }
        }

        if (!predicate_result)
        {
#ifdef DEBUG
            if (psAI_Flags.test(aiDialogs))
                Msg("dialog [%s] phrase[%s] rejected by script predicate", dialog_id, phrase_id);
#endif
            break;
        }
    }
    return predicate_result;
}

void CPhraseScript::Action(const CGameObject* pSpeakerGO1, const CGameObject* pSpeakerGO2, LPCSTR dialog_id, LPCSTR phrase_id) const
{
    TransferInfo(smart_cast<const CInventoryOwner*>(pSpeakerGO1));

    for (const auto& Act : Actions())
    {
        xr_string ActionString(Act.c_str()); //-V808
        if (luabind::functor<void> lua_function; ai().script_engine().functor(ActionString.c_str(), lua_function)) // Обычный функтор
            lua_function(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id);
        else
        { // Функтор с аргументами
            ActionString = "return " + ActionString;
            luabind::object ret_obj = compile_dialog_chunk(ActionString.c_str()); // OP22 (28.11 perf): берём скомпилированное действие.
            auto ret_func = luabind::object_cast<luabind::functor<void>>(ret_obj); // Первое возвращённое loadstring значение должно быть функцией
            ASSERT_FMT_DBG(ret_func, "Loadstring returns nil for code: %s", ActionString.c_str()); // Если это не функция, значит loadstring вернул nil и что-то пошло не так
            // Вызываем созданную функцию и передаём ей дефолтные аргументы. Они прилетят после аргументов, прописанных явно, если например сделать так:
            // <action>my_script.test_func(123, true, nil, ...)</action>
            // А если не указать '...' - дефолтные аргументы не будут переданы в функцию.
            if (ret_func)
                ret_func(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id);
        }
    }
}

LPCSTR CPhraseScript::GetScriptText(LPCSTR str_to_translate, const CGameObject* pSpeakerGO1, const CGameObject* pSpeakerGO2, LPCSTR dialog_id, LPCSTR phrase_id)
{
    if (!m_sScriptTextFunc.size())
        return str_to_translate;

    xr_string ScriptTextString(m_sScriptTextFunc.c_str()); //-V808
    if (luabind::functor<const char*> lua_function; ai().script_engine().functor(ScriptTextString.c_str(), lua_function)) // Обычный функтор
        return lua_function(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id);
    else
    { // Функтор с аргументами
        ScriptTextString = "return " + ScriptTextString;
        luabind::object ret_obj = compile_dialog_chunk(ScriptTextString.c_str()); // OP22 (28.11 perf): берём скомпилированный чанк текста.
        auto ret_func = luabind::object_cast<luabind::functor<const char*>>(ret_obj); // Первое возвращённое loadstring значение должно быть функцией
        ASSERT_FMT_DBG(ret_func, "Loadstring returns nil for code: %s", ScriptTextString.c_str()); // Если это не функция, значит loadstring вернул nil и что-то пошло не так
        // Вызываем созданную функцию и передаём ей дефолтные аргументы. Они прилетят после аргументов, прописанных явно, если например сделать так:
        // <script_text>my_script.test_func(123, true, nil, ...)</script_text>
        // А если не указать '...' - дефолтные аргументы не будут переданы в функцию.
        if (ret_func)
            return ret_func(pSpeakerGO1->lua_game_object(), pSpeakerGO2->lua_game_object(), dialog_id, phrase_id);
    }

    return "";
}

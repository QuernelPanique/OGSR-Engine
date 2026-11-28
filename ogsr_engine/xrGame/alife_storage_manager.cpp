////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_storage_manager.cpp
//	Created 	: 25.12.2002
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator storage manager
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "op22_id32.h"

#include <zstd.h>

#include "alife_storage_manager.h"
#include "alife_simulator_header.h"
#include "alife_time_manager.h"
#include "alife_spawn_registry.h"
#include "alife_object_registry.h"
#include "alife_graph_registry.h"
#include "alife_group_registry.h"
#include "alife_registry_container.h"
#include "xrserver.h"
#include "level.h"

#include "../xr_3da/IGame_Persistent.h"

#include "saved_game_wrapper.h"
#include "script_vars_storage.h"
#include "ai_space.h"
#include "script_engine.h"

// OP22: хуки прослойки op22_compat.script (keyvals и очереди ОП живут в g_ScriptVars строкой)
void op22_compat_call(LPCSTR fn)
{
    luabind::functor<void> f;
    if (ai().script_engine().functor(fn, f))
        f();
}

// OP22: движок ОП 2.2 звал из _G колбэки этапов загрузки (_g.after_objs_load_callback создаёт контейнер
// keyvals, Cycle_Task, Treasure... - до регистрации объектов: se_stalker:on_register зовёт Cycle_Task)
void op22_before_register_objects()
{
    op22_compat_call("after_objs_load_callback");
    op22_compat_call("before_onregs_callback");
}

using namespace ALife;

extern string_path g_last_saved_game;

CALifeStorageManager::~CALifeStorageManager() {}

void CALifeStorageManager::save(LPCSTR save_name, bool update_name)
{
    string_path save;
    strcpy_s(save, m_save_name);
    if (save_name)
    {
        strconcat(sizeof(m_save_name), m_save_name, save_name, SAVE_EXTENSION);
    }
    else
    {
        if (!xr_strlen(m_save_name))
        {
            // OP22 (28.11 perf): отсутствие имени тоже считается ошибкой сохранения.
            Log("! save failed: no file name specified");
            return;
        }
    }

    u32 source_count;
    size_t dest_count;
    void* dest_data;
    {
        CMemoryWriter stream;
        stream.reserve(4 * 1024 * 1024);
        header().save(stream);
        time_manager().save(stream);
        spawns().save(stream);
        objects().save(stream);
        registry().save(stream);

        op22_compat_call("op22_compat.on_before_save");
        g_ScriptVars.save(stream);

        source_count = stream.tell();
        const void* source_data = stream.pointer();
        dest_count = ZSTD_compressBound(source_count);
        dest_data = xr_malloc(dest_count);
        dest_count = ZSTD_compress(dest_data, dest_count, source_data, source_count, 1);
        // OP22 (28.11 perf): при сбое сжатия сохраняем предыдущий слот и его имя.
        if (ZSTD_isError(dest_count))
        {
            Msg("! save failed: cannot compress '%s': %s", m_save_name, ZSTD_getErrorName(dest_count));
            xr_free(dest_data);
            strcpy_s(m_save_name, save);
            return;
        }
    }

    // OP22 (28.11 perf): публикуем сейв с прежним заголовком только после успешной записи.
    string_path file_name;
    FS.update_path(file_name, "$game_saves$", m_save_name);
    const u32 save_header[]{0, g_op22_alife_id32 ? u32(ALIFE_VERSION) : 10u, source_count};
    const bool saved = FS.w_save_atomic(file_name, save_header, sizeof(save_header), dest_data, dest_count);
    xr_free(dest_data);
    if (!saved)
    {
        strcpy_s(m_save_name, save);
        return;
    }

    if (save_name)
        strcpy_s(g_last_saved_game, save_name);
#ifdef DEBUG
    Msg("* Game %s is successfully saved to file '%s' (%u bytes compressed to %zu)", m_save_name, file_name, source_count, dest_count + sizeof(u32));
#else // DEBUG
    Msg("* Game %s is successfully saved to file '%s'", m_save_name, file_name);
#endif // DEBUG

    if (!update_name)
        strcpy_s(m_save_name, save);
}

void CALifeStorageManager::load(void* buffer, const u32& buffer_size, LPCSTR file_name)
{
    IReader source(buffer, buffer_size);
    header().load(source);
    time_manager().load(source);
    spawns().load(source, file_name);

    graph().on_load();
    objects().load(source);

    VERIFY(can_register_objects());
    can_register_objects(false);
    CALifeObjectRegistry::OBJECT_REGISTRY::iterator B = objects().objects().begin();
    CALifeObjectRegistry::OBJECT_REGISTRY::iterator E = objects().objects().end();
    CALifeObjectRegistry::OBJECT_REGISTRY::iterator I;
    for (I = B; I != E; ++I)
    {
        ALife::_OBJECT_ID id = (*I).second->ID;
        (*I).second->ID = server().PerformIDgen(id);
        VERIFY(id == (*I).second->ID);
        register_object((*I).second, false);
    }

    registry().load(source);

    g_ScriptVars.load(source);
    op22_compat_call("op22_compat.on_after_load");

    can_register_objects(true);

    op22_before_register_objects();

    for (I = B; I != E; ++I)
        (*I).second->on_register();
}

bool CALifeStorageManager::load(LPCSTR save_name)
{
    CTimer timer;
    timer.Start();
    string256 save;
    strcpy_s(save, m_save_name);
    if (!save_name)
    {
        if (!xr_strlen(m_save_name))
            R_ASSERT2(false, "There is no file name specified!");
    }
    else
        strconcat(sizeof(m_save_name), m_save_name, save_name, SAVE_EXTENSION);

    strcpy_s(m_loaded_save, sizeof(m_loaded_save) - 1, m_save_name);

    string_path file_name;
    FS.update_path(file_name, "$game_saves$", m_save_name);

    IReader* stream;
    stream = FS.r_open(file_name);
    if (!stream)
    {
        Msg("* Cannot find saved game %s", file_name);
        strcpy_s(m_save_name, save);
        return (false);
    }

    u32 magic{};
    R_ASSERT(CSavedGameWrapper::valid_saved_game(*stream, magic), make_string("%s\nSaved game version mismatch or saved game is corrupted", file_name));

    unload();
    reload(m_section);

    u32 source_count = stream->r_u32();
    void* source_data = xr_malloc(source_count);
    if (magic == 0)
        R_ASSERT(ZSTD_decompress(source_data, source_count, stream->pointer(), stream->length() - 3 * sizeof(u32)) == source_count);
    else
        rtc_decompress(source_data, source_count, stream->pointer(), stream->length() - 3 * sizeof(u32));
    FS.r_close(stream);
    load(source_data, source_count, file_name);
    xr_free(source_data);

    groups().on_after_game_load();

    VERIFY(graph().actor());

    Msg("* Game %s is successfully loaded from file '%s' (%.3fs)", save_name, file_name, timer.GetElapsed_sec());

    return (true);
}

void CALifeStorageManager::save(NET_Packet& net_packet)
{
    prepare_objects_for_save();

    shared_str game_name;
    net_packet.r_stringZ(game_name);
    save(*game_name, !!net_packet.r_u8());
}

void CALifeStorageManager::prepare_objects_for_save()
{
    Level().ClientSend();
    Level().ClientSave();
}

////////////////////////////////////////////////////////////////////////////
//	Module 		: saved_game_wrapper.cpp
//	Created 	: 21.02.2006
//  Modified 	: 21.02.2006
//	Author		: Dmitriy Iassenev
//	Description : saved game wrapper class
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <zstd.h>

#include "saved_game_wrapper.h"
#include "alife_time_manager.h"
#include "alife_object_registry.h"
#include "xrServer_Objects_ALife_Monsters.h"
#include "ai_space.h"
#include "game_graph.h"
#include "alife_simulator_header.h"
#include "alife_simulator.h"
#include "alife_spawn_registry.h"
#include "string_table.h"
#include <limits>
#include <memory>

extern LPCSTR alife_section;

// OP22 (28.11 perf): сохраняем блоки предпросмотра последнего сейва по пути и времени файла.
int g_op22_save_preview_cache = 1;
namespace
{
struct SavedGamePreview
{
    string_path path{};
    WIN32_FILE_ATTRIBUTE_DATA stamp{};
    u8 data[16 * 1024];
    size_t size{};
};

bool preview_file_stamp(LPCSTR path, WIN32_FILE_ATTRIBUTE_DATA& stamp)
{
    const auto* file = FS.exist(path);
    if (!file || file->vfs != (std::numeric_limits<size_t>::max)())
        return false;

    LPCSTR disk_path = file->real_file_path.empty() ? path : file->real_file_path.c_str();
    return GetFileAttributesExA(disk_path, GetFileExInfoStandard, &stamp) != FALSE;
}

bool same_preview_stamp(const WIN32_FILE_ATTRIBUTE_DATA& a, const WIN32_FILE_ATTRIBUTE_DATA& b)
{
    return CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime) == 0 && CompareFileTime(&a.ftCreationTime, &b.ftCreationTime) == 0 &&
        a.nFileSizeHigh == b.nFileSizeHigh && a.nFileSizeLow == b.nFileSizeLow;
}

// Оставляем чтение актёра при каждом запросе: у старых сейвов оно может выбирать персонажа и обновлять реестр A-Life.
void remember_preview(SavedGamePreview& cache, LPCSTR path, const WIN32_FILE_ATTRIBUTE_DATA& stamp, IReader& reader, void* actor_data, size_t actor_size)
{
    constexpr size_t chunk_headers = 3 * 2 * sizeof(u32);
    if (actor_size + chunk_headers >= sizeof(cache.data))
        return;

    const size_t time_size = reader.find_chunk(GAME_TIME_CHUNK_DATA);
    void* time_data = reader.pointer();
    BOOL compressed = FALSE;
    const size_t spawn_size = reader.find_chunk(SPAWN_CHUNK_DATA, &compressed);
    if (!time_size || !spawn_size || compressed || time_size + actor_size + spawn_size + chunk_headers > sizeof(cache.data))
        return;

    CMemoryWriter preview;
    preview.reserve(time_size + actor_size + spawn_size + chunk_headers);
    preview.w_chunk(GAME_TIME_CHUNK_DATA, time_data, time_size);
    preview.w_chunk(OBJECT_CHUNK_DATA, actor_data, actor_size);
    preview.w_chunk(SPAWN_CHUNK_DATA, reader.pointer(), spawn_size);

    WIN32_FILE_ATTRIBUTE_DATA current{};
    if (!preview_file_stamp(path, current) || !same_preview_stamp(stamp, current))
        return;

    strcpy_s(cache.path, path);
    cache.stamp = current;
    cache.size = preview.size();
    memcpy(cache.data, preview.pointer(), cache.size);
}
}

LPCSTR CSavedGameWrapper::saved_game_full_name(LPCSTR saved_game_name, string_path& result)
{
    string_path temp;
    strconcat(sizeof(temp), temp, saved_game_name, SAVE_EXTENSION);
    FS.update_path(result, "$game_saves$", temp);
    return (result);
}

bool CSavedGameWrapper::saved_game_exist(LPCSTR saved_game_name)
{
    string_path file_name;
    return (!!FS.exist(saved_game_full_name(saved_game_name, file_name)));
}

bool CSavedGameWrapper::valid_saved_game(IReader& stream, u32& magic)
{
    if (stream.length() < 8)
        return (false);

    magic = stream.r_u32();
    if (magic != u32(-1) && magic != u32(0))
        return (false);

    if (stream.r_u32() < 0x0002)
        return (false);

    return (true);
}

bool CSavedGameWrapper::valid_saved_game(LPCSTR saved_game_name)
{
    if (!saved_game_name || strlen(saved_game_name) == 0)
        return false;

    string_path file_name;
    if (!FS.exist(saved_game_full_name(saved_game_name, file_name)))
        return (false);

    u32 magic = 0;

    IReader* stream = FS.r_open(file_name);
    const bool result = valid_saved_game(*stream, magic);
    FS.r_close(stream);
    return (result);
}

CSavedGameWrapper::CSavedGameWrapper(LPCSTR saved_game_name)
{
    string_path preview_path;
    saved_game_full_name(saved_game_name, preview_path);
    R_ASSERT3(FS.exist(preview_path), "There is no saved game ", preview_path);

    // OP22 (28.11 perf): на попадании читаем малый снимок и заново разрешаем уровень по текущему спавну.
    static thread_local SavedGamePreview preview;
    WIN32_FILE_ATTRIBUTE_DATA stamp{};
    const bool cacheable = g_op22_save_preview_cache && preview_file_stamp(preview_path, stamp);
    const bool cached = cacheable && preview.size && !xr_strcmp(preview_path, preview.path) && same_preview_stamp(stamp, preview.stamp);
    const auto free_source = [](void* p) { xr_free(p); };
    std::unique_ptr<void, decltype(free_source)> source_data(nullptr, free_source);
    u8 preview_data[sizeof(preview.data)];
    size_t reader_size;
    void* reader_data;
    if (cached)
    {
        memcpy(preview_data, preview.data, preview.size);
        reader_data = preview_data;
        reader_size = preview.size;
    }
    else
    {
        u32 magic = 0;
        IReader* stream = FS.r_open(preview_path);
        if (!valid_saved_game(*stream, magic))
        {
            FS.r_close(stream);
            CALifeTimeManager time_manager(alife_section);
            m_game_time = time_manager.game_time();
            m_actor_health = 1.f;
            m_level_id = _LEVEL_ID(-1);
            m_level_name = "";
            return;
        }

        const u32 source_count = stream->r_u32();
        source_data.reset(xr_malloc(source_count));
        if (magic == 0)
            R_ASSERT(ZSTD_decompress(source_data.get(), source_count, stream->pointer(), stream->length() - 3 * sizeof(u32)) == source_count);
        else
            rtc_decompress(source_data.get(), source_count, stream->pointer(), stream->length() - 3 * sizeof(u32));
        FS.r_close(stream);
        reader_data = source_data.get();
        reader_size = source_count;
    }

    IReader reader(reader_data, reader_size);
    {
        CALifeTimeManager time_manager(alife_section);
        time_manager.load(reader);
        m_game_time = time_manager.game_time();
    }

    {
        R_ASSERT2(reader.find_chunk(OBJECT_CHUNK_DATA), "Can't find chunk OBJECT_CHUNK_DATA!");
        const size_t actor_offset = reader.tell();
#ifdef DEBUG
        u32 count =
#endif
            reader.r_u32();
        VERIFY(count > 0);
        CSE_ALifeDynamicObject* object = CALifeObjectRegistry::get_object(reader);
        const size_t actor_size = reader.tell() - actor_offset;
        VERIFY(object->ID == 0);
        CSE_ALifeCreatureActor* actor = smart_cast<CSE_ALifeCreatureActor*>(object);
        VERIFY(actor);

        m_actor_health = actor->g_Health();

        IReader* R = reader.open_chunk(SPAWN_CHUNK_DATA);
        R_ASSERT(R, "Spawn version mismatch - REBUILD SPAWN!");

        string_path spawn_file_name;
        {
            IReader* sub_chunk = R->open_chunk(0);
            if (!sub_chunk)
            {
                R->close();
                F_entity_Destroy(object);
                m_level_id = _LEVEL_ID(-1);
                m_level_name = "";
                return;
            }
            sub_chunk->r_stringZ(spawn_file_name, sizeof(spawn_file_name));
            sub_chunk->close();
        }
        R->close();

        string_path file_name;
        if (!FS.exist(file_name, "$game_spawn$", spawn_file_name, ".spawn"))
        {
            F_entity_Destroy(object);
            m_level_id = _LEVEL_ID(-1);
            m_level_name = "";
            return;
        }

        IReader* spawn = nullptr;
        bool b_destroy_spawn = true;
        if (ai().get_alife() && ai().alife().spawns().get_spawn_name() == spawn_file_name)
        {
            spawn = ai().alife().spawns().get_spawn_file();
            b_destroy_spawn = false;
        }
        else
            spawn = FS.r_open(file_name);

        if (!spawn)
        {
            F_entity_Destroy(object);
            m_level_id = _LEVEL_ID(-1);
            m_level_name = "";
            return;
        }

        bool separated_graphs = false;
        R = spawn->open_chunk(4);
        if (!R)
        {
            string_path graph_path;
            FS.update_path(graph_path, fsgame::game_data, GRAPH_NAME);
            R = FS.r_open(graph_path);
            separated_graphs = true;
        }

        if (!R)
        {
            F_entity_Destroy(object);
            if (b_destroy_spawn)
                FS.r_close(spawn);
            m_level_id = _LEVEL_ID(-1);
            m_level_name = "";
            return;
        }

        {
            CGameGraph graph(R, separated_graphs);
            m_level_id = graph.vertex(object->m_tGraphID)->level_id();
            if (graph.header().level_exist(m_level_id))
                m_level_name = graph.header().level(m_level_id).name();
            else
                m_level_name = CStringTable().translate("ui_st_error");
        }

        if (!separated_graphs)
            R->close();
        if (b_destroy_spawn)
            FS.r_close(spawn);
        F_entity_Destroy(object);

        if (cacheable && !cached)
            remember_preview(preview, preview_path, stamp, reader, static_cast<u8*>(reader.begin()) + actor_offset, actor_size);
    }
}

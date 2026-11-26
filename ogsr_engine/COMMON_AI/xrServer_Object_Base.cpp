////////////////////////////////////////////////////////////////////////////
//	Module 		: xrServer_Object_Base.cpp
//	Created 	: 19.09.2002
//  Modified 	: 16.07.2004
//	Author		: Oles Shyshkovtsov, Alexander Maksimchuk, Victor Reutskiy and Dmitriy Iassenev
//	Description : Server base object
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "xrServer_Objects.h"
#include "xrMessages.h"
#include "op22_id32_io.h"

// OP22 (id32): см. op22_id32_io.h
xr_vector<u32>* g_op22_id_wcollect = nullptr;
xr_vector<u32*>* g_op22_id_rcollect = nullptr;
#include "game_base_space.h"
#include "script_value_container_impl.h"
#include "clsid_game.h"

#include "object_factory.h"

LPCSTR script_section = "script";
LPCSTR current_version = "current_server_entity_version";

/*IC*/ u16 script_server_object_version()
{
    static bool initialized = false;
    static u16 script_version = 0;
    if (!initialized)
    {
        initialized = true;
        if (!pSettings->section_exist(script_section) || !pSettings->line_exist(script_section, current_version))
            script_version = 0;
        script_version = pSettings->r_u16(script_section, current_version);
    }
    return (script_version);
}

////////////////////////////////////////////////////////////////////////////
// CPureServerObject
////////////////////////////////////////////////////////////////////////////
void CPureServerObject::save(IWriter& tMemoryStream) {}

void CPureServerObject::load(IReader& tFileStream) {}

void CPureServerObject::load(NET_Packet& tNetPacket) {}

void CPureServerObject::save(NET_Packet& tNetPacket) {}

////////////////////////////////////////////////////////////////////////////
// CSE_Abstract
////////////////////////////////////////////////////////////////////////////
CSE_Abstract::CSE_Abstract(LPCSTR caSection)
{
    m_editor_flags.zero();
    RespawnTime = 0;
    net_Ready = FALSE;
    ID = 0xffff;
    ID_Parent = 0xffff;
    ID_Phantom = 0xffff;
    owner = 0;
    s_gameid = 0;
    s_RP = 0xFE; // Use supplied coords
    s_flags.assign(0);
    s_name = caSection;
    s_name_replace = 0; // xr_strdup("");
    o_Angle.set(0.f, 0.f, 0.f);
    o_Position.set(0.f, 0.f, 0.f);
    m_bALifeControl = false;
    m_wVersion = 0;
    m_script_version = 0;
    m_tClassID = TEXT2CLSID(pSettings->r_string(caSection, "class"));

    //	m_spawn_probability			= 1.f;
    m_spawn_flags.zero();
    m_spawn_flags.set(flSpawnEnabled, TRUE);
    m_spawn_flags.set(flSpawnOnSurgeOnly, TRUE);
    m_spawn_flags.set(flSpawnSingleItemOnly, TRUE);
    m_spawn_flags.set(flSpawnIfDestroyedOnly, TRUE);
    m_spawn_flags.set(flSpawnInfiniteCount, TRUE);
    //	m_max_spawn_count			= 1;
    //	m_spawn_control				= "";
    //	m_spawn_count				= 0;
    //	m_last_spawn_time			= 0;
    //	m_next_spawn_time			= 0;
    //	m_min_spawn_interval		= 0;
    //	m_max_spawn_interval		= 0;
    m_ini_file = 0;

    if (pSettings->line_exist(caSection, "custom_data"))
    {
        string_path file_name;
        FS.update_path(file_name, "$game_config$", pSettings->r_string(caSection, "custom_data"));
        if (!FS.exist(file_name))
        {
            Msg("! cannot open config file %s", file_name);
        }
        else
        {
            IReader* reader = FS.r_open(file_name);
            reader->skip_bom(file_name);
            const xr_string temp{reinterpret_cast<const char*>(reader->pointer()), static_cast<size_t>(reader->elapsed())};
            m_ini_string = temp.c_str();
            FS.r_close(reader);
        }
    }

    m_script_clsid = object_factory().script_clsid(m_tClassID);
}

CSE_Abstract::~CSE_Abstract()
{
    xr_free(s_name_replace);
    xr_delete(m_ini_file);
}

CSE_Visual* CSE_Abstract::visual() { return (0); }

ISE_Shape* CSE_Abstract::shape() { return (0); }

CSE_Motion* CSE_Abstract::motion() { return (0); }

CInifile& CSE_Abstract::spawn_ini()
{
    if (!m_ini_file)
    {
        IReader r((void*)m_ini_string.c_str(), m_ini_string.size());
        m_ini_file = xr_new<CInifile>(&r, FS.get_path("$game_config$")->m_Path);
    }
    return (*m_ini_file);
}

void CSE_Abstract::Spawn_Write(NET_Packet& tNetPacket, BOOL bLocal)
{
    // generic
    tNetPacket.w_begin(M_SPAWN);
    tNetPacket.w_stringZ(s_name);
    tNetPacket.w_stringZ(s_name_replace ? s_name_replace : "");
    tNetPacket.w_u8(s_gameid);
    tNetPacket.w_u8(s_RP);
    tNetPacket.w_vec3(o_Position);
    tNetPacket.w_vec3(o_Angle);
    tNetPacket.w_u16(RespawnTime);
    // OP22 (id32): ID - младшие 16 бит на старых местах (формат ОП), полные значения - хвостом после STATE (op22_id32_io.h)
    tNetPacket.w_u16(u16(ID & 0xffff));
    tNetPacket.w_u16(u16(ID_Parent & 0xffff));
    tNetPacket.w_u16(u16(ID_Phantom & 0xffff));

    s_flags.set(M_SPAWN_VERSION, TRUE);
    s_flags.set(M_SPAWN_CD_ID32, m_cd_id32 && !client_data.empty()); // OP22 (id32): ID в client_data - 4 байта
    if (bLocal)
        tNetPacket.w_u16(u16(s_flags.flags | M_SPAWN_OBJECT_LOCAL));
    else
        tNetPacket.w_u16(u16(s_flags.flags & ~(M_SPAWN_OBJECT_LOCAL | M_SPAWN_OBJECT_ASPLAYER)));

    tNetPacket.w_u16(SPAWN_VERSION);

    tNetPacket.w_u16(script_server_object_version());

    // client object custom data serialization SAVE
    u16 client_data_size = (u16)client_data.size(); //не может быть больше 256 байт
    tNetPacket.w_u16(client_data_size);
    //	Msg							("SERVER:saving:save:%d bytes:%d:%s",client_data_size,ID,s_name_replace ? s_name_replace : "");
    if (client_data_size > 0)
    {
        tNetPacket.w(&*client_data.begin(), client_data_size);
    }

    tNetPacket.w(&m_tSpawnID, sizeof(m_tSpawnID));
    //	tNetPacket.w_float			(m_spawn_probability);
    //	tNetPacket.w_u32			(m_spawn_flags.get());
    //	tNetPacket.w_stringZ		(m_spawn_control);
    //	tNetPacket.w_u32			(m_max_spawn_count);
    //	tNetPacket.w_u64			(m_min_spawn_interval);
    //	tNetPacket.w_u64			(m_max_spawn_interval);

    // write specific data
    u32 position = tNetPacket.w_tell();
    tNetPacket.w_u16(0);
    xr_vector<u32> op22_ids{ID, ID_Parent, ID_Phantom}; // OP22 (id32): полные ID заголовка и STATE
    xr_vector<u32>* op22_prev = g_op22_id_wcollect;
    g_op22_id_wcollect = &op22_ids;
    STATE_Write(tNetPacket);
    g_op22_id_wcollect = op22_prev;
    u16 size = u16(tNetPacket.w_tell() - position);
    R_ASSERT3((m_tClassID == CLSID_SPECTATOR) || (size > sizeof(size)), "object isn't successfully saved, get your backup :(", name_replace());
    tNetPacket.w_seek(position, &size, sizeof(u16));

    // OP22 (id32): хвост с полными ID - только если есть ID > 0xffff (иначе пакет побайтно как в ОП)
    if (std::any_of(op22_ids.begin(), op22_ids.end(), [](u32 id) { return id > 0xffff; }))
    {
        tNetPacket.w_u32(OP22_ID32_TAIL_MAGIC);
        tNetPacket.w_u32(u32(op22_ids.size()));
        for (const u32 id : op22_ids)
            tNetPacket.w_u32(id);
    }
}

BOOL CSE_Abstract::Spawn_Read(NET_Packet& tNetPacket)
{
    u16 dummy16;
    // generic
    tNetPacket.r_begin(dummy16);
    R_ASSERT(M_SPAWN == dummy16);
    tNetPacket.r_stringZ(s_name);

    string256 temp;
    tNetPacket.r_stringZ(temp);
    set_name_replace(temp);
    tNetPacket.r_u8(s_gameid);
    tNetPacket.r_u8(s_RP);
    tNetPacket.r_vec3(o_Position);
    tNetPacket.r_vec3(o_Angle);
    tNetPacket.r_u16(RespawnTime);
    ID = tNetPacket.r_u16(); // OP22 (id32): младшие 16 бит
    ID_Parent = tNetPacket.r_u16();
    ID_Phantom = tNetPacket.r_u16();

    tNetPacket.r_u16(s_flags.flags);
    m_cd_id32 = !!(s_flags.flags & M_SPAWN_CD_ID32); // OP22 (id32)

    tNetPacket.r_u16(m_wVersion);

    ASSERT_FMT(m_wVersion && m_wVersion >= 118, "Invalid spawn version: [%u]", m_wVersion);

    m_script_version = tNetPacket.r_u16();

    // read specific data

    // client object custom data serialization LOAD
    {
        client_data.clear();

        const u16 client_data_size = tNetPacket.r_u16(); // не может быть больше 256 байт
        if (client_data_size > 0)
        {
            client_data.resize(client_data_size);
            tNetPacket.r(client_data.data(), client_data_size);
        }
    }

    tNetPacket.r(&m_tSpawnID, sizeof(m_tSpawnID));

    const u32 op22_block = tNetPacket.r_tell(); // OP22 (id32): начало блока STATE (с полем размера)
    u16 size;
    tNetPacket.r_u16(size); // size
    R_ASSERT((size > sizeof(size)), "cannot read object, which is not successfully saved :(", name_replace());

    // OP22 (id32): хвост с полными ID лежит после блока STATE; Lua-часть STATE (se_zones, smart_terrain, ...) читает
    // «до конца пакета» - на время чтения конец пакета ставится на конец блока
    const u32 op22_end = op22_block + size;
    const u32 op22_count = tNetPacket.B.count;
    const bool op22_has_tail = op22_count >= op22_end + 8 && *(u32*)(&tNetPacket.B.data[op22_end]) == OP22_ID32_TAIL_MAGIC;
    xr_vector<u32*> op22_fields{&ID, &ID_Parent, &ID_Phantom};
    xr_vector<u32*>* op22_prev = g_op22_id_rcollect;
    g_op22_id_rcollect = &op22_fields;
    if (op22_has_tail)
        tNetPacket.B.count = op22_end;
    STATE_Read(tNetPacket, size);
    tNetPacket.B.count = op22_count;
    g_op22_id_rcollect = op22_prev;
    if (op22_has_tail)
    {
        tNetPacket.r_seek(op22_end + 4);
        const u32 n = tNetPacket.r_u32();
        if (n == op22_fields.size())
        {
            for (u32 i = 0; i < n; ++i)
            {
                const u32 full = tNetPacket.r_u32();
                if ((full & 0xffff) == (*op22_fields[i] & 0xffff))
                    *op22_fields[i] = full;
                else
                    Msg("! [OP22 id32] %s: id tail mismatch at %u (%u vs %u)", name_replace(), i, full, *op22_fields[i]);
            }
        }
        else
            Msg("! [OP22 id32] %s: id tail count %u, fields %u - tail ignored", name_replace(), n, u32(op22_fields.size()));
    }

    return TRUE;
}

void CSE_Abstract::load(NET_Packet& tNetPacket)
{
    CPureServerObject::load(tNetPacket);
    u16 client_data_size = (m_wVersion > 93) ? tNetPacket.r_u16() : tNetPacket.r_u8();
    if (client_data_size > 0)
    {
#ifdef DEBUG
//		Msg						("SERVER:loading:load:%d bytes:%d:%s",client_data_size,ID,s_name_replace ? s_name_replace : "");
#endif // DEBUG
        client_data.resize(client_data_size);
        tNetPacket.r(&*client_data.begin(), client_data_size);
    }
    else
    {
#ifdef DEBUG
        if (!client_data.empty())
            Msg("CSE_Abstract::load: client_data is cleared for [%d][%s]", ID, name_replace());
#endif // DEBUG
        client_data.clear();
    }
    m_cd_id32 = g_op22_ids_wide; // OP22 (id32): клиент писал ID в client_data 4 байтами, только если в игре есть ID >= 0xfffe
}

CSE_Abstract* CSE_Abstract::base() { return (this); }

const CSE_Abstract* CSE_Abstract::base() const { return (this); }

CSE_Abstract* CSE_Abstract::init() { return (this); }

LPCSTR CSE_Abstract::name() const { return (*s_name); }

LPCSTR CSE_Abstract::name_replace() const { return (s_name_replace); }

Fvector& CSE_Abstract::position() { return (o_Position); }

Fvector& CSE_Abstract::angle() { return (o_Angle); }

Flags16& CSE_Abstract::flags() { return (s_flags); }

xr_token game_types[] = {{"any game", GAME_ANY},
                         {"single", GAME_SINGLE},
                         //{ "deathmatch",		GAME_DEATHMATCH },
                         //	{ "CTF",			GAME_CTF		},
                         //	{ "assault",		GAME_ASSAULT	},
                         //{ "counterstrike",	GAME_CS			},
                         //{ "teamdeathmatch",	GAME_TEAMDEATHMATCH },
                         //{ "artefacthunt",	GAME_ARTEFACTHUNT },
                         {0, 0}};

bool CSE_Abstract::validate() { return (true); }

/**
void CSE_Abstract::save_update				(NET_Packet &tNetPacket)
{
    tNetPacket.w				(&m_spawn_count,sizeof(m_spawn_count));
    tNetPacket.w				(&m_last_spawn_time,sizeof(m_last_spawn_time));
    tNetPacket.w				(&m_next_spawn_time,sizeof(m_next_spawn_time));
}

void CSE_Abstract::load_update				(NET_Packet &tNetPacket)
{
    tNetPacket.r				(&m_spawn_count,sizeof(m_spawn_count));
    tNetPacket.r				(&m_last_spawn_time,sizeof(m_last_spawn_time));
    tNetPacket.r				(&m_next_spawn_time,sizeof(m_next_spawn_time));
}
/**/
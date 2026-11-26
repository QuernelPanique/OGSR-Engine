////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_simulator_header.cpp
//	Created 	: 05.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator header
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_simulator_header.h"
#include "op22_id32.h"

bool g_op22_alife_id32 = true; // OP22 (id32)

CALifeSimulatorHeader::~CALifeSimulatorHeader() {}

void CALifeSimulatorHeader::save(IWriter& memory_stream)
{
    memory_stream.open_chunk(ALIFE_CHUNK_DATA);
    // OP22 (id32): 11 и 4-байтные ID в реестрах - только если в игре есть ID >= 0xfffe; иначе сейв как раньше (10)
    g_op22_alife_id32 = g_op22_ids_wide;
    memory_stream.w_u32(g_op22_alife_id32 ? ALIFE_VERSION : 10);
    memory_stream.close_chunk();
}

void CALifeSimulatorHeader::load(IReader& file_stream)
{
    R_ASSERT2(file_stream.find_chunk(ALIFE_CHUNK_DATA), "Can't find chunk ALIFE_CHUNK_DATA");
    m_version = file_stream.r_u32();
    g_op22_alife_id32 = m_version >= 11; // OP22 (id32): старые сейвы (10) - ID в реестрах 2 байта

    R_ASSERT2(m_version >= 0x0002, "ALife version mismatch! (Delete saved game and try again)");
};

bool CALifeSimulatorHeader::valid(IReader& file_stream) const
{
    if (!file_stream.find_chunk(ALIFE_CHUNK_DATA))
        return (false);

    u32 version;
    file_stream.r(&version, sizeof(version));
    return (version >= 2);
}

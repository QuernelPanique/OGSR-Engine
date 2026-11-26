////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_simulator_header_inline.h
//	Created 	: 05.01.2003
//  Modified 	: 12.05.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife Simulator header inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once

extern bool g_op22_alife_id32;
IC CALifeSimulatorHeader::CALifeSimulatorHeader(LPCSTR section)
{
    m_version = ALIFE_VERSION;
    g_op22_alife_id32 = true; // OP22 (id32): новая игра
}

IC u32 CALifeSimulatorHeader::version() const { return (m_version); }

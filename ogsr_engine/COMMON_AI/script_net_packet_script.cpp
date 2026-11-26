////////////////////////////////////////////////////////////////////////////
//	Module 		: script_net packet_script.cpp
//	Created 	: 06.02.2004
//  Modified 	: 24.06.2004
//	Author		: Dmitriy Iassenev
//	Description : XRay Script net packet class script export
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "script_net_packet.h"
#include "../xr_3da/NET_Server_Trash/net_utils.h"

using namespace luabind;

bool r_eof(NET_Packet* self) { return (!!self->r_eof()); }

LPCSTR r_stringZ(NET_Packet* self)
{
    shared_str temp;
    self->r_stringZ(temp);
    return (*temp);
}

void w_bool(NET_Packet* self, bool value) { self->w_u8(value ? 1 : 0); }

bool r_bool(NET_Packet* self) { return (!!self->r_u8()); }

ClientID r_clientID(NET_Packet* self)
{
    ClientID clientID;
    self->r_clientID(clientID);
    return clientID;
}

extern u16 script_server_object_version();

// OP22 (id32): w_u16 из Lua - значение больше 65535 (например, ID объекта выше старого предела) обрезалось бы молча;
// пишем как раньше, но сообщаем в лог. Для ID в скриптах - w_id/r_id (4 байта).
static void op22_w_u16_checked(NET_Packet* self, double v)
{
    if (v > 65535.0)
        Msg("! [OP22 id32] net_packet:w_u16(%.0f) - value above 65535 truncated (object id? use w_id)", v);
    self->w_u16(u16(s64(v)));
}
static void op22_w_id(NET_Packet* self, u32 v) { self->w_id(v); }
static u32 op22_r_id(NET_Packet* self) { return self->r_id(); }


void CScriptNetPacket::script_register(lua_State* L)
{
    module(L)[(def("script_server_object_version", &script_server_object_version),

              class_<ClientID>("ClientID").def(constructor<>()).def("value", &ClientID::value).def("set", &ClientID::set).def(self == other<ClientID>()),

              class_<NET_Packet>("net_packet")
                  .def(constructor<>())

                  .def("w_begin", &NET_Packet::w_begin)
                  .def("w_tell", &NET_Packet::w_tell)
                  .def("w_vec3", &NET_Packet::w_vec3)

                  .def("w_float", &NET_Packet::w_float)
                  .def("w_u64", &NET_Packet::w_u64)
                  .def("w_s64", &NET_Packet::w_s64)
                  .def("w_u32", &NET_Packet::w_u32)
                  .def("w_s32", &NET_Packet::w_s32)
                  .def("w_u16", &op22_w_u16_checked) // OP22 (id32)
                  .def("w_id", &op22_w_id)
                  .def("r_id", &op22_r_id)
                  .def("w_s16", &NET_Packet::w_s16)
                  .def("w_u8", &NET_Packet::w_u8)
                  .def("w_s8", &NET_Packet::w_s8)

                  .def("w_bool", &w_bool)
                  .def("w_float_q16", &NET_Packet::w_float_q16)
                  .def("w_float_q8", &NET_Packet::w_float_q8)
                  .def("w_angle16", &NET_Packet::w_angle16)
                  .def("w_angle8", &NET_Packet::w_angle8)
                  .def("w_dir", &NET_Packet::w_dir)
                  .def("w_sdir", &NET_Packet::w_sdir)
                  .def("w_stringZ", (void(NET_Packet::*)(LPCSTR))(&NET_Packet::w_stringZ))
                  .def("w_matrix", &NET_Packet::w_matrix)
                  .def("w_clientID", &NET_Packet::w_clientID)

                  //.def("w_chunk_open8", &NET_Packet::w_chunk_open8)
                  //.def("w_chunk_close8", &NET_Packet::w_chunk_close8)
                  //.def("w_chunk_open16", &NET_Packet::w_chunk_open16)
                  //.def("w_chunk_close16", &NET_Packet::w_chunk_close16)

                  .def("r_seek", &NET_Packet::r_seek)

                  .def("r_elapsed", &NET_Packet::r_elapsed)
                  .def("r_advance", &NET_Packet::r_advance)

                  .def("r_eof", &r_eof)

                  .def("r_begin", &NET_Packet::r_begin)
                  .def("r_tell", &NET_Packet::r_tell)
                  .def("r_vec3", (void(NET_Packet::*)(Fvector&))(&NET_Packet::r_vec3), pure_out_value<2>())

                  .def("r_float", (void(NET_Packet::*)(float&))(&NET_Packet::r_float))
                  .def("r_u64", (void(NET_Packet::*)(u64&))(&NET_Packet::r_u64))
                  .def("r_s64", (void(NET_Packet::*)(s64&))(&NET_Packet::r_s64))
                  .def("r_u32", (void(NET_Packet::*)(u32&))(&NET_Packet::r_u32))
                  .def("r_s32", (void(NET_Packet::*)(s32&))(&NET_Packet::r_s32))
                  .def("r_u16", (void(NET_Packet::*)(u16&))(&NET_Packet::r_u16))
                  .def("r_s16", (void(NET_Packet::*)(s16&))(&NET_Packet::r_s16))
                  .def("r_u8", (void(NET_Packet::*)(u8&))(&NET_Packet::r_u8))
                  .def("r_s8", (void(NET_Packet::*)(s8&))(&NET_Packet::r_s8))

                  .def("r_float", (float(NET_Packet::*)())(&NET_Packet::r_float))
                  .def("r_u64", (u64(NET_Packet::*)())(&NET_Packet::r_u64))
                  .def("r_s64", (s64(NET_Packet::*)())(&NET_Packet::r_s64))
                  .def("r_u32", (u32(NET_Packet::*)())(&NET_Packet::r_u32))
                  .def("r_s32", (s32(NET_Packet::*)())(&NET_Packet::r_s32))
                  .def("r_u16", (u16(NET_Packet::*)())(&NET_Packet::r_u16))
                  .def("r_s16", (s16(NET_Packet::*)())(&NET_Packet::r_s16))
                  .def("r_u8", (u8(NET_Packet::*)())(&NET_Packet::r_u8))
                  .def("r_s8", (s8(NET_Packet::*)())(&NET_Packet::r_s8))

                  .def("r_bool", &r_bool)
                  .def("r_float_q16", (void(NET_Packet::*)(float&, float, float))(&NET_Packet::r_float_q16))
                  .def("r_float_q8", (void(NET_Packet::*)(float&, float, float))(&NET_Packet::r_float_q8))
                  .def("r_angle16", &NET_Packet::r_angle16)
                  .def("r_angle8", &NET_Packet::r_angle8)
                  .def("r_dir", &NET_Packet::r_dir)
                  .def("r_sdir", &NET_Packet::r_sdir)
                  .def("r_stringZ", &r_stringZ)
                  .def("r_matrix", &NET_Packet::r_matrix)
                  .def("r_clientID", &r_clientID)
    )];
}
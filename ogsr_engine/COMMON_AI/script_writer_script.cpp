#include "stdafx.h"

#include "script_writer.h"

using namespace luabind;

void w_bool(IWriter* self, bool v) { (self->w_u8(v)); }

// OP22 (id32): см. script_net_packet_script.cpp
static void op22_writer_w_u16(IWriter* self, double v)
{
    if (v > 65535.0)
        Msg("! [OP22 id32] writer:w_u16(%.0f) - value above 65535 truncated", v);
    self->w_u16(u16(s64(v)));
}

void w_fvector3(IWriter* self, Fvector* arg0) { self->w_fvector3(*arg0); }

void w_stringZ(IWriter* self, LPCSTR s)
{
    self->w_stringZ(s);
}

void CScriptWriter::script_register(lua_State* L)
{
    module(L)[class_<IWriter>("writer")
                  .def("open_chunk", &IWriter::open_chunk)
                  .def("close_chunk", &IWriter::close_chunk)
                  .def("w_seek", &IWriter::seek)
                  .def("w_tell", &IWriter::tell)
                  .def("w_vec3", &::w_fvector3)
                  .def("w_float", &IWriter::w_float)
                  .def("w_u64", &IWriter::w_u64)
                  .def("w_s64", &IWriter::w_s64)
                  .def("w_u32", &IWriter::w_u32)
                  .def("w_s32", &IWriter::w_s32)
                  .def("w_u16", &op22_writer_w_u16) // OP22 (id32)
                  .def("w_s16", &IWriter::w_s16)
                  .def("w_u8", &IWriter::w_u8)
                  .def("w_s8", &IWriter::w_s8)
                  .def("w_bool", &::w_bool)
                  .def("w_float_q16", &IWriter::w_float_q16)
                  .def("w_float_q8", &IWriter::w_float_q8)
                  .def("w_angle16", &IWriter::w_angle16)
                  .def("w_angle8", &IWriter::w_angle8)
                  .def("w_dir", &IWriter::w_dir)
                  .def("w_sdir", &IWriter::w_sdir)
                  .def("w_stringZ", &w_stringZ)];
}

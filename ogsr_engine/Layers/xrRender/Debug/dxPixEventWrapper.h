#pragma once

#ifdef TRACY_ENABLE
#include "dxPixEvents.h"

extern bool is_editor_active;

class dxPixEventWrapper
{
    CBackend& cmd_list;

    size_t idx{};

    // OP22: Tracy - замер видеокарты на каждом проходе PIX_EVENT, только в основном контексте D3D11
    // (запросы времени в отложенном контексте не означают время на видеокарте)
    tracy::D3D11ZoneScope gpu_zone;

public:
    dxPixEventWrapper(CBackend& cmd_list_in, const char* name, const wchar_t* wname)
        : cmd_list(cmd_list_in), gpu_zone(HW.profiler_ctx, __LINE__, __FILE__, sizeof(__FILE__) - 1, "PIX_EVENT", 9, name, strlen(name),
                                          cmd_list_in.context_id == CHW::IMM_CTX_ID)
    {
        cmd_list.gpu_mark_begin(wname);

        if (is_editor_active && cmd_list.context_id == CHW::IMM_CTX_ID)
            idx = PIXEventsPushEvent(cmd_list.context_id, name);
    }
    ~dxPixEventWrapper()
    {
        cmd_list.gpu_mark_end();

        if (is_editor_active && cmd_list.context_id == CHW::IMM_CTX_ID)
            PIXEventsPopEvent(cmd_list.context_id, idx);
    }
};

#define PIX_EVENT(Name) dxPixEventWrapper pixEvent##Name(RCache, #Name, L## #Name)
#define PIX_EVENT_CTX(C, Name) dxPixEventWrapper pixEvent##Name(C, #Name, L## #Name)

#define PIX_FRAME_START() if (is_editor_active) PIXEventsBeginRendering()
#define PIX_FRAME_END() if (is_editor_active) PIXEventsEndRendering()

#define DXUT_SetDebugName(pObj, pstrName) pObj->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(pstrName), pstrName)

#else

#define PIX_EVENT(Name) __noop(RCache, L## #Name)
#define PIX_EVENT_CTX(C, Name) __noop(C, L## #Name)

#define PIX_FRAME_START()
#define PIX_FRAME_END()

#define DXUT_SetDebugName(pObj, pstrName) __noop(pObj, pstrName)

#endif

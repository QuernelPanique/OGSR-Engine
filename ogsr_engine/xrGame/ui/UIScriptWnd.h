#pragma once

#include "UIDialogWnd.h"
#include "../script_export_space.h"

struct SCallbackInfo;

// OP22: событие закрытия скриптового окна (ui_events.WINDOW_STOP_DIALOG в op22_compat.script, то же значение)
constexpr s16 OP22_WINDOW_STOP_DIALOG = 30087;

class CUIDialogWndEx : public CUIDialogWnd, public DLL_Pure
{
    typedef CUIDialogWnd inherited;
    typedef xr_vector<SCallbackInfo> CALLBACKS;
    typedef CALLBACKS::iterator CALLBACK_IT;

private:
    CALLBACKS m_callbacks;
    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = NULL);
    SCallbackInfo* NewCallback();

protected:
    bool Load(LPCSTR xml_name);

public:
    CUIDialogWndEx();
    virtual ~CUIDialogWndEx();

    void Register(CUIWindow* pChild);
    void Register(CUIWindow* pChild, LPCSTR name);

    void AddCallback(LPCSTR control_id, s16 event, const luabind::functor<void>& lua_function);
    void AddCallback(LPCSTR control_id, s16 event, const luabind::functor<void>& functor, const luabind::object& object);
    void ClearCallbacks();

    virtual void Update();
    virtual void Hide(); // OP22: колбэки WINDOW_STOP_DIALOG, как в движке ОП
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);
    virtual bool Dispatch(int cmd, int param) { return true; }

    template <typename T>
    IC T* GetControl(LPCSTR name);

    DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(CUIDialogWndEx)
#undef script_type_list
#define script_type_list save_type_list(CUIDialogWndEx)

// OP22: контролы окна опций ОП 2.2 (реализация и регистрация - op22_ui_custom_controls.cpp)
#pragma once
#include "UITrackBar.h"
#include "UISpinText.h"
#include "UI3tButton.h"
#include "UILines.h"
#include "../ai_space.h"
#include "../script_engine.h"

class CUITrackBarCustom : public CUITrackBar
{
    bool m_show_value{};

public:
    // CUIOptionsItem: консоли нет
    virtual void SetCurrentValue() { UpdatePos(); }
    virtual void SaveValue()
    {
        string128 buf{};
        if (m_show_value)
        {
            if (!m_b_is_float)
                sprintf_s(buf, "%d", m_i_val);
            else if (m_f_step >= 1)
                sprintf_s(buf, "%2.0f", m_f_val);
            else if (m_f_step >= 0.1f)
                sprintf_s(buf, "%3.1f", m_f_val);
            else
                sprintf_s(buf, "%4.2f", m_f_val);
        }
        if (m_op22 && m_op22_val) // OP22: значение справа, не на ползунке
        {
            m_op22_val->SetText(buf);
            m_pSlider->SetText("");
        }
        else
            m_pSlider->SetText(buf);
        if (GetMessageTarget())
            GetMessageTarget()->SendMessage(this, TRACKBAR_CHANGED);
    }
    virtual bool IsChanged() { return false; }
    virtual void SeveBackUpValue() {}
    virtual void Undo() {}

    // после CUIXmlInit::InitTrackBar: тип задан, а min/max/val в union остались в float - привести
    void Normalize()
    {
        if (!m_b_is_float)
        {
            m_i_min = iFloor(m_f_min_xml);
            m_i_max = fis_zero(m_f_max_xml) ? 1 : iFloor(m_f_max_xml);
            m_i_val = m_i_min;
            if (m_i_step <= 0)
                m_i_step = 1;
        }
        else
        {
            m_f_min = m_f_min_xml;
            m_f_max = fis_zero(m_f_max_xml) ? 1.f : m_f_max_xml;
            m_f_val = m_f_min;
        }
        SafeUpdate();
    }
    void SafeUpdate()
    {
        if (m_b_is_float ? !(m_f_max > m_f_min) : !(m_i_max > m_i_min))
        {
            SaveValue();
            return;
        }
        UpdatePos();
    }

    void SetTypeCustom(bool is_float)
    {
        if (is_float == m_b_is_float)
            return;
        if (is_float)
        {
            const int v = m_i_val, mn = m_i_min, mx = m_i_max, st = m_i_step;
            m_f_val = float(v), m_f_min = float(mn), m_f_max = float(mx), m_f_step = st > 0 ? float(st) : 0.01f;
        }
        else
        {
            const float v = m_f_val, mn = m_f_min, mx = m_f_max, st = m_f_step;
            m_i_val = iFloor(v), m_i_min = iFloor(mn), m_i_max = iFloor(mx), m_i_step = st >= 1.f ? iFloor(st) : 1;
        }
        m_b_is_float = is_float;
        SafeUpdate();
    }
    void SetValue(float v)
    {
        if (m_b_is_float)
        {
            clamp(v, m_f_min, m_f_max);
            m_f_val = v;
        }
        else
        {
            int i = iFloor(v + 0.5f);
            clamp(i, m_i_min, m_i_max);
            m_i_val = i;
        }
        SafeUpdate();
    }
    float GetValue() { return GetTrackValue(); }
    void SetMinValue(float v)
    {
        if (m_b_is_float)
            m_f_min = v, m_f_val = _max(m_f_val, v);
        else
            m_i_min = iFloor(v), m_i_val = _max(m_i_val, m_i_min);
        SafeUpdate();
    }
    void SetMaxValue(float v)
    {
        if (m_b_is_float)
            m_f_max = v, m_f_val = _min(m_f_val, v);
        else
            m_i_max = iFloor(v), m_i_val = _min(m_i_val, m_i_max);
        SafeUpdate();
    }
    void SetStepCustom(float s)
    {
        if (m_b_is_float)
            m_f_step = s > 0 ? s : 0.01f;
        else
            m_i_step = s >= 1.f ? iFloor(s) : 1;
    }
    void ShowDynamicValue(bool b)
    {
        m_show_value = b;
        SaveValue();
    }
};

class CUISpinTextCustom : public CUISpinText
{
    luabind::functor<void> m_callback;
    bool m_has_callback{};

    void changed(int old_index)
    {
        if (old_index == m_curItem || !m_has_callback)
            return;
        m_callback(old_index, m_curItem);
    }

public:
    virtual void SetCurrentValue()
    {
        if (m_curItem >= 0)
            SetItem();
    }
    virtual void SaveValue() {}
    virtual bool IsChanged() { return false; }

    virtual void OnBtnUpClick()
    {
        const int old = m_curItem;
        CUISpinText::OnBtnUpClick();
        changed(old);
    }
    virtual void OnBtnDownClick()
    {
        const int old = m_curItem;
        CUISpinText::OnBtnDownClick();
        changed(old);
    }

    void AddItem(LPCSTR text, int id) { AddItem_(text ? text : "", id); }
    int GetSelectedId() { return m_curItem >= 0 ? m_list[m_curItem]._id : -1; }
    void SetSelectedId(int id)
    {
        for (u32 i = 0; i < m_list.size(); ++i)
            if (m_list[i]._id == id)
            {
                m_curItem = int(i);
                SetItem();
                return;
            }
    }
    LPCSTR GetSelectedText() { return m_curItem >= 0 ? m_list[m_curItem]._orig.c_str() : ""; }
    void SetSelectedText(LPCSTR text)
    {
        if (!text)
            return;
        for (u32 i = 0; i < m_list.size(); ++i)
            if (!xr_strcmp(m_list[i]._orig, text) || !xr_strcmp(m_list[i]._transl, text))
            {
                m_curItem = int(i);
                SetItem();
                return;
            }
    }
    int GetIdByIndex(int i) { return (i >= 0 && i < int(m_list.size())) ? m_list[i]._id : -1; }
    LPCSTR GetTextByIndex(int i) { return (i >= 0 && i < int(m_list.size())) ? m_list[i]._orig.c_str() : ""; }
    void SetSpinButtonCallback(const luabind::functor<void>& fn)
    {
        m_has_callback = false;
        if (!fn.is_valid())
            return;
        lua_State* L = ai().script_engine().lua();
        fn.pushvalue();
        const bool ok = lua_isfunction(L, -1);
        lua_pop(L, 1);
        if (!ok)
            return;
        m_callback = fn;
        m_has_callback = true;
    }
};


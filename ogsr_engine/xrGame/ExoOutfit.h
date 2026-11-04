///////////////////////////////////////////////////////////////
// ExoOutfit.h
// ExoOutfit - защитный костюм с усилением
///////////////////////////////////////////////////////////////

#pragma once

#include "customoutfit.h"

class CExoOutfit : public CCustomOutfit
{
private:
    typedef CCustomOutfit inherited;

    // OP22: аккумулятор экзоскелета ОП 2.2. Состояние и расчёт заряда - в скрипте op22_exo.script,
    // здесь только меню, бросок на костюм, вызовы из обновления актора и запрет бега/прыжка без заряда
    u32 m_op22_time{};
    u32 m_op22_jumps{};
    float m_op22_ms_move{};
    float m_op22_ms_sprint{};
    bool m_op22_in_jump{};
    bool m_op22_powered{true};
    float m_op22_charge{-1.f}; // 0..1, -1 - костюм без батарей или ещё не обновлялся

public:
    CExoOutfit(void);
    virtual ~CExoOutfit(void);

    virtual bool CanAttach(PIItem pIItem) override; // OP22
    virtual bool Attach(PIItem pIItem, bool b_send_event) override; // OP22

    void op22_update(u32 actor_state); // OP22: из CActor::UpdateCL, костюм надет
    bool op22_powered() const { return m_op22_powered; } // OP22
    float op22_charge() const { return m_op22_charge; } // OP22: для значка на HUD
    bool op22_has_battery(); // OP22: для значка на иконке
};

bool op22_exo_unpowered(); // OP22: надет экзоскелет с пустым аккумулятором или без него

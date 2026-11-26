/////////////////////////////////////////////////////
//
// PdaMsg.h - сообщение посылаемое при помощи PDA
//
/////////////////////////////////////////////////////

#pragma once
#include "op22_id32.h"
#include "alife_space.h"
#include "pda_space.h"

//структура для описания сообщения PDA,
//используется для ведения логов
typedef struct tagSPdaMessage
{
    EPdaMsg msg;

    // true если мы получали сообщение
    //и false если мы его посылали
    bool receive;

    // true, если сообщение - вопрос
    //и false, если ответ
    bool question;

    // информаци
    shared_str info_id;

    //время получения/отправки сообщения
    ALife::_TIME_ID time;

} SPdaMessage;

//информация о контактах персонажей по PDA и во время диалога
struct TALK_CONTACT_DATA
{
    TALK_CONTACT_DATA() : id(u16(-1)), time(0){};
    TALK_CONTACT_DATA(u32 contact_id, ALife::_TIME_ID contact_time) : id(contact_id), time(contact_time){}; // OP22 (id32)
    //время контакта
    ALife::_TIME_ID time;
    // id персонажа с которым говорили
    u32 id; // OP22 (id32): было u16 (в старом сейве - u16 + 2 байта выравнивания, см. op22_fix_old_ids)
};

DEFINE_VECTOR(TALK_CONTACT_DATA, TALK_CONTACT_VECTOR, TALK_CONTACT_VECTOR_IT);
static_assert(sizeof(TALK_CONTACT_DATA) == 16, "OP22 (id32): raw save layout of TALK_CONTACT_DATA");

// OP22 (id32): старый сейв - id был u16 в тех же байтах, старшие 16 бит - выравнивание (мусор)
inline void op22_fix_old_ids(TALK_CONTACT_VECTOR& v) // перегрузка, находится по ADL при разборе реестра
{
    for (auto& c : v)
        c.id &= 0xffff;
}

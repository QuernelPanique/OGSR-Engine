#pragma once
// OP22: CGBox - переносной контейнер ОП 2.2 (класс CL_GBOX: контейнеры артефактов, зарядник).
// Движок ОП (разбор xrGame.dll, 03.11): предмет без своей логики; get_user_data/set_user_data копируют пакет
// скрипта из/в серверный объект CSE_ALifeItemGameBox (ищется в alife по ID). Логика - binder_art_container.script.
#include "inventory_item_object.h"

class NET_Packet;

class CGBox : public CInventoryItemObject
{
public:
    void get_user_data(NET_Packet& P);
    void set_user_data(NET_Packet& P);
};

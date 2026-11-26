#pragma once
#include "object_interfaces.h"
#include "map_location_defs.h"

class CMapLocationWrapper;
class CInventoryOwner;
class CMapLocation;

class CMapManager
{
    CMapLocationWrapper* m_locations;

public:
    CMapManager();
    ~CMapManager();
    void Update();
    void initialize(u32 id); // OP22 (id32)
    Locations& Locations();
    CMapLocation* AddMapLocation(const shared_str& spot_type, u32 id);
    CMapLocation* AddRelationLocation(CInventoryOwner* pInvOwner);
    CMapLocation* AddUserLocation(const shared_str& spot_type, const shared_str& level_name, Fvector position);
    void RemoveMapLocation(const shared_str& spot_type, u32 id);
    u16 HasMapLocation(const shared_str& spot_type, u32 id);
    void RemoveMapLocationByObjectID(u32 id); // call on destroy object
    void RemoveMapLocation(CMapLocation* ml);
    CMapLocation* GetMapLocation(const shared_str& spot_type, u32 id);
    void DisableAllPointers();
#ifdef DEBUG
    void Dump();
#endif
};

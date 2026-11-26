#pragma once

class IDamageSource
{
public:
    virtual void SetInitiator(u32 id) = 0;
    virtual u32 Initiator() = 0; // OP22 (id32)
    virtual IDamageSource* cast_IDamageSource() { return this; }
};
#pragma once

#include "soundrender_Target.h"
#include "soundrender_CoreA.h"

class CSoundRender_TargetA : public CSoundRender_Target
{
    typedef CSoundRender_Target inherited;

    // OpenAL
    ALuint pSource;
    ALuint pBuffers[sdef_target_count]{};

    float cache_gain{};
    float cache_pitch{};

    ALuint buf_block{};

private:
    void fill_block(ALuint BufferID);

public:
    CSoundRender_TargetA();
    virtual ~CSoundRender_TargetA();

    virtual BOOL _initialize();
    virtual void _destroy();
    virtual void _restart();

    virtual void start(CSoundRender_Emitter* E);
    virtual void render();
    virtual void rewind();
    virtual void stop();
    virtual void update();

    void source_changed();

    virtual void fill_parameters(CSoundRender_Core* core);

    virtual void alAuxInit(ALuint slot) override;

private:
    // OP22 (26.11): звуки «в голове» (2D: КПК, интерфейс) - без эффекта помещения (эхо от писка КПК)
    ALuint m_op22_slot = 0;
    bool m_op22_dry = false;
};

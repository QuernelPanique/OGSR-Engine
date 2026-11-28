#include "stdafx.h"

#include "soundrender_TargetA.h"
#include "soundrender_emitter.h"
#include "soundrender_source.h"

#include <efx.h>

xr_vector<u8> g_target_temp_data;
// OP22 (28.11 perf): прежний логический размер для режима совместимости.
static u32 g_legacy_target_block{};

CSoundRender_TargetA::CSoundRender_TargetA() : CSoundRender_Target()
{
    cache_gain = 0.f;
    cache_pitch = 1.f;
    pSource = 0;
}

CSoundRender_TargetA::~CSoundRender_TargetA() {}

BOOL CSoundRender_TargetA::_initialize()
{
    // OP22 (28.11 perf): после создания источника кэш пуст.
    invalidate_parameters();
    inherited::_initialize();
    // initialize buffer
    A_CHK(alGenBuffers(sdef_target_count, pBuffers));
    alGenSources(1, &pSource);
    ALenum error = alGetError();
    if (AL_NO_ERROR == error)
    {
        A_CHK(alSourcei(pSource, AL_LOOPING, AL_FALSE));
        A_CHK(alSourcef(pSource, AL_MIN_GAIN, 0.f));
        A_CHK(alSourcef(pSource, AL_MAX_GAIN, 1.f));
        A_CHK(alSourcef(pSource, AL_GAIN, cache_gain));
        A_CHK(alSourcef(pSource, AL_PITCH, cache_pitch));
        return TRUE;
    }
    else
    {
        Msg("! sound: OpenAL: Can't create source. Error: %s.", (LPCSTR)alGetString(error));
        return FALSE;
    }
}

void CSoundRender_TargetA::alAuxInit(ALuint slot)
{
    m_op22_slot = slot; // OP22
    // OP22 (28.11 perf): при возврате EFX играющий 2D-звук сразу остаётся сухим.
    m_op22_dry = op22_snd_efx_toggle && slot != AL_EFFECTSLOT_NULL && m_pEmitter && m_pEmitter->b2D;
    A_CHK(alSource3i(pSource, AL_AUXILIARY_SEND_FILTER, m_op22_dry ? AL_EFFECTSLOT_NULL : slot, 0, AL_FILTER_NULL));
}

void CSoundRender_TargetA::_destroy()
{
    // clean up target
    if (alIsSource(pSource))
        alDeleteSources(1, &pSource);
    A_CHK(alDeleteBuffers(sdef_target_count, pBuffers));
}

void CSoundRender_TargetA::_restart()
{
    _destroy();
    _initialize();
}

void CSoundRender_TargetA::start(CSoundRender_Emitter* E)
{
    inherited::start(E);

    // OP22 (28.11 perf): смена эмиттера сбрасывает кэш, общий размер нужен только для старого пути.
    invalidate_parameters();
    g_legacy_target_block = sdef_target_block * E->source()->m_wformat.nAvgBytesPerSec / 1000;
    if (!op22_snd_pcm_block)
        g_target_temp_data.resize(g_legacy_target_block);
}

void CSoundRender_TargetA::render()
{
    for (u32 buf_idx = 0; buf_idx < sdef_target_count; buf_idx++)
        fill_block(pBuffers[buf_idx]);

    A_CHK(alSourceQueueBuffers(pSource, sdef_target_count, pBuffers));
    A_CHK(alSourcePlay(pSource));

    inherited::render();
}

void CSoundRender_TargetA::stop()
{
    // OP22 (28.11 perf): stop меняет относительный режим источника AL.
    invalidate_parameters();
    if (rendering)
    {
        A_CHK(alSourceStop(pSource));
        A_CHK(alSourcei(pSource, AL_BUFFER, NULL));
        A_CHK(alSourcei(pSource, AL_SOURCE_RELATIVE, TRUE));
    }
    inherited::stop();
}

void CSoundRender_TargetA::rewind()
{
    inherited::rewind();

    A_CHK(alSourceStop(pSource));
    A_CHK(alSourcei(pSource, AL_BUFFER, NULL));
    for (u32 buf_idx = 0; buf_idx < sdef_target_count; buf_idx++)
        fill_block(pBuffers[buf_idx]);
    A_CHK(alSourceQueueBuffers(pSource, sdef_target_count, pBuffers));
    A_CHK(alSourcePlay(pSource));
}

void CSoundRender_TargetA::update()
{
    inherited::update();

    // Msg("--[%s] bEFX is [%d]", __FUNCTION__, this->bEFX);

    if (bAlSoft)
    {
        ALint processed, state;

        /* Get relevant source info */
        alGetSourcei(pSource, AL_SOURCE_STATE, &state);
        alGetSourcei(pSource, AL_BUFFERS_PROCESSED, &processed);
        ALenum error = alGetError();
        if (error != AL_NO_ERROR)
        {
            Msg("!![%s]Error checking source state! OpenAL Error: [%s]", __FUNCTION__, alGetString(error));
            return;
        }

        while (processed > 0)
        {
            ALuint BufferID;
            A_CHK(alSourceUnqueueBuffers(pSource, 1, &BufferID));
            fill_block(BufferID);
            A_CHK(alSourceQueueBuffers(pSource, 1, &BufferID));
            processed--;
            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                Msg("!![%s]Error buffering data! OpenAL Error: [%s]", __FUNCTION__, alGetString(error));
                return;
            }
        }

        /* Make sure the source hasn't underrun */
        if (state != AL_PLAYING && state != AL_PAUSED)
        {
            ALint queued;

            /* If no buffers are queued, playback is finished */
            alGetSourcei(pSource, AL_BUFFERS_QUEUED, &queued);
            if (queued == 0)
                return;

            alSourcePlay(pSource);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                Msg("!![%s]Error restarting playback! OpenAL Error: [%s]", __FUNCTION__, alGetString(error));
                return;
            }
        }
    }
    else
    {
        ALint processed;
        // Get status
        A_CHK(alGetSourcei(pSource, AL_BUFFERS_PROCESSED, &processed));

        if (processed > 0)
        {
            while (processed)
            {
                ALuint BufferID;
                A_CHK(alSourceUnqueueBuffers(pSource, 1, &BufferID));
                fill_block(BufferID);
                A_CHK(alSourceQueueBuffers(pSource, 1, &BufferID));
                --processed;
            }
        }
        else
        {
            // processed == 0
            // check play status -- if stopped then queue is not being filled fast enough
            ALint state;
            A_CHK(alGetSourcei(pSource, AL_SOURCE_STATE, &state));
            if (state != AL_PLAYING)
            {
                //			Log		("Queuing underrun detected.");
                A_CHK(alSourcePlay(pSource));
            }
        }
    }
}

void CSoundRender_TargetA::fill_parameters(CSoundRender_Core* core)
{
#ifdef DEBUG
    CSoundRender_Emitter* SE = m_pEmitter;
    VERIFY(SE);
#endif

    inherited::fill_parameters(core);

    // OP22 (28.11 perf): проверяем каждое значение точно, без порога для 3D-параметров.
    VERIFY2(m_pEmitter, SE->source()->file_name());
    const auto& params = m_pEmitter->p_source;
    const AlParameters current{params.min_distance, params.max_distance, params.position, m_pEmitter->b2D, psSoundRolloff};
    const auto& last = m_al_parameters;
    const bool cache_enabled = op22_snd_al_cache;
    const bool refresh = !cache_enabled || !m_al_parameters_valid;
    bool changed = false;
    const auto set_float = [&](ALenum prop, float value, float old_value) {
        if (refresh || value != old_value)
        {
            A_CHK(alSourcef(pSource, prop, value));
            changed = true;
        }
    };
    set_float(AL_REFERENCE_DISTANCE, current.min_distance, last.min_distance);
    set_float(AL_MAX_DISTANCE, current.max_distance, last.max_distance);
    if (refresh || current.position.x != last.position.x || current.position.y != last.position.y || current.position.z != last.position.z)
    {
        A_CHK(alSource3f(pSource, AL_POSITION, current.position.x, current.position.y, -current.position.z));
        changed = true;
    }
    if (refresh || current.relative != last.relative)
    {
        A_CHK(alSourcei(pSource, AL_SOURCE_RELATIVE, current.relative));
        changed = true;
    }

    // OP22 (26.11): 2D-звук (в голове игрока) не уходит в эффект помещения; цель общая - переключаем по эмиттеру
    if (bEFX && m_op22_slot != AL_EFFECTSLOT_NULL && !!m_pEmitter->b2D != m_op22_dry)
    {
        m_op22_dry = !!m_pEmitter->b2D;
        A_CHK(alSource3i(pSource, AL_AUXILIARY_SEND_FILTER, m_op22_dry ? AL_EFFECTSLOT_NULL : m_op22_slot, 0, AL_FILTER_NULL));
    }

    set_float(AL_ROLLOFF_FACTOR, current.rolloff, last.rolloff);
    if (cache_enabled && changed)
    {
        const ALenum error = alGetError();
        m_al_parameters_valid = error == AL_NO_ERROR;
        if (m_al_parameters_valid)
            m_al_parameters = current;
        else
            Msg("! OpenAL: source parameters failed: %s", alGetString(error));
    }
    else if (!cache_enabled)
        invalidate_parameters();

    VERIFY2(m_pEmitter, SE->source()->file_name());
    float _gain = m_pEmitter->smooth_volume;
    clamp(_gain, EPS_S, 1.f);
    if (!fsimilar(_gain, cache_gain, 0.01f))
    {
        cache_gain = _gain;
        A_CHK(alSourcef(pSource, AL_GAIN, _gain));
    }

    VERIFY2(m_pEmitter, SE->source()->file_name());
    float _pitch = m_pEmitter->p_source.freq * psSoundTimeFactor; //--#SM+#-- Correct sound "speed" by time factor
    clamp(_pitch, EPS_L, 100.f); //--#SM+#-- Increase sound frequancy (speed) limit
    if (!fsimilar(_pitch, cache_pitch))
    {
        cache_pitch = _pitch;
        A_CHK(alSourcef(pSource, AL_PITCH, _pitch));
    }
    VERIFY2(m_pEmitter, SE->source()->file_name());
}

void CSoundRender_TargetA::fill_block(ALuint BufferID)
{
    R_ASSERT(m_pEmitter);

    // OP22 (28.11 perf): длительность блока определяет текущий источник, общий буфер хранит только данные.
    u32 block = g_legacy_target_block;
    if (op22_snd_pcm_block)
    {
        const auto& source_format = m_pEmitter->source()->m_wformat;
        const u32 frames = static_cast<u32>(u64{sdef_target_block} * source_format.nSamplesPerSec / 1000);
        block = frames * source_format.nBlockAlign;
    }
    if (g_target_temp_data.size() < block)
        g_target_temp_data.resize(block);
    m_pEmitter->fill_block(g_target_temp_data.data(), block);

    const auto& wvf = m_pEmitter->source()->m_wformat;
    const bool mono = wvf.nChannels == 1;

    ALuint format;
#ifdef AL_EXT_float32
    if (wvf.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        format = mono ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
    else
#endif
    {
        format = mono ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    }

    A_CHK(alBufferData(BufferID, format, g_target_temp_data.data(), block, wvf.nSamplesPerSec));
}
void CSoundRender_TargetA::source_changed()
{
    // OP22 (28.11 perf): при переходе к присоединённому источнику параметры отправятся заново.
    invalidate_parameters();
    dettach();
    attach();
}

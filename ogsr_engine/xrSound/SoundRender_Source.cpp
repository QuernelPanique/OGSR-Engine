#include "stdafx.h"

#include "soundrender_core.h"
#include "soundrender_source.h"

CSoundRender_Source::CSoundRender_Source()
{
    m_fMinDist = 1.f;
    m_fMaxDist = 300.f;
    m_fMaxAIDist = 300.f;
    m_fBaseVolume = 1.f;
    m_uGameType = 0;
    fname = 0;
    CAT.table = 0;
    CAT.size = 0;
}

CSoundRender_Source::~CSoundRender_Source() { unload(); }

// OP22 (28.11 perf): диагностика ошибки без повторного чтения той же строки PCM.
static void log_ov_read_error(long res)
{
    switch (res)
    {
    case OV_HOLE: Msg("! Vorbis: missing or corrupt data in the bitstream"); break;
    case OV_EBADLINK: Msg("! Vorbis: corrupt link in the bitstream"); break;
    default: Msg("! Vorbis: PCM read failed (%ld)", res); break;
    }
}

void CSoundRender_Source::i_decompress(OggVorbis_File* ovf, char* _dest, u32 size) const
{
    u32 decoded = 0;
    u32 recoverable = 0;

    // OP22 (28.11 perf): отрицательный код не двигает курсор, недочитанный PCM заполняем тишиной.
    // OV_HOLE/OV_EBADLINK - восстановимые (как раньше, читаем дальше), но не больше 16 раз подряд.
    while (decoded < size)
    {
        const auto ret = ov_read(ovf, _dest + decoded, static_cast<int>(size - decoded), 0, 2, 1, nullptr);
        if (ret <= 0)
        {
            if (ret < 0)
                log_ov_read_error(ret);
            if ((ret == OV_HOLE || ret == OV_EBADLINK) && ++recoverable < 16)
                continue;
            break;
        }
        recoverable = 0;
        decoded += static_cast<u32>(ret);
    }
    if (decoded < size)
        ZeroMemory(_dest + decoded, size - decoded);
}

void CSoundRender_Source::i_decompress(OggVorbis_File* ovf, float* _dest, u32 size) const
{
    // OP22 (28.11 perf): считаем только декодированные кадры и обнуляем остаток в байтах.
    s32 left = s32(size / m_wformat.nBlockAlign);
    const u32 remainder = size % m_wformat.nBlockAlign;
    u32 recoverable = 0;
    while (left)
    {
        float** pcm;
        long samples = ov_read_float(ovf, &pcm, left, nullptr);

        if (samples <= 0)
        {
            if (samples < 0)
                log_ov_read_error(samples);
            if ((samples == OV_HOLE || samples == OV_EBADLINK) && ++recoverable < 16)
                continue; // OP22 (28.11 perf): восстановимая ошибка - читаем дальше, как раньше
            break;
        }
        recoverable = 0;

        if (samples > left)
            samples = left;

        for (long j = 0; j < samples; j++)
            for (long i = 0; i < m_wformat.nChannels; i++)
                *_dest++ = clampr(pcm[i][j], -1.0f, 1.0f);

        left -= samples;
    }
    if (left || remainder)
        ZeroMemory(_dest, left * m_wformat.nBlockAlign + remainder);
}

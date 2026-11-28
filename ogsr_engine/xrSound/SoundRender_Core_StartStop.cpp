#include "stdafx.h"

#include "SoundRender_Core.h"
#include "SoundRender_Emitter.h"
#include "SoundRender_Target.h"
#include "SoundRender_Source.h"

// OP22 (28.11 perf): общий поиск минимума, при равенстве остаётся первая цель.
static CSoundRender_Target* lowest_priority_target(const xr_vector<CSoundRender_Target*>& targets)
{
    float Ptarget = flt_max;
    CSoundRender_Target* T = nullptr;
    for (CSoundRender_Target* candidate : targets)
    {
        if (candidate->priority < Ptarget)
        {
            T = candidate;
            Ptarget = candidate->priority;
        }
    }
    return T;
}

void CSoundRender_Core::i_start(CSoundRender_Emitter* E, CSoundRender_Target* T)
{
    R_ASSERT(E);

    // OP22 (28.11 perf): при выключенной оптимизации поиск выполняется на старте, как раньше.
    const float Ptest = E->priority();
    if (!T)
        T = lowest_priority_target(s_targets);

    // Stop currently playing
    if (T->get_emitter())
        T->get_emitter()->cancel();

    // Associate
    E->target = T;
    E->target->start(E);
    T->priority = Ptest;
}

void CSoundRender_Core::i_stop(CSoundRender_Emitter* E)
{
    // Msg					("- %10s : %3d[%1.4f] : %s","i_stop",E->dbg_ID,E->priority(),E->source->fname);
    R_ASSERT(E);
    R_ASSERT(E == E->target->get_emitter());
    E->target->stop();
    E->target = NULL;
}

void CSoundRender_Core::i_rewind(CSoundRender_Emitter* E)
{
    // Msg					("- %10s : %3d[%1.4f] : %s","i_rewind",E->dbg_ID,E->priority(),E->source->fname);
    R_ASSERT(E);
    R_ASSERT(E == E->target->get_emitter());
    E->target->rewind();
}

BOOL CSoundRender_Core::i_allow_play(CSoundRender_Emitter* E, CSoundRender_Target*& available)
{
    // Search available target
    float Ptest = E->priority();
    // OP22 (28.11 perf): сразу ищем цель для вытеснения с прежним строгим сравнением.
    if (op22_snd_target_search)
    {
        available = lowest_priority_target(s_targets);
        return available && available->priority < Ptest;
    }
    for (u32 it = 0; it < s_targets.size(); it++)
    {
        CSoundRender_Target* T = s_targets[it];
        if (T->priority < Ptest)
            return TRUE;
    }
    return FALSE;
}

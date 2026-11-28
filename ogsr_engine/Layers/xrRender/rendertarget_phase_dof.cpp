#include "stdafx.h"

// OP22 (28.11 perf): учитываем тепловизор и параметры того же прохода DOF.
bool CRenderTarget::needs_dof() const
{
    const auto& dof_params = shader_exports.get_dof_params();
    return ps_pnv_mode != 2 && ps_pnv_mode != 3 &&
        !(fis_zero(dof_params.x) && fis_zero(dof_params.y) && fis_zero(dof_params.z) && fis_zero(dof_params.w));
}

void CRenderTarget::phase_dof(CBackend& cmd_list)
{
    // OP22 (28.11 perf): условие совпадает с проверкой перед копированием глубины.
    if (!needs_dof())
        return;

    PIX_EVENT(phase_DOF);

    RenderScreenTriangle(cmd_list, rt_dof, s_dof->E[0]);
    RenderScreenTriangle(cmd_list, rt_Generic_combine, s_dof->E[1]);

	//Resolve RT
    HW.get_context(cmd_list.context_id)->CopyResource(rt_Postprocess_0->pSurface, rt_Generic_combine->pSurface);
}

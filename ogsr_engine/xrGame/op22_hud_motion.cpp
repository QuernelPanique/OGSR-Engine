#include "stdafx.h"
// OP22 (27.11): «живые» руки и оружие поверх старых анимаций (op22_hud_motion.h)
#include "op22_hud_motion.h"
#include <cmath>
#include "../Include/xrRender/Kinematics.h"
#include "../xr_3da/bone.h"
#include "CameraEffector.h"
#include "Actor.h"
#include "ActorEffector.h"

float g_op22_hud_smooth = 1.f;
float g_op22_cam_follow = 1.f;

#define eCEOp22Follow ((ECamEffectorType)(cefNext + 40))

namespace
{
constexpr float OP22_STEP = 0.004f; // подшаг интегрирования, с: одинаково при 30 и при 240 кадрах
constexpr float OP22_POSE_ZETA = 0.75f; // затухание поз: разгон/торможение и перелёт ~3 % - как у руки, без «желе»

IC float dot4(const Fvector4& a, const Fvector4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

IC void quat_of(const Fmatrix& m, Fvector4& o)
{
    Fquaternion q;
    q.set(m);
    q.normalize();
    o.set(q.x, q.y, q.z, q.w);
}

IC void store(op22_bone_filter& f, u16 i)
{
    Fquaternion q;
    q.set(f.rot[i].w, f.rot[i].x, f.rot[i].y, f.rot[i].z);
    f.out[i].rotation(q);
    f.out[i].c.set(f.pos[i]);
}

void write(IKinematics* K, op22_bone_filter& f)
{
    for (u16 i = 0; i < f.count; ++i)
    {
        if (!K->LL_GetBoneVisible(i))
            continue;
        CBoneInstance& bi = K->LL_GetBoneInstance(i);
        bi.mTransform.set(f.out[i]);
        bi.mRenderTransform.mul_43(f.out[i], K->LL_GetData(i).m2b_transform);
    }
}

void snap(IKinematics* K, op22_bone_filter& f, u16 i)
{
    const Fmatrix& T = K->LL_GetBoneInstance(i).mTransform;
    f.pos[i].set(T.c);
    f.vel[i].set(0.f, 0.f, 0.f);
    quat_of(T, f.rot[i]);
    f.avel[i].set(0.f, 0.f, 0.f, 0.f);
    f.out[i].set(T);
}
} // namespace

void op22_hud_filter_bones(IKinematics* K, op22_bone_filter& f, float weight)
{
    if (!K)
        return;
    const u16 n = K->LL_BoneCount();
    if (g_op22_hud_smooth <= 0.f)
    {
        f.count = 0; // выключено: при включении - с текущей позы, без рывка
        return;
    }
    const bool same_model = f.owner == K && f.count == n;
    if (same_model && f.frame == Device.dwFrame)
    {
        write(K, f); // кости посчитаны в этом кадре ещё раз (двойной апдейт худа) - тот же результат
        return;
    }
    const float dt = Device.fTimeDelta;
    if (!same_model || dt > 0.25f)
    {
        // новая модель в руках или долгая пауза (загрузка, меню) - начать с текущей позы
        f.owner = K;
        f.count = n;
        f.pos.resize(n);
        f.vel.resize(n);
        f.rot.resize(n);
        f.avel.resize(n);
        f.out.resize(n);
        for (u16 i = 0; i < n; ++i)
            snap(K, f, i);
        f.frame = Device.dwFrame;
        return;
    }
    f.frame = Device.dwFrame;

    // собственная частота: лёгкий пистолет ~10 Гц (запаздывание ~0,02 с), винтовка ~7 Гц, пулемёт ~5 Гц
    float hz = 9.f * std::pow(1.5f / _max(weight, 0.5f), 0.3f);
    clamp(hz, 5.f, 11.f);
    hz /= g_op22_hud_smooth;
    const float w = PI_MUL_2 * hz, k = w * w, c = 2.f * OP22_POSE_ZETA * w;
    const int steps = _max(1, iCeil(dt / OP22_STEP));
    const float h = dt / steps;

    for (u16 i = 0; i < n; ++i)
    {
        const Fmatrix& T = K->LL_GetBoneInstance(i).mTransform;
        Fvector tp = T.c;
        Fvector4 tq;
        quat_of(T, tq);
        Fvector4& q = f.rot[i];
        Fvector4& av = f.avel[i];
        if (dot4(q, tq) < 0.f)
            tq.set(-tq.x, -tq.y, -tq.z, -tq.w); // тот же поворот, ближняя полусфера
        // анимация мгновенно перенесла кость (смена набора, скрипт) - догнать сразу, а не лететь через кадр
        if (f.pos[i].distance_to(tp) > 0.35f || dot4(q, tq) < 0.8f)
        {
            snap(K, f, i);
            continue;
        }
        Fvector& p = f.pos[i];
        Fvector& v = f.vel[i];
        for (int s = 0; s < steps; ++s)
        {
            // положение: x'' = k (цель - x) - c x'
            Fvector a;
            a.sub(tp, p).mul(k);
            a.mad(v, -c);
            v.mad(a, h);
            p.mad(v, h);
            // поворот: та же пружина на единичной сфере кватернионов (малые углы - точная угловая пружина)
            Fvector4 aq;
            aq.set(k * (tq.x - q.x) - c * av.x, k * (tq.y - q.y) - c * av.y, k * (tq.z - q.z) - c * av.z, k * (tq.w - q.w) - c * av.w);
            av.set(av.x + aq.x * h, av.y + aq.y * h, av.z + aq.z * h, av.w + aq.w * h);
            q.set(q.x + av.x * h, q.y + av.y * h, q.z + av.z * h, q.w + av.w * h);
            const float len = _sqrt(dot4(q, q));
            if (len > EPS_S)
                q.set(q.x / len, q.y / len, q.z / len, q.w / len);
            const float rad = dot4(av, q); // скорость - касательная к сфере
            av.set(av.x - rad * q.x, av.y - rad * q.y, av.z - rad * q.z, av.w - rad * q.w);
        }
        store(f, i);
    }
    write(K, f);
}

// ------------------------------------------------------------------ камера за действием
namespace
{
struct cam_follow_state
{
    Fquaternion base; // среднее положение ствола (медленно догоняет текущее)
    bool has_base{};
    Fvector target{}, ang{}, vel{}; // тангаж, рыскание, крен (рад)
    u32 frame{u32(-1)};
    float last_feed{-1.f};
} g_cf;

constexpr float OP22_CAM_HZ = 3.5f; // камера мягче рук: голова доворачивается за руками без дрожи
constexpr float OP22_CAM_ZETA = 0.85f;
constexpr float OP22_CAM_BASE_TAU = 0.45f; // «среднее» положение ствола - за ~полсекунды
constexpr float OP22_CAM_GAIN = 0.10f; // доля поворота ствола, переданная камере
const float OP22_CAM_MAX = deg2rad(1.5f), OP22_CAM_MAX_ROLL = deg2rad(2.f);

class CEffectorOp22Follow : public CEffectorCam
{
public:
    CEffectorOp22Follow() : CEffectorCam(eCEOp22Follow, 100000.f) {}

    BOOL ProcessCam(SCamEffectorInfo& info) override
    {
        fLifeTime = 100000.f; // бессрочно
        float dt = Device.fTimeDelta;
        clamp(dt, 0.f, 0.05f);
        if (Device.fTimeGlobal - g_cf.last_feed > 0.2f || g_op22_cam_follow <= 0.f)
            g_cf.target.set(0.f, 0.f, 0.f); // оружия в руках нет / выключено - плавно вернуться
        const float w = PI_MUL_2 * OP22_CAM_HZ, k = w * w, c = 2.f * OP22_CAM_ZETA * w;
        const int steps = _max(1, iCeil(dt / OP22_STEP));
        const float h = dt / steps;
        for (int s = 0; s < steps; ++s)
        {
            Fvector a;
            a.sub(g_cf.target, g_cf.ang).mul(k);
            a.mad(g_cf.vel, -c);
            g_cf.vel.mad(a, h);
            g_cf.ang.mad(g_cf.vel, h);
        }
        if (g_cf.ang.square_magnitude() < 1e-10f)
            return TRUE;

        // повороты вокруг осей камеры (явно: вправо r = верх x вперёд)
        Fvector d = info.d, n = info.n, r;
        r.crossproduct(n, d).normalize_safe();
        const float p = g_cf.ang.x, y = g_cf.ang.y, b = g_cf.ang.z;
        Fvector d1, n1, r1, n2;
        d1.mul(d, _cos(p)).mad(n, _sin(p)); // тангаж: + вверх
        n1.mul(n, _cos(p)).mad(d, -_sin(p));
        Fvector d2;
        d2.mul(d1, _cos(y)).mad(r, _sin(y)); // рыскание: + вправо
        r1.mul(r, _cos(y)).mad(d1, -_sin(y));
        n2.mul(n1, _cos(b)).mad(r1, _sin(b)); // крен: + верх вправо
        d2.normalize_safe();
        Fvector r2;
        r2.crossproduct(n2, d2).normalize_safe();
        n2.crossproduct(d2, r2).normalize_safe();
        info.d.set(d2);
        info.n.set(n2);
        return TRUE;
    }
};
} // namespace

void op22_cam_follow_feed(IKinematics* K, u16 bone, const Fmatrix& xform, float zoom_factor, bool firing, bool moving)
{
    if (!K || bone == BI_NONE || bone >= K->LL_BoneCount())
        return;
    if (g_cf.frame == Device.dwFrame)
        return;
    g_cf.frame = Device.dwFrame;
    g_cf.last_feed = Device.fTimeGlobal;

    CActor* A = Actor();
    if (!A)
        return;
    if (!A->Cameras().GetCamEffector(eCEOp22Follow))
        A->Cameras().AddCamEffector(xr_new<CEffectorOp22Follow>());
    if (g_op22_cam_follow <= 0.f)
        return;

    const Fmatrix& B = K->LL_GetTransform(bone); // кость ствола в пространстве модели: только анимация, без мыши
    Fquaternion qc;
    qc.set(B);
    qc.normalize();
    const float dt = Device.fTimeDelta;
    if (!g_cf.has_base || dt > 0.25f)
    {
        g_cf.base.set(qc);
        g_cf.has_base = true;
        g_cf.target.set(0.f, 0.f, 0.f);
        return;
    }
    if (qc.w * g_cf.base.w + qc.x * g_cf.base.x + qc.y * g_cf.base.y + qc.z * g_cf.base.z < 0.f)
        qc.set(-qc.w, -qc.x, -qc.y, -qc.z);
    Fquaternion nb;
    nb.slerp(g_cf.base, qc, 1.f - std::exp(-dt / OP22_CAM_BASE_TAU));
    g_cf.base.set(nb);

    // поворот «среднее -> сейчас» вектора, жёстко связанного с костью: Mc * Mb^-1
    Fmatrix Mb, Mbi, Mc, xi;
    Mb.rotation(g_cf.base);
    Mbi.invert(Mb);
    Mc.rotation(qc);
    xi.invert(xform);
    Fvector F, U, f1, u1;
    xi.transform_dir(F, Device.vCameraDirection); // взгляд камеры в осях модели
    xi.transform_dir(U, Device.vCameraTop);
    F.normalize_safe();
    U.normalize_safe();
    Mbi.transform_dir(f1, F);
    Mc.transform_dir(f1);
    Mbi.transform_dir(u1, U);
    Mc.transform_dir(u1);
    xform.transform_dir(f1); // обратно в мир и в оси камеры
    xform.transform_dir(u1);
    const float fx = f1.dotproduct(Device.vCameraRight), fy = f1.dotproduct(Device.vCameraTop), fz = f1.dotproduct(Device.vCameraDirection);
    const float ux = u1.dotproduct(Device.vCameraRight), uy = u1.dotproduct(Device.vCameraTop);

    float gain = OP22_CAM_GAIN * g_op22_cam_follow;
    gain *= 1.f - _min(1.f, _max(0.f, zoom_factor)); // в прицеливании - нет: точка не уходит
    if (firing)
        gain *= 0.25f; // отдачу камеры делает выстрел ОП
    if (moving)
        gain *= 0.6f; // на ходу качается сама походка
    g_cf.target.set(atan2f(fy, fz) * gain, atan2f(fx, fz) * gain, atan2f(ux, uy) * gain * 1.2f);
    clamp(g_cf.target.x, -OP22_CAM_MAX, OP22_CAM_MAX);
    clamp(g_cf.target.y, -OP22_CAM_MAX, OP22_CAM_MAX);
    clamp(g_cf.target.z, -OP22_CAM_MAX_ROLL, OP22_CAM_MAX_ROLL);
}

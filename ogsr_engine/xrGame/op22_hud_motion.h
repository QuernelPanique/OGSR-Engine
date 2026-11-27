#pragma once
// OP22 (27.11): «живые» руки и оружие поверх старых анимаций - без правки самих анимаций.
//  1. Сглаживание и пружина поз (op22_hud_smooth): каждая кость модели худа в пространстве модели не встаёт в позу
//     анимации мгновенно, а догоняет её пружиной с затуханием (2-й порядок). Резкие старты/остановки линейных
//     анимаций ОП получают разгон, торможение и едва заметный перелёт; рывки срезаются. Все кости фильтруются
//     одинаково - руки и ствол не расходятся. Частота пружины - от веса предмета (тяжёлый ствол инертнее).
//  2. Камера за действием (op22_cam_follow): малая доля поворота ствола относительно его среднего положения
//     (перезарядка, доставание, передёргивание) передаётся камере; в прицеливании гаснет, при стрельбе слабее.
// Консоль: op22_hud_smooth 0..3 (0 - выкл, 1 - норма, больше - тяжелее/плавнее), op22_cam_follow 0..3 (0 - выкл).

class IKinematics;

struct op22_bone_filter
{
    xr_vector<Fvector> pos, vel; // положение кости и скорость, пространство модели
    xr_vector<Fvector4> rot, avel; // кватернион кости как 4-вектор и его скорость
    xr_vector<Fmatrix> out; // результат кадра (для повторного расчёта костей в том же кадре)
    xr_vector<Fvector> tp, raw; // OP22 (27.11): цель после предфильтра (плавный старт) и сырая поза анимации (трасса)
    xr_vector<Fvector4> tq;
    const void* owner{};
    u16 count{};
    u32 frame{u32(-1)};
    float var{1.f}; // жёсткость пружины этого действия (вариативность, op22_hud_variety)
};

extern float g_op22_hud_smooth;
extern float g_op22_cam_follow;
extern float g_op22_hud_variety; // OP22 (27.11): манера действия, 0 - выкл
extern float g_op22_body_state; // OP22 (27.11): влияние состояния тела на дыхание и тремор, 0 - выкл
extern float g_op22_hud_motor; // OP22 (27.11): моторика руки - плавный старт поз, поправки вместо «звона», 0 - как 3.250
extern float g_op22_hud_trace; // OP22 (27.11): запись траектории худа в userdata/logs, 0 - выкл

// OP22 (27.11): какая кость пишется в трассу (основной предмет; ставит player_hud раз в кадр)
struct op22_trace_ref
{
    const op22_bone_filter* f{};
    u16 bone{u16(-1)};
};
extern op22_trace_ref g_op22_trace_ref;

// после CalculateBones модели худа: фильтр поз; weight - масса предмета в руках, кг
void op22_hud_filter_bones(IKinematics* K, op22_bone_filter& f, float weight);

// раз в кадр, основной предмет: поза ствола (кость bone модели K, модель -> мир: xform) для камеры
void op22_cam_follow_feed(IKinematics* K, u16 bone, const Fmatrix& xform, float zoom_factor, bool firing, bool moving);

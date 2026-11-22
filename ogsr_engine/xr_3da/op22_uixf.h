#pragma once

// OP22: перспектива и движение плоских окон интерфейса (HUD GAMMA 2.0, 22.11): панель слева внизу «висит в воздухе»
// и трясётся от движения ГГ, счётчик патронов всплывает у оружия. Окно с m_op22_xf = N рисуется (вместе с детьми)
// при включённой группе N: каждая вершина интерфейса и шрифта проходит через map() - так же, как CSS макета:
//   анимация (translate, rotate, scale вокруг центра коробки) -> rotateY * rotateX * rotateZ вокруг (ox, oy)
//   -> perspective(d) вокруг (pox, poy). Координаты - пиксели экрана, коробка (bx, by, w, h) - плоское окно.
// W - глубина вершины: рендер кладёт её в w вершины, текстура ложится с поправкой на перспективу.

struct op22_uixf_t
{
    bool on{};
    float bx{}, by{}, w{}, h{};    // коробка (плоско), px экрана
    float tx{}, ty{}, rot{}, sc{1.f}; // анимация вокруг центра коробки: px, рад, масштаб
    float ry{}, rx{}, rz{};        // поворот, рад (CSS rotateY rotateX rotateZ)
    float ox{}, oy{};              // точка поворота от левого верхнего угла коробки, px
    float pox{}, poy{};            // точка перспективы от левого верхнего угла коробки, px
    float d{1000.f};               // perspective, px
    float alpha{1.f};              // множитель непрозрачности

    // коэффициенты (prepare)
    float m_ar{1.f}, m_ai{}, m_r[9]{1.f, 0, 0, 0, 1.f, 0, 0, 0, 1.f};

    void prepare()
    {
        m_ar = sc * _cos(rot);
        m_ai = sc * _sin(rot);
        const float cy = _cos(ry), sy = _sin(ry), cx = _cos(rx), sx = _sin(rx), cz = _cos(rz), sz = _sin(rz);
        // RY * RX * RZ (векторы-столбцы, y вниз, z к зрителю - как в CSS)
        const float RY[9] = {cy, 0, sy, 0, 1, 0, -sy, 0, cy};
        const float RX[9] = {1, 0, 0, 0, cx, -sx, 0, sx, cx};
        const float RZ[9] = {cz, -sz, 0, sz, cz, 0, 0, 0, 1};
        float T[9];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                T[i * 3 + j] = RY[i * 3 + 0] * RX[0 * 3 + j] + RY[i * 3 + 1] * RX[1 * 3 + j] + RY[i * 3 + 2] * RX[2 * 3 + j];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m_r[i * 3 + j] = T[i * 3 + 0] * RZ[0 * 3 + j] + T[i * 3 + 1] * RZ[1 * 3 + j] + T[i * 3 + 2] * RZ[2 * 3 + j];
    }

    void map(float x, float y, float& X, float& Y, float& W) const
    {
        const float cx = w * .5f, cy = h * .5f;
        const float dx = x - bx - cx, dy = y - by - cy;
        const float ax = cx + tx + m_ar * dx - m_ai * dy - ox;
        const float ay = cy + ty + m_ai * dx + m_ar * dy - oy;
        const float qx = m_r[0] * ax + m_r[1] * ay + ox;
        const float qy = m_r[3] * ax + m_r[4] * ay + oy;
        const float qz = m_r[6] * ax + m_r[7] * ay;
        W = 1.f - qz / d;
        if (W < .05f)
            W = .05f;
        X = bx + pox + (qx - pox) / W;
        Y = by + poy + (qy - poy) / W;
    }

    u32 color(u32 c) const
    {
        if (alpha >= 1.f)
            return c;
        const u32 a = u32(float(c >> 24) * (alpha < 0.f ? 0.f : alpha));
        return (c & 0x00ffffff) | (a << 24);
    }
};

ENGINE_API extern op22_uixf_t g_op22_uixf; // текущее (on - пока рисуется группа)
ENGINE_API extern op22_uixf_t g_op22_uixf_group[4]; // группы 1..3, заполняет игра каждый кадр (on - группа активна)

// окно группы g: true - рисовать (группа выключена - рисуется плоско; прозрачная - не рисуется)
struct op22_uixf_scope
{
    op22_uixf_t prev;
    bool draw{true};
    explicit op22_uixf_scope(u8 g) : prev(g_op22_uixf)
    {
        if (g && g < 4 && g_op22_uixf_group[g].on)
        {
            g_op22_uixf = g_op22_uixf_group[g];
            draw = g_op22_uixf.alpha > 0.004f;
        }
    }
    ~op22_uixf_scope() { g_op22_uixf = prev; }
};

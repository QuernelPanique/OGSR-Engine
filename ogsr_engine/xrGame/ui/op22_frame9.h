#pragma once

// OP22: GAMMA 2.0 (22.11) - рамка из одной текстуры квадратом N = 2C + P px (доска 2560x1440): углы C x C, края и середина -
// плиткой по P px (пунктир не растягивается). Рисуется прямо вершинами в пикселях экрана - без CUIFrameWindow/CUIFrameRect,
// у которых созданные в коде рамки уезжали на поле свечения (22.11, скриншоты пользователя: видны только правый и нижний край).
// Спецификация в разметке: "текстура,C,P".

#include "../ui_defs.h"
#include "../ui_base.h"
#include "../../Include/xrRender/UIRender.h"
#include "../../Include/xrRender/UIShader.h"

extern int g_op22_frame9_cache; // OP22 (28.11 perf): переиспользование раскладки рамки.

struct op22_frame9
{
    ui_shader sh;
    float C{}, P{};
    bool ok{};

private:
    // OP22 (28.11 perf): вершины берутся из декартова произведения двух закэшированных осей.
    struct seg
    {
        float s0, s1, t0, t1;
    };
    struct axis
    {
        float lo{}, hi{};
        xr_vector<seg> segments;
    };
    mutable axis cached_x, cached_y;
    mutable float cached_c{}, cached_p{}, cached_C{}, cached_P{};

public:
    void init(LPCSTR spec)
    {
        ok = false;
        if (!spec || !spec[0])
            return;
        string256 tex{};
        if (sscanf(spec, "%255[^,],%f,%f", tex, &C, &P) != 3 || C <= 0.f || P <= 0.f)
            return;
        sh->create("hud\\default", tex);
        ok = true;
    }

    // r - прямоугольник рамки в единицах окна (уже с полем под свечение)
    void draw(const Frect& r) const
    {
        if (!ok)
            return;
        Fvector2 a, b;
        UI()->ClientToScreenScaled(a, r.x1, r.y1);
        UI()->ClientToScreenScaled(b, r.x2, r.y2);
        const float k = float(Device.dwHeight) / 1440.f;
        const float N = 2.f * C + P;
        float c = C * k;
        const float p = _max(1.f, P * k);
        c = _min(c, _min((b.x - a.x) * .5f, (b.y - a.y) * .5f));
        if (c <= 0.f)
            return;
        auto split = [&](float lo, float hi, xr_vector<seg>& out) {
            out.clear();
            out.push_back({lo, lo + c, 0.f, C / N});
            for (float s = lo + c; s < hi - c - 0.01f; s += p)
            {
                const float e = _min(s + p, hi - c);
                out.push_back({s, e, C / N, (C + P * (e - s) / p) / N});
            }
            out.push_back({hi - c, hi, (C + P) / N, 1.f});
        };
        // OP22 (28.11 perf): сравниваем точные экранные координаты и параметры раскладки, включая масштаб.
        static xr_vector<seg> scratch_x, scratch_y;
        const bool cache = g_op22_frame9_cache != 0;
        auto& xs = cache ? cached_x.segments : scratch_x;
        auto& ys = cache ? cached_y.segments : scratch_y;
        if (cache)
        {
            const bool same_tiles = cached_c == c && cached_p == p && cached_C == C && cached_P == P;
            if (!same_tiles || cached_x.lo != a.x || cached_x.hi != b.x)
            {
                split(a.x, b.x, xs);
                cached_x.lo = a.x;
                cached_x.hi = b.x;
            }
            if (!same_tiles || cached_y.lo != a.y || cached_y.hi != b.y)
            {
                split(a.y, b.y, ys);
                cached_y.lo = a.y;
                cached_y.hi = b.y;
            }
            cached_c = c;
            cached_p = p;
            cached_C = C;
            cached_P = P;
        }
        else
        {
            split(a.x, b.x, xs);
            split(a.y, b.y, ys);
        }
        UIRender->SetShader(*sh);
        UIRender->StartPrimitive(u32(xs.size() * ys.size() * 6), IUIRender::ptTriList, UI()->m_currentPointType);
        for (const seg& X : xs)
            for (const seg& Y : ys)
            {
                UIRender->PushPoint(X.s0, Y.s0, 0, 0xffffffff, X.t0, Y.t0);
                UIRender->PushPoint(X.s1, Y.s0, 0, 0xffffffff, X.t1, Y.t0);
                UIRender->PushPoint(X.s1, Y.s1, 0, 0xffffffff, X.t1, Y.t1);
                UIRender->PushPoint(X.s0, Y.s0, 0, 0xffffffff, X.t0, Y.t0);
                UIRender->PushPoint(X.s1, Y.s1, 0, 0xffffffff, X.t1, Y.t1);
                UIRender->PushPoint(X.s0, Y.s1, 0, 0xffffffff, X.t0, Y.t1);
            }
        UIRender->FlushPrimitive();
    }

    void draw(const Frect& r, const Fvector2& pad) const
    {
        Frect q;
        q.set(r.x1 - pad.x, r.y1 - pad.y, r.x2 + pad.x, r.y2 + pad.y);
        draw(q);
    }
};

#include "UIWindow.h"

// окно-рамка op22_frame9 (ребёнок окна: рисуется в порядке детей, например под подписью)
class CUIOp22Frame9 : public CUIWindow
{
public:
    op22_frame9 f;
    void Draw() override
    {
        Frect r;
        GetAbsoluteRect(r);
        f.draw(r);
        CUIWindow::Draw();
    }
};

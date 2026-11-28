#pragma once

#include "xr_fixed_map.h"

class dxRender_Visual;

namespace R_dsgraph
{

// Elementary types
struct _NormalItem
{
    float ssa{};
    dxRender_Visual* pVisual{};
};

struct _TreeItem
{
    dxRender_Visual* pVisual;
    xr_vector<FloraVertData*> data;
};

using vs_type = SVS*;
using gs_type = SGS*;
using ps_type = SPS*;
using hs_type = SHS*;
using ds_type = SDS*;

struct pass_ptr_less
{
    bool operator()(const SPass* lhs, const SPass* rhs) const
    {
        if (lhs->vs != rhs->vs)
            return lhs->vs < rhs->vs;

        if (lhs->gs != rhs->gs)
            return lhs->gs < rhs->gs;

        if (lhs->ps != rhs->ps)
            return lhs->ps < rhs->ps;

        if (lhs->hs != rhs->hs)
            return lhs->hs < rhs->hs;

        if (lhs->ds != rhs->ds)
            return lhs->ds < rhs->ds;

        if (lhs->constants != rhs->constants)
            return lhs->constants < rhs->constants;

        if (lhs->state != rhs->state)
            return lhs->state < rhs->state;

        return lhs->T < rhs->T;     
    }
};

// NORMAL
using mapNormalDirect = xr_vector<_NormalItem>;
class mapNormalItems
{
public:
    float ssa{};
    mapNormalDirect* items{};
    xr_unordered_map<u32, _TreeItem>* trees{};

    mapNormalItems()
    {
        items = xr_new<mapNormalDirect>();
        trees = xr_new<xr_unordered_map<u32, _TreeItem>>();
    }

    ~mapNormalItems()
    {
        xr_delete(items);
        xr_delete(trees);
    }

    // OP22 (28.11 perf): сохраняем ёмкость контейнеров, сбрасываем данные и SSA прохода.
    void clear()
    {
        items->clear();
        trees->clear();
        ssa = 0.f;
    }
};

struct _MatrixItem
{
    float ssa{};
    IRenderable* pObject{};
    dxRender_Visual* pVisual{};
    Fmatrix Matrix{}; // matrix (copy)
};

struct _MatrixItemS
{
    // Хак для использования списков инициализации
    // Не используем наследование

    // _MatrixItem begin
    float ssa{};
    IRenderable* pObject{};
    dxRender_Visual* pVisual{};
    Fmatrix Matrix{}; // matrix (copy)
    // _MatrixItem end

    ShaderElement* se;
};

// MATRIX
using mapMatrixDirect = xr_vector<_MatrixItem>;
class mapMatrixItems
{
public:
    mapMatrixDirect* items{};

    mapMatrixItems() { items = xr_new<mapMatrixDirect>(); }

    ~mapMatrixItems() { xr_delete(items); }

    // OP22 (28.11 perf): сохраняем буфер объектов для следующего прохода.
    void clear() { items->clear(); }
};

// OP22 (28.11 perf): сохраняем извлечённые узлы без ссылок на шейдеры и визуалы.
// У каждой карты каждого контекста свой пул. Сравниваются только ключи активного прохода.
template <class Items>
class ReusablePassMap
{
    using Map = xr_map<SPass*, Items, pass_ptr_less>;
    Map m_active;
    xr_vector<typename Map::node_type> m_free;

public:
    using const_iterator = typename Map::const_iterator;

    bool empty() const { return m_active.empty(); }
    auto size() const { return m_active.size(); }
    const_iterator begin() const { return m_active.cbegin(); }
    const_iterator end() const { return m_active.cend(); }

    Items& operator[](SPass* pass)
    {
        const auto pos = m_active.lower_bound(pass);
        if (pos != m_active.end() && !m_active.key_comp()(pass, pos->first))
            return pos->second;

        if (m_free.empty())
            return m_active.try_emplace(pos, pass)->second;

        auto node = std::move(m_free.back());
        m_free.pop_back();
        node.key() = pass;
        return m_active.insert(pos, std::move(node))->second;
    }

    void clear()
    {
        m_free.reserve(m_free.size() + m_active.size());
        while (!m_active.empty())
        {
            auto node = m_active.extract(m_active.begin());
            node.mapped().clear();
            node.key() = nullptr;
            m_free.push_back(std::move(node));
        }
    }

    void release_cached_nodes() { m_free.clear(); } // OP22 (28.11 perf): освобождаем буферы прежнего уровня.
};

using mapNormal_T = ReusablePassMap<mapNormalItems>;
using mapNormalPasses_T = mapNormal_T[SHADER_PASSES_MAX];
using mapMatrix_T = ReusablePassMap<mapMatrixItems>;
using mapMatrixPasses_T = mapMatrix_T[SHADER_PASSES_MAX];

// Top level
using mapSorted_T = xr_fixed_map<float, _MatrixItemS>;
using mapSortedLarge_T = mapMatrix_T;

// LOD
struct _LodItem
{
    float ssa;
    dxRender_Visual* pVisual;
};

} // namespace R_dsgraph

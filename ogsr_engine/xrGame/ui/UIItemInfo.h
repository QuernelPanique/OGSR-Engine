#pragma once
#include "uiwindow.h"

class CInventoryItem;
class CUIStatic;
class CUIScrollView;
class CUIProgressBar;
class CUIWpnParams;
class CUIArtefactParams;
class CPhysicsShellHolder;

// extern const char * const 		fieldsCaptionColor;

class CUIItemInfo : public CUIWindow
{
private:
    typedef CUIWindow inherited;
    struct _desc_info
    {
        CGameFont* pDescFont;
        u32 uDescClr;
        bool bShowDescrText;
    } m_desc_info{};
    CInventoryItem* m_pInvItem;

public:
    CUIItemInfo();
    virtual ~CUIItemInfo();

    void Init(float x, float y, float width, float height, LPCSTR xml_name);
    void Init(LPCSTR xml_name);
    void InitItem(CInventoryItem* pInvItem);

    void TryAddWpnInfo(CPhysicsShellHolder& obj);
    void TryAddArtefactInfo(const shared_str& af_section);
    void TryAddCustomInfo(CPhysicsShellHolder& obj);

    virtual void Draw();
    bool m_b_force_drawing;
    CUIStatic* UIName;
    CUIStatic* UIWeight;
    CUIStatic* UICost;
    CUIStatic* UICondition;
    CUIScrollView* UIDesc;
    CUIProgressBar* UICondProgresBar;
    CUIWpnParams* UIWpnParams;
    CUIArtefactParams* UIArtefactParams;

    Fvector2 UIItemImageSize;
    // OP22: картинка вписана в поле image_static (op22_fit="1") и стоит по его центру
    bool m_op22_fit{};
    // OP22: плавающее окно по высоте содержимого (main_frame op22_autoh="1"): низ - конец описания + отступ, не выше максимума
    bool m_op22_autoh{};
    float m_op22_max_h{}, m_op22_pad_b{};
    class CUIFrameWindow* m_op22_back{};
    // OP22: шкала состояния только у предметов с состоянием (condition_progress op22_cond_auto="1")
    bool m_op22_cond_auto{};
    float m_op22_cond_h{}, m_op22_desc_y{}, m_op22_desc_h{};
    CUIStatic* m_op22_cond_lbl{};
    CUIStatic* m_op22_cond_val{}; // OP22: GAMMA 2.0 - «64» справа от пунктира
    bool m_op22_weight_fmt{}; // OP22: GAMMA 2.0 - вес «4,9 кг» (static_weight op22_fmt)
    float m_op22_cost_gap{-1.f}; // OP22: GAMMA 2.0 - цена сразу за текстом веса (static_cost op22_after_weight)
    float m_op22_line_h{};
    bool m_op22_wpn_fixed{}; // OP22: GAMMA 2.0 - параметры оружия своим окном в точке разметки (wpn_params op22_fixed), не в прокрутке
    float m_op22_w0{-1.f}, m_op22_desc_w0{-1.f}; // OP22: исходные ширины окна и списка (при прокрутке окно шире на полосу) // OP22: GAMMA 2.0 - высота строки описания (descr_list op22_line_h, единицы окна)
    Fvector2 m_op22_img_pos{};
    // OP22: высота имени по тексту (static_name op22_name_auto="1"): длинное имя в две строки сдвигает вниз вес, цену,
    // состояние и описание, а не наезжает на них
    bool m_op22_name_auto{}, m_op22_base_ok{};
    int m_op22_name_shift{5}; // OP22: сколько окон под именем сдвигать (op22_name_auto="2" - только вес и цена: строка состояния в другой колонке)
    float m_op22_name_h{}, m_op22_base_y[5]{};
    CUIStatic* UIItemImage;
};
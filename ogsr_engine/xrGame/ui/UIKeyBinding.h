#pragma once
#include "UIWindow.h"
#include "UILabel.h"
#include "UIListWnd.h"
#include "UIFrameWindow.h"
#include "UIEditBox.h"

class CUIXml;
class CUIScrollView;

class CUIKeyBinding : public CUIWindow
{
public:
    CUIKeyBinding();
    void InitFromXml(CUIXml& xml_doc, LPCSTR path);
#ifdef DEBUG
    void CheckStructure(CUIXml& xml_doc);
    bool IsActionExist(LPCSTR action, CUIXml& xml_doc);
#endif
protected:
    void FillUpList(CUIXml& xml_doc, LPCSTR path);

    CUILabel m_header[3];
    CUIFrameWindow m_frame;
    CUIScrollView* m_scroll_wnd{};
    // OP22: таблица по макету - op22_cols="2" (пары действий в строке, вторая колонка со сдвигом op22_col2_x), без групп
    void op22_fill(CUIXml& xml_doc_ui, LPCSTR path_ui);
};
#include "stdafx.h"
#include "UITaskDescrWnd.h"
#include "UIXmlInit.h"
#include "UIFrameWindow.h"
#include "UIFrameLineWnd.h"
#include "UIScrollView.h"
#include "UIEncyclopediaArticleWnd.h"
#include "../encyclopedia_article.h"
#include "../gametask.h"
#include "../string_table.h"
#include "UIStatic.h"

CUITaskDescrWnd::CUITaskDescrWnd() {}

CUITaskDescrWnd::~CUITaskDescrWnd() {}

void CUITaskDescrWnd::Init(CUIXml* doc, LPCSTR start_from)
{
    CUIXmlInit xml_init;

    xml_init.InitWindow(*doc, start_from, 0, this);

    string512 str;

    m_UIMainFrame = xr_new<CUIFrameWindow>();
    m_UIMainFrame->SetAutoDelete(true);
    AttachChild(m_UIMainFrame);

    strconcat(sizeof(str), str, start_from, ":main_frame");
    xml_init.InitFrameWindow(*doc, str, 0, m_UIMainFrame);

    //.	strconcat				(str,start_from,":main_frame");
    //.	xml_init.InitAutoStaticGroup(*doc, str, m_UIMainFrame);

    m_UIMainHeader = xr_new<CUIFrameLineWnd>();
    m_UIMainHeader->SetAutoDelete(true);
    m_UIMainFrame->AttachChild(m_UIMainHeader);
    strconcat(sizeof(str), str, start_from, ":main_frame:header_frame_line");
    xml_init.InitFrameLine(*doc, str, 0, m_UIMainHeader);

    m_UITaskInfoWnd = xr_new<CUIScrollView>();
    m_UITaskInfoWnd->SetAutoDelete(true);
    m_UIMainFrame->AttachChild(m_UITaskInfoWnd);
    strconcat(sizeof(str), str, start_from, ":main_frame:scroll_view");
    xml_init.InitScrollView(*doc, str, 0, m_UITaskInfoWnd);
    if (doc->ReadAttribInt(start_from, 0, "op22_summary", 0))
    {
        strconcat(sizeof(str), str, start_from, ":summary_title");
        xml_init.InitFont(*doc, str, 0, m_op22_title_color, m_op22_title_font);
        strconcat(sizeof(str), str, start_from, ":summary_text");
        xml_init.InitFont(*doc, str, 0, m_op22_text_color, m_op22_text_font);
    }
}

void CUITaskDescrWnd::AddTaskSummary(CGameTask* task, int objective)
{
    if (!m_op22_title_font || !m_op22_text_font) return;
    const auto append = [&](LPCSTR text, CGameFont* font, u32 color)
    {
        auto* line = xr_new<CUIStatic>();
        line->SetAutoDelete(true);
        line->SetFont(font);
        line->SetTextColor(color);
        line->SetTextComplexMode(true);
        line->SetWidth(m_UITaskInfoWnd->GetDesiredChildWidth());
        line->SetText(text);
        line->AdjustHeightToText();
        line->SetHeight(line->GetHeight() + 8.f);
        m_UITaskInfoWnd->AddWindow(line, true);
    };
    append(*CStringTable().translate(task->m_Title), m_op22_title_font, m_op22_title_color);
    const int first = objective > 0 ? objective : (task->m_Objectives.size() > 1 ? 1 : 0);
    const int end = objective > 0 ? objective + 1 : int(task->m_Objectives.size());
    for (int i = first; i < end; ++i)
    {
        auto& item = task->m_Objectives[i];
        const char* mark = item.TaskState() == eTaskStateCompleted ? "[+] " : item.TaskState() == eTaskStateFail ? "[-] " : "[ ] ";
        const xr_string text = xr_string(mark) + *CStringTable().translate(item.description);
        append(text.c_str(), m_op22_text_font, m_op22_text_color);
    }
}

void CUITaskDescrWnd::Draw() { inherited::Draw(); }

void CUITaskDescrWnd::ClearAll() { m_UITaskInfoWnd->Clear(); }

void CUITaskDescrWnd::AddArticle(LPCSTR article)
{
    CUIEncyclopediaArticleWnd* article_info = xr_new<CUIEncyclopediaArticleWnd>();
    article_info->Init("encyclopedia_item.xml", "events_wnd:objective_item");
    article_info->SetArticle(article);
    m_UITaskInfoWnd->AddWindow(article_info, true);
}

void CUITaskDescrWnd::AddArticle(CEncyclopediaArticle* article)
{
    CUIEncyclopediaArticleWnd* article_info = xr_new<CUIEncyclopediaArticleWnd>();
    article_info->Init("encyclopedia_item.xml", "events_wnd:objective_item");
    article_info->SetArticle(article);
    m_UITaskInfoWnd->AddWindow(article_info, true);
}

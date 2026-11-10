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

CUITaskDescrWnd::~CUITaskDescrWnd() { xr_delete(m_op22_xml); }

void CUITaskDescrWnd::Init(CUIXml* doc, LPCSTR start_from)
{
    CUIXmlInit xml_init;
    m_op22_cards = !!doc->ReadAttribInt(start_from, 0, "op22_cards", 0);
    if (m_op22_cards)
    {
        m_op22_xml = xr_new<CUIXml>();
        R_ASSERT(m_op22_xml->Init(CONFIG_PATH, UI_PATH, "pda_events.xml"));
    }
    m_op22_path = start_from;

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
    if (m_op22_cards)
    {
        const float kx = 1024.f / 1280.f, ky = 768.f / 720.f;
        CUIXmlInit init;
        const float width = m_UITaskInfoWnd->GetDesiredChildWidth() - 8.f * kx;
        // OP22: размеры карточки из разметки (плотный интерфейс 10.11), по умолчанию - прежние
        const auto attr = [&](LPCSTR key, LPCSTR name, float def)
        {
            const xr_string path = m_op22_path + ":" + key;
            return m_op22_xml->ReadAttribFlt(path.c_str(), 0, name, def);
        };
        const float obj_row = attr("card_objective", "op22_row", 38.f), obj_pad = attr("card_objective", "op22_pad", 16.f);
        const float obj_x = attr("card_objective", "op22_text_x", 32.f), obj_y = attr("card_objective", "op22_text_y", 8.f);
        const float chk = attr("card_objective", "op22_check", 22.f), chk_y = attr("card_objective", "op22_check_y", 8.f);
        const auto append = [&](LPCSTR key, LPCSTR text, float minimum, float padding)
        {
            auto* line = xr_new<CUIStatic>();
            line->SetAutoDelete(true);
            const xr_string path = m_op22_path + ":" + key;
            init.InitStatic(*m_op22_xml, path.c_str(), 0, line);
            line->SetWidth(width);
            line->SetText(text);
            line->AdjustHeightToText();
            line->SetHeight(_max(minimum * ky, line->GetHeight() + padding * ky));
            m_UITaskInfoWnd->AddWindow(line, true);
        };
        append("card_title", *CStringTable().translate(task->m_Title), attr("card_title", "op22_min", 36.f), attr("card_title", "op22_pad", 8.f));
        if (task->Objective(0).description.size())
            append("card_description", *CStringTable().translate(task->Objective(0).description), attr("card_description", "op22_min", 29.f), attr("card_description", "op22_pad", 8.f));
        for (size_t i = 1; i < task->m_Objectives.size(); ++i)
        {
            auto& objective = task->m_Objectives[i];
            auto* row = xr_new<CUIWindow>();
            row->SetAutoDelete(true);
            row->Init(0.f, 0.f, width, obj_row * ky);
            auto* label = xr_new<CUIStatic>();
            label->SetAutoDelete(true);
            row->AttachChild(label);
            const xr_string path = m_op22_path + ":card_objective";
            init.InitStatic(*m_op22_xml, path.c_str(), 0, label);
            label->SetWndPos(obj_x * kx, obj_y * ky);
            label->SetWidth(width - obj_x * kx);
            label->SetText(*CStringTable().translate(objective.description));
            const auto state = objective.TaskState();
            if (state == eTaskStateCompleted || state == eTaskStateSkiped) label->SetTextColor(color_rgba(183,154,99,255));
            else if (state == eTaskStateFail) label->SetTextColor(color_rgba(255,90,60,255));
            label->AdjustHeightToText();
            row->SetHeight(_max(row->GetHeight(), label->GetHeight() + obj_pad * ky));
            auto* check = xr_new<CUIStatic>();
            check->SetAutoDelete(true);
            row->AttachChild(check);
            check->Init(0.f, chk_y * ky, chk * kx, chk * ky);
            check->InitTexture(state == eTaskStateCompleted ? "ui\\ct_tr_tasks_check_done" :
                state == eTaskStateFail ? "ui\\ct_tr_tasks_check_failed" :
                state == eTaskStateSkiped ? "ui\\ct_tr_tasks_check_skipped" : "ui\\ct_tr_tasks_check_open");
            check->SetStretchTexture(true);
            check->SetWndSize(Fvector2().set(chk * kx, chk * ky));
            auto* rule = xr_new<CUIStatic>();
            rule->SetAutoDelete(true);
            rule->Init(0.f, 0.f, width, ky);
            rule->InitTexture("ui\\ct_tr_tasks_line");
            rule->SetStretchTexture(true);
            row->AttachChild(rule);
            m_UITaskInfoWnd->AddWindow(row, true);
        }
        auto* rule = xr_new<CUIStatic>();
        rule->Init(0.f,0.f,width,ky);
        rule->InitTexture("ui\\ct_tr_tasks_line");
        rule->SetStretchTexture(true);
        m_UITaskInfoWnd->AddWindow(rule, true);
        m_UITaskInfoWnd->ScrollToBegin();
        return;
    }
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

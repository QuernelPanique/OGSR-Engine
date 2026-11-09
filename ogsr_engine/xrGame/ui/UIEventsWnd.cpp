#include "stdafx.h"
#include "UIEventsWnd.h"
#include "UIFrameWindow.h"
#include "UIFrameLineWnd.h"
#include "UIAnimatedStatic.h"
#include "UIMapWnd.h"
#include "UIScrollView.h"
#include "UITabControl.h"
#include "UITaskDescrWnd.h"
#include "UI3tButton.h"
#include "../HUDManager.h"
#include "../level.h"
#include "../actor.h"
#include "../gametaskmanager.h"
#include "../gametask.h"
#include "../map_manager.h"
#include "../map_location.h"
#include "../string_table.h"
#include "UITaskItem.h"
#include "../alife_registry_wrappers.h"
#include "../encyclopedia_article.h"
#include "../ui.h"
#include "../UIGameSP.h"
#include "UIPdaWnd.h"

CUIEventsWnd::CUIEventsWnd() { m_flags.zero(); }

CUIEventsWnd::~CUIEventsWnd()
{
    delete_data(m_UIMapWnd);
    delete_data(m_UITaskInfoWnd);
}

void CUIEventsWnd::Init()
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, "pda_events.xml");
    R_ASSERT3(xml_result, "xml file not found", "pda_events.xml");

    CUIXmlInit xml_init;
    xml_init.InitWindow(uiXml, "main_wnd", 0, this);
    m_op22_split = !!uiXml.ReadAttribInt("main_wnd", 0, "op22_split", 0);
    m_op22_cards = !!uiXml.ReadAttribInt("main_wnd", 0, "op22_cards", 0);

    m_UILeftFrame = xr_new<CUIFrameWindow>();
    m_UILeftFrame->SetAutoDelete(true);
    AttachChild(m_UILeftFrame);
    xml_init.InitFrameWindow(uiXml, "main_wnd:left_frame", 0, m_UILeftFrame);

    m_UILeftHeader = xr_new<CUIFrameLineWnd>();
    m_UILeftHeader->SetAutoDelete(true);
    m_UILeftFrame->AttachChild(m_UILeftHeader);
    xml_init.InitFrameLine(uiXml, "main_wnd:left_frame:left_frame_header", 0, m_UILeftHeader);

    if (uiXml.NavigateToNode("main_wnd:left_frame:left_frame_header:anim_static"))
    {
        m_UIAnimation = xr_new<CUIAnimatedStatic>();
        m_UIAnimation->SetAutoDelete(true);
        xml_init.InitAnimatedStatic(uiXml, "main_wnd:left_frame:left_frame_header:anim_static", 0, m_UIAnimation);
        m_UILeftHeader->AttachChild(m_UIAnimation);
    }

    m_UIRightWnd = xr_new<CUIWindow>();
    m_UIRightWnd->SetAutoDelete(true);
    AttachChild(m_UIRightWnd);
    xml_init.InitWindow(uiXml, "main_wnd:right_frame", 0, m_UIRightWnd);

    m_UIMapWnd = xr_new<CUIMapWnd>();
    m_UIMapWnd->SetAutoDelete(false);
    m_UIMapWnd->Init("pda_events.xml", "main_wnd:right_frame:map_wnd");

    m_UITaskInfoWnd = xr_new<CUITaskDescrWnd>();
    m_UITaskInfoWnd->SetAutoDelete(false);
    m_UITaskInfoWnd->Init(&uiXml, "main_wnd:right_frame:task_descr_view");

    m_ListWnd = xr_new<CUIScrollView>();
    m_ListWnd->SetAutoDelete(true);
    m_UILeftFrame->AttachChild(m_ListWnd);
    xml_init.InitScrollView(uiXml, "main_wnd:left_frame:list", 0, m_ListWnd);

    m_TaskFilter = xr_new<CUITabControl>();
    m_TaskFilter->SetAutoDelete(true);
    m_UILeftFrame->AttachChild(m_TaskFilter);
    xml_init.InitTabControl(uiXml, "main_wnd:left_frame:filter_tab", 0, m_TaskFilter);
    m_TaskFilter->SetWindowName("filter_tab");
    Register(m_TaskFilter);
    AddCallback("filter_tab", TAB_CHANGED, fastdelegate::MakeDelegate(this, &CUIEventsWnd::OnFilterChanged));

    if (m_op22_cards)
    {
        m_op22_map_button = xr_new<CUI3tButton>();
        m_op22_map_button->SetAutoDelete(true);
        m_UIRightWnd->AttachChild(m_op22_map_button);
        xml_init.Init3tButton(uiXml, "main_wnd:right_frame:op22_map_button", 0, m_op22_map_button);
        m_op22_map_button->SetWindowName("task_map");
        Register(m_op22_map_button);
        AddCallback("task_map", BUTTON_CLICKED, fastdelegate::MakeDelegate(this, &CUIEventsWnd::OnTaskMap));
        m_op22_map_button->Enable(false);
    }
    m_currFilter = eActiveTask;
    SetDescriptionMode(true);

    m_ui_task_item_xml.Init(CONFIG_PATH, UI_PATH, "job_item.xml");
}

void CUIEventsWnd::Update()
{
    if (m_flags.test(flNeedReload))
    {
        ReloadList(false);
        m_flags.set(flNeedReload, FALSE);
    }
    inherited::Update();
}

void CUIEventsWnd::Draw() { inherited::Draw(); }

void CUIEventsWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData) { CUIWndCallback::OnEvent(pWnd, msg, pData); }

void CUIEventsWnd::OnFilterChanged(CUIWindow* w, void*)
{
    const int index = m_TaskFilter->GetActiveIndex();
    m_currFilter = m_op22_cards && index == 2 ? eMaxTask : (ETaskFilters)index;
    ReloadList(false);
    if (!GetDescriptionMode())
        SetDescriptionMode(true);
}

void CUIEventsWnd::Reload() { m_flags.set(flNeedReload, TRUE); }

void CUIEventsWnd::ReloadList(bool bClearOnly)
{
    const int scroll = m_ListWnd->GetCurrentScrollPos();
    m_ListWnd->Clear();
    if (m_op22_map_button) m_op22_map_button->Enable(false);
    m_op22_map_level = "";
    // Selection belongs to the rebuilt list; its former description is stale.
    if (m_op22_split) m_UITaskInfoWnd->ClearAll();
    if (bClearOnly)
        return;
    if (!g_actor)
        return;

    GameTasks& tasks = Actor()->GameTaskManager().GameTasks();
    xr_vector<CGameTask*> game_tasks;
    for (const auto& it : tasks)
    {
        CGameTask* task = it.game_task;
        R_ASSERT(task);
        R_ASSERT(task->m_Objectives.size() > 0);
        if (Filter(task))
            game_tasks.push_back(task);
    }

    if (m_currFilter == eActiveTask)
        std::sort(game_tasks.begin(), game_tasks.end(), [](const auto& a, const auto& b) {
            if (a->m_priority == b->m_priority)
                return a->m_ReceiveTime > b->m_ReceiveTime;
            return a->m_priority < b->m_priority;
        });

    if (m_op22_cards)
    {
        if (m_currFilter != eActiveTask)
            std::stable_sort(game_tasks.begin(), game_tasks.end(), [](const auto* a, const auto* b) { return a->m_ReceiveTime > b->m_ReceiveTime; });
        CGameTask* selected = nullptr;
        for (auto* task : game_tasks)
        {
            auto* item = xr_new<CUITaskRootItem>(this);
            item->SetGameTask(task, 0);
            m_ListWnd->AddWindow(item, true);
            if (task->m_ID == m_op22_selected) selected = task;
        }
        if (!selected && !game_tasks.empty())
        {
            selected = game_tasks.front();
            for (auto* task : game_tasks)
                if (task->m_ID == g_active_task_id) { selected = task; break; }
        }
        if (selected) ShowDescription(selected, 0);
        else { m_op22_selected = ""; m_UIMapWnd->Show(false); }
        m_ListWnd->ScrollToBegin();
        m_ListWnd->SetScrollPos(scroll);
        return;
    }
    for (const auto& task : game_tasks)
    {
        CUITaskItem* pTaskItem = NULL;
        /*
                if(task->m_Objectives[0].TaskState()==eTaskUserDefined)
                {
                    VERIFY				(task->m_Objectives.size()==1);
                    pTaskItem			= xr_new<CUIUserTaskItem>(this);
                    pTaskItem->SetGameTask			(task, 0);
                    m_ListWnd->AddWindow			(pTaskItem,true);
                }else
        */
        u32 visible_objectives = 0;
        if (task->m_show_all_objectives || task->Objective(0).TaskState() != eTaskStateInProgress || task->m_Objectives.size() <= 2)
        {
            visible_objectives = task->m_Objectives.size();
        }
        else
        {
            for (u32 i = 0; i < task->m_Objectives.size(); i++)
            {
                auto& it = task->m_Objectives.at(i);
                if (it.TaskState() != eTaskStateInProgress)
                    visible_objectives = i + 1;
            }
            if (visible_objectives < 2)
                visible_objectives = 2;
            else if (visible_objectives < task->m_Objectives.size())
                visible_objectives++;
        }

        for (u32 i = 0; i < visible_objectives; ++i)
        {
            if (i == 0)
            {
                pTaskItem = xr_new<CUITaskRootItem>(this);
            }
            else
            {
                pTaskItem = xr_new<CUITaskSubItem>(this);
            }
            pTaskItem->SetGameTask(task, (u16)i);
            m_ListWnd->AddWindow(pTaskItem, true);
        }
    }
}

void CUIEventsWnd::Show(bool status)
{
    inherited::Show(status);

    // хз почему, но если сделать Show для m_UIMapWnd, который не приаттачен к родителю - на нём пропадают LevelMap, причину не нашел, будет пока костыль

    if (!GetDescriptionMode())
        m_UIRightWnd->AttachChild(m_UIMapWnd);

    m_UIMapWnd->Show(status);
    m_UITaskInfoWnd->Show(status);

    if (!GetDescriptionMode())
        m_UIRightWnd->DetachChild(m_UIMapWnd);

    ReloadList(status == false);
}

bool CUIEventsWnd::Filter(CGameTask* t)
{
    ETaskState task_state = t->m_Objectives[0].TaskState();
    if (m_op22_cards && m_currFilter == eMaxTask) return true;
    //	bool bprimary_only			= m_primary_or_all_filter_btn->GetCheck();

    return (false /*m_currFilter==eOwnTask && task_state==eTaskUserDefined*/) ||
        ((true /*!bprimary_only || (bprimary_only && t->m_is_task_general)*/) &&
         ((m_currFilter == eAccomplishedTask && task_state == eTaskStateCompleted) || (m_currFilter == eFailedTask && task_state == eTaskStateFail) ||
          (m_currFilter == eActiveTask && task_state == eTaskStateInProgress) || (m_currFilter == eSkipedTask && task_state == eTaskStateSkiped)));
}

void CUIEventsWnd::SetDescriptionMode(bool bMap)
{
    if (m_op22_split)
    {
        if (!m_UIRightWnd->IsChild(m_UIMapWnd)) m_UIRightWnd->AttachChild(m_UIMapWnd);
        if (!m_UIRightWnd->IsChild(m_UITaskInfoWnd)) m_UIRightWnd->AttachChild(m_UITaskInfoWnd);
        m_flags.set(flMapMode, true);
        return;
    }
    if (bMap)
    {
        m_UIRightWnd->DetachChild(m_UITaskInfoWnd);
        m_UIRightWnd->AttachChild(m_UIMapWnd);
    }
    else
    {
        m_UIRightWnd->DetachChild(m_UIMapWnd);
        m_UIRightWnd->AttachChild(m_UITaskInfoWnd);
    }
    m_flags.set(flMapMode, bMap);
}

bool CUIEventsWnd::GetDescriptionMode() { return !!m_flags.test(flMapMode); }

void CUIEventsWnd::ShowDescription(CGameTask* t, int idx)
{
    if (m_op22_cards)
    {
        m_op22_selected = t->m_ID;
        CMapLocation* target = nullptr;
        const int count = int(t->m_Objectives.size());
        for (int i = 1; i < count; ++i)
        {
            auto& objective = t->m_Objectives[i];
            auto* location = objective.LinkedMapLocation();
            if (!location || !location->SpotEnabled()) continue;
            if (!target) target = location;
            if (objective.TaskState() == eTaskStateInProgress) { target = location; break; }
        }
        if (!target) target = t->Objective(0).LinkedMapLocation();
        m_op22_map_level = "";
        if (target && target->SpotEnabled())
        {
            m_op22_map_level = target->LevelName();
            m_op22_map_position = target->Position();
            m_UIMapWnd->Show(true);
            m_UIMapWnd->SetTargetMap(m_op22_map_level, m_op22_map_position, true);
        }
        else m_UIMapWnd->Show(false);
        m_op22_map_button->Enable(m_op22_map_level.size() != 0);
        for (u32 i = 0; i < m_ListWnd->GetSize(); ++i)
        {
            auto* item = static_cast<CUITaskItem*>(m_ListWnd->GetItem(i));
            item->MarkSelected(item->GameTask()->m_ID == m_op22_selected);
        }
        m_UITaskInfoWnd->ClearAll();
        m_UITaskInfoWnd->AddTaskSummary(t, 0);
        // Keep real quest articles accessible below the compact objective list.
        if (auto* articles = Actor()->encyclopedia_registry->registry().objects_ptr())
            for (const auto& entry : *articles)
            {
                if (entry.article_type != ARTICLE_DATA::eTaskArticle) continue;
                CEncyclopediaArticle article;
                article.Load(entry.article_id);
                if (article.data()->group == t->m_ID || (t->Objective(0).article_id.size() && entry.article_id == t->Objective(0).article_id))
                    m_UITaskInfoWnd->AddArticle(&article);
            }
        return;
    }
    if (GetDescriptionMode())
    { // map
        SGameTaskObjective& o = t->Objective(idx);
        CMapLocation* ml = o.LinkedMapLocation();

        if (ml && ml->SpotEnabled())
            m_UIMapWnd->SetTargetMap(ml->LevelName(), ml->Position(), true);

        int sz = m_ListWnd->GetSize();

        for (int i = 0; i < sz; ++i)
        {
            CUITaskItem* itm = (CUITaskItem*)m_ListWnd->GetItem(i);

            if ((itm->GameTask() == t) && (itm->ObjectiveIdx() == idx))
                itm->MarkSelected(true);
            else
                itm->MarkSelected(false);
        }
    }
    if (!GetDescriptionMode() || m_op22_split)
    { // articles
        SGameTaskObjective& o = t->Objective(0);

        m_UITaskInfoWnd->ClearAll();
        if (m_op22_split) m_UITaskInfoWnd->AddTaskSummary(t, idx);

        if (Actor()->encyclopedia_registry->registry().objects_ptr())
        {
            ARTICLE_VECTOR::const_iterator it = Actor()->encyclopedia_registry->registry().objects_ptr()->begin();

            for (; it != Actor()->encyclopedia_registry->registry().objects_ptr()->end(); ++it)
            {
                if (ARTICLE_DATA::eTaskArticle == it->article_type)
                {
                    CEncyclopediaArticle A;
                    A.Load(it->article_id);

                    if (t->m_ID == A.data()->group)
                    {
                        m_UITaskInfoWnd->AddArticle(&A);
                    }
                    else if (o.article_id.size() && it->article_id == o.article_id)
                    {
                        m_UITaskInfoWnd->AddArticle(&A);
                    }
                }
            }
        }
    }
}

void CUIEventsWnd::OnTaskMap(CUIWindow*, void*)
{
    if (!m_op22_map_level.size()) return;
    // Changing the tab clears the task view: copy the destination first.
    const shared_str level = m_op22_map_level;
    const Fvector2 position = m_op22_map_position;
    auto* game = smart_cast<CUIGameSP*>(HUD().GetUI()->UIGame());
    if (game && game->PdaMenu)
    {
        game->PdaMenu->SetActiveSubdialog(eptMap);
        game->PdaMenu->UIMapWnd->SetTargetMap(level, position, true);
    }
}

bool CUIEventsWnd::ItemHasDescription(CUITaskItem* itm)
{
    if (itm->ObjectiveIdx() == 0) // root
    {
        bool bHasLocation = itm->GameTask()->HasLinkedMapLocations();
        return bHasLocation;
    }
    else
    {
        SGameTaskObjective* obj = itm->Objective();
        CMapLocation* ml = obj->LinkedMapLocation();
        bool bHasLocation = (NULL != ml);
        bool bIsMapMode = GetDescriptionMode();
        bool b = (bIsMapMode && bHasLocation && ml->SpotEnabled());
        return b;
    }
}
void CUIEventsWnd::Reset()
{
    inherited::Reset();
    Reload();
}

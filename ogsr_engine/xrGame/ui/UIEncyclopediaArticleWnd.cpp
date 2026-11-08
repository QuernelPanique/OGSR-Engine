#include "stdafx.h"
#include "UIEncyclopediaArticleWnd.h"
#include "UIStatic.h"
#include "../encyclopedia_article.h"
#include "UIXmlInit.h"
#include "../string_table.h"
#include "../UI.h"

CUIEncyclopediaArticleWnd::CUIEncyclopediaArticleWnd() : m_Article(NULL) {}

CUIEncyclopediaArticleWnd::~CUIEncyclopediaArticleWnd() {}

void CUIEncyclopediaArticleWnd::Init(LPCSTR xml_name, LPCSTR start_from)
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, xml_name);
    R_ASSERT3(xml_result, "xml file not found", xml_name);

    CUIXmlInit xml_init;

    string512 str;

    strcpy_s(str, sizeof(str), start_from);
    xml_init.InitWindow(uiXml, str, 0, this);
    m_op22_fit = !!uiXml.ReadAttribInt(start_from, 0, "op22_fit", 0);
    m_op22_image_limit = uiXml.ReadAttribFlt(start_from, 0, "op22_max_image_height", 240.f);

    strconcat(sizeof(str), str, start_from, ":image");
    m_UIImage = xr_new<CUIStatic>();
    m_UIImage->SetAutoDelete(true);
    xml_init.InitStatic(uiXml, str, 0, m_UIImage);
    AttachChild(m_UIImage);

    strconcat(sizeof(str), str, start_from, ":text_cont");
    m_UIText = xr_new<CUIStatic>();
    m_UIText->SetAutoDelete(true);
    xml_init.InitStatic(uiXml, str, 0, m_UIText);
    AttachChild(m_UIText);
}

void CUIEncyclopediaArticleWnd::SetArticle(CEncyclopediaArticle* article)
{
    if (article->data()->image.TextureAvailable())
    {
        m_UIImage->SetShader(article->data()->image.GetShader());
        m_UIImage->SetOriginalRect(article->data()->image.GetStaticItem()->GetOriginalRect());
        m_UIImage->SetWndSize(article->data()->image.GetWndSize());

        float img_x = (GetWidth() - m_UIImage->GetWidth()) / 2.0f;
        img_x = _max(0.0f, img_x);
        m_UIImage->SetWndPos(img_x, m_UIImage->GetWndPos().y);
        if (m_op22_fit)
        {
            const Frect source = m_UIImage->GetUIStaticItem().GetOriginalRect();
            const float width = _max(1.f, source.width() * UI()->get_current_kx());
            const float height = _max(1.f, source.height());
            const float scale = _min(GetWidth() / width, m_op22_image_limit / height);
            m_UIImage->SetStretchTexture(true);
            m_UIImage->SetWndRect((GetWidth() - width * scale) / 2.f, 0.f, width * scale, height * scale);
            m_UIImage->Show(true);
        }
    };
    if (m_op22_fit && !article->data()->image.TextureAvailable())
    {
        m_UIImage->Show(false);
        m_UIImage->SetHeight(0.f);
    }
    m_UIText->SetText(*CStringTable().translate(article->data()->text.c_str()));
    m_UIText->AdjustHeightToText();

    AdjustLauout();
}

void CUIEncyclopediaArticleWnd::AdjustLauout()
{
    const float top = m_UIImage->GetWndPos().y + m_UIImage->GetHeight() + (m_op22_fit && m_UIImage->IsShown() ? 12.f : 0.f);
    m_UIText->SetWndPos(m_UIText->GetWndPos().x, top);
    SetHeight(top + m_UIText->GetHeight());
}

void CUIEncyclopediaArticleWnd::SetArticle(LPCSTR article)
{
    CEncyclopediaArticle A;
    A.Load(article);
    SetArticle(&A);
}

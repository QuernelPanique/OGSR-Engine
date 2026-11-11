// UICharacterInfo.h:  окошко, с информацией о персонаже
//
//////////////////////////////////////////////////////////////////////
#pragma once

#include "uiwindow.h"
#include "../alife_space.h"
#include "../character_info_defs.h"

class CUIStatic;
class CCharacterInfo;
class CUIXml;
class CUIScrollView;
class CUICharacterInfo : public CUIWindow
{
private:
    typedef CUIWindow inherited;

protected:
    void SetRelation(ALife::ERelationType relation, CHARACTER_GOODWILL goodwill);
    void ResetAllStrings();
    void UpdateRelation();
    bool hasOwner() const { return (m_ownerID != u16(-1)); }
    // Biography
    CUIScrollView* pUIBio;
    bool m_bForceUpdate;
    bool m_op22_rel_over_rep{}; // OP22: у собеседника отношение на месте репутации (карточка диалога по макету)
    xr_vector<CUIStatic*> m_op22_decor; // OP22: op22_auto_static (рамка портрета и т.п.)
    u16 m_ownerID;

    enum
    {
        eUIIcon = 0,
        eUIName,
        eUIRank,
        eUIRankCaption,
        eUICommunity,
        eUICommunityCaption,
        eUIReputation,
        eUIReputationCaption,
        eUIRelation,
        eUIRelationCaption,
        eMaxCaption,
    };
    CUIStatic* m_icons[eMaxCaption];
    u32 m_op22_normal[eMaxCaption]{};
    bool m_op22_has_normal{};
    bool m_op22_selected{};
    bool m_op22_numeric_relation{};
    bool m_op22_numeric_reputation{};
    bool m_op22_combine_rank{};

    enum
    {
        eUIRankCaptionText = 0,
        eUICommunityCaptionText,
        eUIReputationCaptionText,
        eUIRelationCaptionText,
        eMaxCaptionText,
    };
    shared_str m_text_ids[eMaxCaptionText];

    shared_str m_texture_name;

public:
    CUICharacterInfo();
    virtual ~CUICharacterInfo();

    void Init(float x, float y, float width, float height, CUIXml* xml_doc);
    void Init(float x, float y, float width, float height, const char* xml_name);
    void InitCharacter(u16 id);
    void ClearInfo();
    void op22_SetSelected(bool selected);
    void op22_NameOnly(const char* name); // OP22: карточка без портрета и его оформления, только имя (тайник)
    void op22_ShowPortrait(); // OP22: вернуть портрет и оформление

    virtual void Update();

    u16 OwnerID() const { return m_ownerID; }
    CUIStatic& UIIcon()
    {
        VERIFY(m_icons[eUIIcon]);
        return *m_icons[eUIIcon];
    }
    CUIStatic& UIName()
    {
        VERIFY(m_icons[eUIName]);
        return *m_icons[eUIName];
    }
    const shared_str& IconName() { return m_texture_name; }
};
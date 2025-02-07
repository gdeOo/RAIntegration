#include "RA_Achievement.h"

#include "RA_MemManager.h"
#include "RA_md5factory.h"

#include "RA_Defs.h"

#include "services\AchievementRuntime.hh"
#include "services\ServiceLocator.hh"

#include "ui\viewmodels\MessageBoxViewModel.hh"

#ifndef RA_UTEST
#include "RA_ImageFactory.h"
#endif

#ifndef RA_UTEST
#include "RA_Core.h"
#else
// duplicate of code in RA_Core, but RA_Core needs to be cleaned up before it can be pulled into the unit test build
GSL_SUPPRESS_F23
void _ReadStringTil(std::string& value, char nChar, const char* restrict& pSource)
{
    Expects(pSource != nullptr);
    const char* pStartString = pSource;

    while (*pSource != '\0' && *pSource != nChar)
        pSource++;

    value.assign(pStartString, pSource - pStartString);
    pSource++;
}
#endif

//////////////////////////////////////////////////////////////////////////

Achievement::Achievement() noexcept
{
    m_vConditions.AddGroup();
}

Achievement::~Achievement() noexcept
{
    if (m_bActive)
    {
        if (ra::services::ServiceLocator::Exists<ra::services::AchievementRuntime>())
        {
            auto& pRuntime = ra::services::ServiceLocator::GetMutable<ra::services::AchievementRuntime>();
            pRuntime.DeactivateAchievement(ID());
        }
    }
}

static void MergeHits(rc_condset_t& pGroup, rc_condset_t& pOldGroup) noexcept
{
    // for each condition in pGroup, attempt to find the matching condition in pOldGroup
    // if found, merge the hits, delta, and prior values into the new condition to maintain state
    auto* pFirstOldCondition = pOldGroup.conditions;
    for (auto* pCondition = pGroup.conditions; pCondition; pCondition = pCondition->next)
    {
        for (auto* pOldCondition = pFirstOldCondition; pOldCondition; pOldCondition = pOldCondition->next)
        {
            if (pOldCondition->operand1.type != pCondition->operand1.type ||
                pOldCondition->operand2.type != pCondition->operand2.type ||
                pOldCondition->oper != pCondition->oper ||
                pOldCondition->type != pCondition->type ||
                pOldCondition->required_hits != pCondition->required_hits)
            {
                continue;
            }

            switch (pOldCondition->operand1.type)
            {
                case RC_OPERAND_ADDRESS:
                case RC_OPERAND_DELTA:
                case RC_OPERAND_PRIOR:
                    if (pOldCondition->operand1.value.memref->memref.address != pCondition->operand1.value.memref->memref.address ||
                        pOldCondition->operand1.value.memref->memref.size != pCondition->operand1.value.memref->memref.size)
                    {
                        continue;
                    }

                    pCondition->operand1.value.memref->previous = pOldCondition->operand1.value.memref->previous;
                    pCondition->operand1.value.memref->prior = pOldCondition->operand1.value.memref->prior;
                    break;

                default:
                case RC_OPERAND_CONST:
                    if (pOldCondition->operand1.value.num != pCondition->operand1.value.num)
                        continue;

                    break;

                case RC_OPERAND_FP:
                    if (pOldCondition->operand1.value.dbl != pCondition->operand1.value.dbl)
                        continue;

                    break;

                case RC_OPERAND_LUA:
                    if (pOldCondition->operand1.value.luafunc != pCondition->operand1.value.luafunc)
                        continue;

                    break;
            }

            pCondition->current_hits = pOldCondition->current_hits;

            // if we matched the first condition, ignore it on future scans
            if (pOldCondition == pFirstOldCondition)
                pFirstOldCondition = pOldCondition->next;

            break;
        }
    }
}

void Achievement::RebuildTrigger()
{
    // store the original trigger for later
    std::shared_ptr<std::vector<unsigned char>> pOldTriggerBuffer = m_pTriggerBuffer;
    auto* pOldTrigger = static_cast<rc_trigger_t*>(m_pTrigger);

    // convert the UI definition into a string
    std::string sTrigger;
    m_vConditions.Serialize(sTrigger);

    // parse the string into a trigger and rebuild the UI elements
    ParseTrigger(sTrigger.c_str());

    // attempt to copy over the hit counts
    if (pOldTrigger != nullptr && m_pTrigger != nullptr)
    {
        auto* pTrigger = static_cast<rc_trigger_t*>(m_pTrigger);
        if (pTrigger->requirement && pOldTrigger->requirement)
            MergeHits(*pTrigger->requirement, *pOldTrigger->requirement);

        auto* pAltGroup = pTrigger->alternative;
        auto* pOldAltGroup = pOldTrigger->alternative;
        while (pAltGroup && pOldAltGroup)
        {
            MergeHits(*pAltGroup, *pOldAltGroup);
            pAltGroup = pAltGroup->next;
            pOldAltGroup = pOldAltGroup->next;
        }
    }

    // mark the conditions as dirty so they will get repainted
    SetDirtyFlag(DirtyFlags::Conditions);

    if (m_bActive)
    {
        // disassociate the old trigger and register the new one
        SetActive(false);
        SetActive(true);
    }
}

static constexpr MemSize GetCompVariableSize(char nOperandSize) noexcept
{
    switch (nOperandSize)
    {
        case RC_MEMSIZE_BIT_0:
            return MemSize::Bit_0;
        case RC_MEMSIZE_BIT_1:
            return MemSize::Bit_1;
        case RC_MEMSIZE_BIT_2:
            return MemSize::Bit_2;
        case RC_MEMSIZE_BIT_3:
            return MemSize::Bit_3;
        case RC_MEMSIZE_BIT_4:
            return MemSize::Bit_4;
        case RC_MEMSIZE_BIT_5:
            return MemSize::Bit_5;
        case RC_MEMSIZE_BIT_6:
            return MemSize::Bit_6;
        case RC_MEMSIZE_BIT_7:
            return MemSize::Bit_7;
        case RC_MEMSIZE_LOW:
            return MemSize::Nibble_Lower;
        case RC_MEMSIZE_HIGH:
            return MemSize::Nibble_Upper;
        case RC_MEMSIZE_8_BITS:
            return MemSize::EightBit;
        case RC_MEMSIZE_16_BITS:
            return MemSize::SixteenBit;
        case RC_MEMSIZE_32_BITS:
            return MemSize::ThirtyTwoBit;
        default:
            ASSERT(!"Unsupported operand size");
            return MemSize::EightBit;
    }
}

inline static constexpr void SetOperand(CompVariable& var, const rc_operand_t& operand) noexcept
{
    switch (operand.type)
    {
        case RC_OPERAND_ADDRESS:
            var.Set(GetCompVariableSize(operand.value.memref->memref.size), CompVariable::Type::Address, operand.value.memref->memref.address);
            break;

        case RC_OPERAND_DELTA:
            var.Set(GetCompVariableSize(operand.value.memref->memref.size), CompVariable::Type::DeltaMem, operand.value.memref->memref.address);
            break;

        case RC_OPERAND_PRIOR:
            var.Set(GetCompVariableSize(operand.value.memref->memref.size), CompVariable::Type::PriorMem, operand.value.memref->memref.address);
            break;

        case RC_OPERAND_CONST:
            var.Set(MemSize::ThirtyTwoBit, CompVariable::Type::ValueComparison, operand.value.num);
            break;

        case RC_OPERAND_FP:
            ASSERT(!"Floating point operand not supported");
            var.Set(MemSize::ThirtyTwoBit, CompVariable::Type::ValueComparison, 0U);
            break;

        case RC_OPERAND_LUA:
            ASSERT(!"Lua operand not supported");
            var.Set(MemSize::ThirtyTwoBit, CompVariable::Type::ValueComparison, 0U);
            break;
    }
}

static void MakeConditionGroup(ConditionSet& vConditions, rc_condset_t* pCondSet)
{
    vConditions.AddGroup();
    if (!pCondSet)
        return;

    ConditionGroup& group = vConditions.GetGroup(vConditions.GroupCount() - 1);

    rc_condition_t* pCondition = pCondSet->conditions;
    while (pCondition != nullptr)
    {
        Condition cond;
        SetOperand(cond.CompSource(), pCondition->operand1);
        SetOperand(cond.CompTarget(), pCondition->operand2);

        switch (pCondition->oper)
        {
            default:
                ASSERT(!"Unsupported operator");
                _FALLTHROUGH;
            case RC_CONDITION_EQ:
                cond.SetCompareType(ComparisonType::Equals);
                break;
            case RC_CONDITION_NE:
                cond.SetCompareType(ComparisonType::NotEqualTo);
                break;
            case RC_CONDITION_LT:
                cond.SetCompareType(ComparisonType::LessThan);
                break;
            case RC_CONDITION_LE:
                cond.SetCompareType(ComparisonType::LessThanOrEqual);
                break;
            case RC_CONDITION_GT:
                cond.SetCompareType(ComparisonType::GreaterThan);
                break;
            case RC_CONDITION_GE:
                cond.SetCompareType(ComparisonType::GreaterThanOrEqual);
                break;
        }

        switch (pCondition->type)
        {
            default:
                ASSERT(!"Unsupported condition type");
                _FALLTHROUGH;
            case RC_CONDITION_STANDARD:
                cond.SetConditionType(Condition::Type::Standard);
                break;
            case RC_CONDITION_RESET_IF:
                cond.SetConditionType(Condition::Type::ResetIf);
                break;
            case RC_CONDITION_PAUSE_IF:
                cond.SetConditionType(Condition::Type::PauseIf);
                break;
            case RC_CONDITION_ADD_SOURCE:
                cond.SetConditionType(Condition::Type::AddSource);
                break;
            case RC_CONDITION_SUB_SOURCE:
                cond.SetConditionType(Condition::Type::SubSource);
                break;
            case RC_CONDITION_ADD_HITS:
                cond.SetConditionType(Condition::Type::AddHits);
                break;
            case RC_CONDITION_AND_NEXT:
                cond.SetConditionType(Condition::Type::AndNext);
                break;
        }

        cond.SetRequiredHits(pCondition->required_hits);

        group.Add(cond);
        pCondition = pCondition->next;
    }
}

void Achievement::ParseTrigger(const char* sTrigger)
{
    m_vConditions.Clear();

    const int nSize = rc_trigger_size(sTrigger);
    if (nSize < 0)
    {
        // parse error occurred
        RA_LOG("rc_parse_trigger returned %d", nSize);
        m_pTrigger = nullptr;

        ra::ui::viewmodels::MessageBoxViewModel::ShowWarningMessage(
            ra::StringPrintf(L"Unable to activate achievement: %s", Title()),
            ra::StringPrintf(L"Parse error %d", nSize));
    }
    else
    {
        // allocate space and parse again
        m_pTriggerBuffer = std::make_shared<std::vector<unsigned char>>(nSize);
        auto* pTrigger = rc_parse_trigger(m_pTriggerBuffer->data(), sTrigger, nullptr, 0);
        m_pTrigger = pTrigger;

        if (m_bActive)
        {
            SetActive(false);
            SetActive(true);
        }

        // wrap rc_trigger_t in a ConditionSet for the UI
        MakeConditionGroup(m_vConditions, pTrigger->requirement);
        rc_condset_t* alternative = pTrigger->alternative;
        while (alternative != nullptr)
        {
            MakeConditionGroup(m_vConditions, alternative);
            alternative = alternative->next;
        }
    }
}

static constexpr bool HasHitCounts(const rc_condset_t* pCondSet) noexcept
{
    rc_condition_t* condition = pCondSet->conditions;
    while (condition != nullptr)
    {
        if (condition->current_hits)
            return true;

        condition = condition->next;
    }

    return false;
}

static constexpr bool HasHitCounts(const rc_trigger_t* pTrigger) noexcept
{
    if (HasHitCounts(pTrigger->requirement))
        return true;

    rc_condset_t* pAlternate = pTrigger->alternative;
    while (pAlternate != nullptr)
    {
        if (HasHitCounts(pAlternate))
            return true;

        pAlternate = pAlternate->next;
    }

    return false;
}

static constexpr rc_condition_t* GetTriggerCondition(rc_trigger_t* pTrigger, size_t nGroup, size_t nIndex) noexcept
{
    rc_condset_t* pGroup = pTrigger->requirement;
    if (nGroup > 0)
    {
        pGroup = pTrigger->alternative;
        --nGroup;

        while (pGroup && nGroup > 0)
        {
            pGroup = pGroup->next;
            --nGroup;
        }
    }

    if (!pGroup)
        return nullptr;

    rc_condition_t* pCondition = pGroup->conditions;
    while (pCondition && nIndex > 0)
    {
        --nIndex;
        pCondition = pCondition->next;
    }

    return pCondition;
}

unsigned int Achievement::GetConditionHitCount(size_t nGroup, size_t nIndex) const noexcept
{
    if (m_pTrigger == nullptr)
        return 0U;

    rc_trigger_t* pTrigger = static_cast<rc_trigger_t*>(m_pTrigger);
    const rc_condition_t* pCondition = GetTriggerCondition(pTrigger, nGroup, nIndex);
    return pCondition ? pCondition->current_hits : 0U;
}

void Achievement::SetConditionHitCount(size_t nGroup, size_t nIndex, unsigned int nCurrentHits) const noexcept
{
    if (m_pTrigger == nullptr)
        return;

    rc_trigger_t* pTrigger = static_cast<rc_trigger_t*>(m_pTrigger);
    rc_condition_t* pCondition = GetTriggerCondition(pTrigger, nGroup, nIndex);
    if (pCondition)
        pCondition->current_hits = nCurrentHits;
}

void Achievement::AddAltGroup() noexcept { m_vConditions.AddGroup(); }
void Achievement::RemoveAltGroup(gsl::index nIndex) { m_vConditions.RemoveAltGroup(nIndex); }

void Achievement::SetID(ra::AchievementID nID) noexcept
{
    m_nAchievementID = nID;
    SetDirtyFlag(DirtyFlags::ID);
}

void Achievement::SetActive(BOOL bActive) noexcept
{
    if (m_bActive != bActive)
    {
        m_bActive = bActive;
        SetDirtyFlag(DirtyFlags::All);

        if (m_pTrigger && ra::services::ServiceLocator::Exists<ra::services::AchievementRuntime>())
        {
            auto& pRuntime = ra::services::ServiceLocator::GetMutable<ra::services::AchievementRuntime>();
            if (!m_bActive)
                pRuntime.DeactivateAchievement(ID());
            else if (m_bPauseOnReset)
                pRuntime.MonitorAchievementReset(ID(), static_cast<rc_trigger_t*>(m_pTrigger));
            else
                pRuntime.ActivateAchievement(ID(), static_cast<rc_trigger_t*>(m_pTrigger));
        }
    }
}

void Achievement::SetPauseOnReset(BOOL bPause)
{
    if (m_bPauseOnReset != bPause)
    {
        m_bPauseOnReset = bPause;

        if (m_bActive)
        {
            auto& pRuntime = ra::services::ServiceLocator::GetMutable<ra::services::AchievementRuntime>();
            if (m_bPauseOnReset)
                pRuntime.MonitorAchievementReset(ID(), static_cast<rc_trigger_t*>(m_pTrigger));
            else
                pRuntime.ActivateAchievement(ID(), static_cast<rc_trigger_t*>(m_pTrigger));
        }
    }
}

void Achievement::SetModified(BOOL bModified) noexcept
{
    if (m_bModified != bModified)
    {
        m_bModified = bModified;
        SetDirtyFlag(DirtyFlags::All); // TBD? questionable...
    }
}

void Achievement::SetBadgeImage(const std::string& sBadgeURI)
{
    SetDirtyFlag(DirtyFlags::Badge);

    if (ra::StringEndsWith(sBadgeURI, "_lock"))
        m_sBadgeImageURI.assign(sBadgeURI.c_str(), sBadgeURI.length() - 5);
    else
        m_sBadgeImageURI = sBadgeURI;
}

void Achievement::Reset() noexcept
{
    if (m_pTrigger)
    {
        rc_trigger_t* pTrigger = static_cast<rc_trigger_t*>(m_pTrigger);
        rc_reset_trigger(pTrigger);

        SetDirtyFlag(DirtyFlags::Conditions);
    }
}

size_t Achievement::AddCondition(size_t nConditionGroup, const Condition& rNewCond)
{
    while (NumConditionGroups() <= nConditionGroup) //	ENSURE this is a legal op!
        m_vConditions.AddGroup();

    ConditionGroup& group = m_vConditions.GetGroup(nConditionGroup);
    group.Add(rNewCond); //	NB. Copy by value
    SetDirtyFlag(DirtyFlags::All);

    return group.Count();
}

size_t Achievement::InsertCondition(size_t nConditionGroup, size_t nIndex, const Condition& rNewCond)
{
    while (NumConditionGroups() <= nConditionGroup) //	ENSURE this is a legal op!
        m_vConditions.AddGroup();

    ConditionGroup& group = m_vConditions.GetGroup(nConditionGroup);
    group.Insert(nIndex, rNewCond); //	NB. Copy by value
    SetDirtyFlag(DirtyFlags::All);

    return group.Count();
}

BOOL Achievement::RemoveCondition(size_t nConditionGroup, unsigned int nID)
{
    if (nConditionGroup < m_vConditions.GroupCount())
    {
        m_vConditions.GetGroup(nConditionGroup).RemoveAt(nID);
        SetDirtyFlag(DirtyFlags::All); //	Not Conditions:
        return TRUE;
    }

    return FALSE;
}

void Achievement::RemoveAllConditions(size_t nConditionGroup)
{
    if (nConditionGroup < m_vConditions.GroupCount())
    {
        m_vConditions.GetGroup(nConditionGroup).Clear();
        SetDirtyFlag(DirtyFlags::All); //	All - not just conditions
    }
}

std::string Achievement::CreateMemString() const
{
    std::string buffer;
    m_vConditions.Serialize(buffer);
    return buffer;
}

void Achievement::CopyFrom(const Achievement& rRHS)
{
    SetAuthor(rRHS.m_sAuthor);
    SetDescription(rRHS.m_sDescription);
    SetPoints(rRHS.m_nPointValue);
    SetTitle(rRHS.m_sTitle);
    SetModified(rRHS.m_bModified);
    SetBadgeImage(rRHS.m_sBadgeImageURI);

    // TBD: set to 'now'?
    SetModifiedDate(rRHS.m_nTimestampModified);
    SetCreatedDate(rRHS.m_nTimestampCreated);

    ParseTrigger(rRHS.CreateMemString().c_str());

    SetDirtyFlag(DirtyFlags::All);
}

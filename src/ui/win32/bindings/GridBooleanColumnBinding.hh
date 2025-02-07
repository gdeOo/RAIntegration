#ifndef RA_UI_WIN32_GRIDBOOLEANCOLUMNBINDING_H
#define RA_UI_WIN32_GRIDBOOLEANCOLUMNBINDING_H
#pragma once

#include "GridColumnBinding.hh"
#include "ui\ModelProperty.hh"

namespace ra {
namespace ui {
namespace win32 {
namespace bindings {

class GridBooleanColumnBinding : public GridColumnBinding
{
public:
    GridBooleanColumnBinding(const BoolModelProperty& pBoundProperty, const std::wstring& sTrueText, const std::wstring& sFalseText) noexcept
        : m_pBoundProperty(&pBoundProperty), m_sTrueText(sTrueText), m_sFalseText(sFalseText)
    {
    }

    std::wstring GetText(const ra::ui::ViewModelCollectionBase& vmItems, gsl::index nIndex) const override
    {
        return vmItems.GetItemValue(nIndex, *m_pBoundProperty) ? m_sTrueText : m_sFalseText;
    }

protected:
    const BoolModelProperty* m_pBoundProperty = nullptr;
    std::wstring m_sTrueText;
    std::wstring m_sFalseText;
};

} // namespace bindings
} // namespace win32
} // namespace ui
} // namespace ra

#endif // !RA_UI_WIN32_GRIDBOOLEANCOLUMNBINDING_H

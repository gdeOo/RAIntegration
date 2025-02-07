#ifndef RA_UI_WIN32_TEXTBOXBINDING_H
#define RA_UI_WIN32_TEXTBOXBINDING_H
#pragma once

#include "ControlBinding.hh"

namespace ra {
namespace ui {
namespace win32 {
namespace bindings {

class TextBoxBinding : public ControlBinding
{
public:
    explicit TextBoxBinding(ViewModelBase& vmViewModel) noexcept : ControlBinding(vmViewModel) {}

    void SetHWND(DialogBase& pDialog, HWND hControl) override
    {
        ControlBinding::SetHWND(pDialog, hControl);

        if (m_pTextBoundProperty)
            SetWindowTextW(hControl, GetValue(*m_pTextBoundProperty).c_str());
    }

    enum class UpdateMode
    {
        None = 0,  // one way from source
        LostFocus, // update source when control loses focus
        KeyPress,  // update source after each key press
    };

    void BindText(const StringModelProperty& pSourceProperty, UpdateMode nUpdateMode = UpdateMode::LostFocus)
    {
        m_pTextBoundProperty = &pSourceProperty;
        m_pTextUpdateMode = nUpdateMode;

        UpdateBoundText();
    }

    void OnLostFocus() override
    {
        if (m_pTextUpdateMode == UpdateMode::LostFocus)
            UpdateSourceText();
    }

    void OnValueChanged() override
    {
        if (m_pTextUpdateMode == UpdateMode::KeyPress)
            UpdateSourceText();
    }

protected:
    void OnViewModelStringValueChanged(const StringModelProperty::ChangeArgs& args) override
    {
        if (m_pTextBoundProperty && *m_pTextBoundProperty == args.Property)
            UpdateBoundText();
    }

private:
    void UpdateSourceText()
    {
        const int nLength = GetWindowTextLengthW(m_hWnd);

        std::wstring sBuffer;
        sBuffer.resize(nLength + 1);
        GetWindowTextW(m_hWnd, sBuffer.data(), sBuffer.capacity());
        sBuffer.resize(nLength);

        SetValue(*m_pTextBoundProperty, sBuffer);
    }

    void UpdateBoundText()
    {
        if (m_hWnd && m_pTextBoundProperty)
            SetWindowTextW(m_hWnd, GetValue(*m_pTextBoundProperty).c_str());
    }

    const StringModelProperty* m_pTextBoundProperty = nullptr;
    UpdateMode m_pTextUpdateMode = UpdateMode::None;
};

} // namespace bindings
} // namespace win32
} // namespace ui
} // namespace ra

#endif // !RA_UI_WIN32_TEXTBOXBINDING_H

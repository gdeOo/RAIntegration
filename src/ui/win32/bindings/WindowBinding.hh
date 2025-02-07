#ifndef RA_UI_WIN32_WINDOWBINDING_H
#define RA_UI_WIN32_WINDOWBINDING_H
#pragma once

#include "ui/BindingBase.hh"
#include "ui/WindowViewModelBase.hh"

namespace ra {
namespace ui {
namespace win32 {
namespace bindings {

class WindowBinding : protected BindingBase
{
public:
    explicit WindowBinding(WindowViewModelBase& vmWindowViewModel) noexcept
        : BindingBase(vmWindowViewModel)
    {
        GSL_SUPPRESS_F6 s_vKnownBindings.push_back(this);
    }

    GSL_SUPPRESS_F6 ~WindowBinding() noexcept
    {
        for (auto iter = s_vKnownBindings.begin(); iter != s_vKnownBindings.end(); ++iter)
        {
            if (*iter == this)
            {
                s_vKnownBindings.erase(iter);
                break;
            }
        }
    }

    WindowBinding(const WindowBinding&) noexcept = delete;
    WindowBinding& operator=(const WindowBinding&) noexcept = delete;
    WindowBinding(WindowBinding&&) noexcept = delete;
    WindowBinding& operator=(WindowBinding&&) noexcept = delete;

    /// <summary>
    /// Gets the binding for the provided <see cref="WindowViewModelBase" />.
    /// </summary>
    /// <param name="vmWindowViewModel">The window view model.</param>
    /// <returns>Associated binding, <c>nullptr</c> if not found.</returns>
    static WindowBinding* GetBindingFor(const WindowViewModelBase& vmWindowViewModel) noexcept
    {
        for (auto* pBinding : s_vKnownBindings)
        {
            if (pBinding && &pBinding->GetViewModel<WindowViewModelBase>() == &vmWindowViewModel)
                return pBinding;
        }

        return nullptr;
    }
    
    /// <summary>
    /// Sets the unique identifier for remembering this window's size and position.
    /// </summary>
    /// <param name="nDefaultHorizontalLocation">The default horizontal location if a custom placement is not known.</param>
    /// <param name="nDefaultVerticalLocation">The default vertical location if a custom placement is not known.</param>
    /// <param name="sSizeAndPositionKey">The unique identifier for the window.</param>
    void SetInitialPosition(RelativePosition nDefaultHorizontalLocation, RelativePosition nDefaultVerticalLocation, const char* sSizeAndPositionKey = nullptr);
    
    /// <summary>
    /// Associates the <see cref="HWND" /> of a window for binding.
    /// </summary>
    /// <param name="hWnd">The window handle.</param>
    void SetHWND(HWND hWnd);
    
    /// <summary>
    /// Gets the HWND bound to the window.
    /// </summary>
    /// <returns>The window handle.</returns>
    HWND GetHWnd() const noexcept { return m_hWnd; }

    /// <summary>
    /// Binds the a label to a property of the viewmodel.
    /// </summary>
    /// <param name="nDlgItemId">The unique identifier of the label in the dialog.</param>
    /// <param name="pSourceProperty">The property to bind to.</param>
    void BindLabel(int nDlgItemId, const StringModelProperty& pSourceProperty);

    
    /// <summary>
    /// Called when the window's size changes.
    /// </summary>
    /// <param name="oSize">The new size.</param>
    void OnSizeChanged(ra::ui::Size oSize);
    
    /// <summary>
    /// Called when the window's position changes.
    /// </summary>
    /// <param name="oPosition">The new position.</param>
    void OnPositionChanged(ra::ui::Position oPosition);

protected:
    GSL_SUPPRESS_F6 void OnViewModelStringValueChanged(const StringModelProperty::ChangeArgs& args) noexcept override;

    void RestoreSizeAndPosition();

private:
    std::unordered_map<int, int> m_mLabelBindings;

    std::string m_sSizeAndPositionKey;
    RelativePosition m_nDefaultHorizontalLocation{ RelativePosition::None };
    RelativePosition m_nDefaultVerticalLocation{ RelativePosition::None };
    
    HWND m_hWnd{};

    static std::vector<WindowBinding*> s_vKnownBindings;
};

} // namespace bindings
} // namespace win32
} // namespace ui
} // namespace ra

#endif // !RA_UI_WIN32_WINDOWBINDING_H

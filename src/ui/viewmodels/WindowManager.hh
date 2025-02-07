#ifndef RA_UI_WINDOW_MANAGER
#define RA_UI_WINDOW_MANAGER

#include "EmulatorViewModel.hh"
#include "RichPresenceMonitorViewModel.hh"

namespace ra {
namespace ui {
namespace viewmodels {

class WindowManager {
public:
    EmulatorViewModel Emulator;
    RichPresenceMonitorViewModel RichPresenceMonitor;
};

} // namespace viewmodels
} // namespace ui
} // namespace ra

#endif // !RA_UI_WINDOW_MANAGER

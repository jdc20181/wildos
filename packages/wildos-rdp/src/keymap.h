#pragma once

#include <cstdint>

namespace keymap
{
// The RDP scancode (FreeRDP's form: the make code, with 0x100 for the E0
// extended keys) for a Linux evdev key code, or 0 for a key RDP has no
// scancode for.
uint32_t rdpScancode(uint32_t evdev);

// The Windows keyboard layout (KLID) matching the person's first Plasma
// keyboard layout, or their language when Plasma names none; 0 when
// neither is known, which leaves Windows on its own default.
uint32_t windowsLayout();
}

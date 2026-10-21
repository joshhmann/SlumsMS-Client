#pragma once

// Which way a v83 IUIMsgHandler::OnMouseWheel nWheel points.
//
// CWndMan::ProcessMouse builds it from WM_MOUSEWHEEL as GET_WHEEL_DELTA_WPARAM / -120
// (0x009E3DE9: shr eax,16; movsx; push -0x78; idiv), so the sign is the OPPOSITE of
// Windows': wheel UP (away from the user) arrives NEGATIVE, wheel down positive. A
// touchpad notch under 120 truncates to 0, which is no movement.
//
// Returns the row delta for a list that scrolls one row per notch: -1 for wheel up
// (earlier rows), +1 for wheel down, 0 for nothing.
inline int WheelRowStep(int nWheel) {
    return (nWheel < 0) ? -1 : (nWheel > 0) ? 1 : 0;
}

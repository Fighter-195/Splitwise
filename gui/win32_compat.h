// Splitwise — Win32 compatibility shim for the vendored Dear ImGui backend.
//
// The MinGW.org (mingw32) toolchain ships very old Win32 headers that are
// missing a handful of symbols the ImGui Win32 backend uses. This header fills
// exactly those gaps. It is force-included into the GUI build via `-include`.
//
// It does nothing on modern mingw-w64 (which defines __MINGW64_VERSION_MAJOR and
// already has all of these), so the GUI still builds cleanly on a normal setup.

#ifndef SPLITWISE_WIN32_COMPAT_H
#define SPLITWISE_WIN32_COMPAT_H

#include <windows.h>  // pulls in _mingw.h, which sets __MINGW64_VERSION_MAJOR on mingw-w64

#ifndef __MINGW64_VERSION_MAJOR
// ---- Old MinGW.org headers only --------------------------------------------

// TrackMouseEvent flag for tracking mouse leave over the non-client area.
#ifndef TME_NONCLIENT
#define TME_NONCLIENT 0x00000010
#endif

// Extracts the X-button index from a WM_XBUTTON* message's wParam.
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(wParam) (HIWORD(wParam))
#endif

// The backend refers to RTL_OSVERSIONINFOEXW, which MinGW.org doesn't name — but
// it has the identically-laid-out OSVERSIONINFOEXW, and the backend passes the
// value to an OSVERSIONINFOEXW* function pointer, so alias the two.
typedef OSVERSIONINFOEXW RTL_OSVERSIONINFOEXW;
typedef OSVERSIONINFOEXW* PRTL_OSVERSIONINFOEXW;

#endif // __MINGW64_VERSION_MAJOR

#endif // SPLITWISE_WIN32_COMPAT_H

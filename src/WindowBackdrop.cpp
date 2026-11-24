#include "WindowBackdrop.h"
#include <QRegion>
#include <QTransform>
#include <QtMath>
#include <cmath>

#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#include <dwmapi.h>
#endif

bool enableWindowBackdrop(WId windowId, BackdropStyle style)
{
#ifdef Q_OS_WIN
    struct AccentPolicy {
        DWORD state;
        DWORD flags;
        DWORD tint;
        DWORD animationId;
    };
    struct CompositionAttributeData {
        DWORD attribute;
        const void *data;
        SIZE_T size;
    };
    using SetCompositionAttribute = BOOL (WINAPI *)(HWND, const CompositionAttributeData *);
    static const auto setAttribute = reinterpret_cast<SetCompositionAttribute>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!setAttribute)
        return false;

    const auto window = reinterpret_cast<HWND>(windowId);
    constexpr DWORD borderColorAttribute = 34;
    constexpr DWORD noBorderColor = 0xFFFFFFFE;
    DwmSetWindowAttribute(window, borderColorAttribute, &noBorderColor, sizeof(noBorderColor));

    constexpr DWORD accentPolicyAttribute = 19;
    constexpr DWORD acrylicBlurBehind = 4;
    constexpr DWORD blurBehind = 3;
    // With this accent flag, DWM clips the blur to the window's native region.
    constexpr DWORD clipToWindowRegion = 0x10;
    const auto applyPolicy = [=](DWORD state) {
        // Let the Qt surface supply the tint. Both themes use the same
        // compositor path while the HWND changes size during docking.
        const AccentPolicy policy{state, clipToWindowRegion, 0x01F5F5F5, 0};
        const CompositionAttributeData data{accentPolicyAttribute, &policy, sizeof(policy)};
        return setAttribute(window, &data) != FALSE;
    };
    if (style == BackdropStyle::PopupMenu)
        return applyPolicy(acrylicBlurBehind) || applyPolicy(blurBehind);
    return applyPolicy(blurBehind) || applyPolicy(acrylicBlurBehind);
#else
    Q_UNUSED(windowId);
    Q_UNUSED(style);
    return false;
#endif
}

bool windowAnimationsEnabled()
{
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
    return enabled != FALSE;
#else
    return true;
#endif
}

void clipPopupBackdrop(WId windowId, QSize pixels, qreal pixelRadius)
{
#ifdef Q_OS_WIN
    const int w = pixels.width();
    const int h = pixels.height();
    const qreal radius = qMin(pixelRadius, qMin(w, h) / 2.0);
    QRegion outline(0, 0, w, h);
    // DWM clips to whole pixels. Exclude only fully exterior pixels so Qt
    // can still paint every partially covered pixel of the antialiased edge.
    for (int y = 0; y < qFloor(radius); ++y) {
        const qreal dy = radius - (y + 1);
        const int inset = qFloor(radius - std::sqrt(radius * radius - dy * dy));
        if (!inset)
            break;
        for (const int row : {y, h - 1 - y}) {
            outline -= QRect(0, row, inset, 1);
            outline -= QRect(w - inset, row, inset, 1);
        }
    }
    const HRGN native = outline.toHRGN();
    if (native && !SetWindowRgn(reinterpret_cast<HWND>(windowId), native, TRUE))
        DeleteObject(native); // Windows owns the region after a successful call.
#else
    Q_UNUSED(windowId);
    Q_UNUSED(pixels);
    Q_UNUSED(pixelRadius);
#endif
}

void placeWindowBelow(WId windowId, WId above)
{
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(windowId), reinterpret_cast<HWND>(above),
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    Q_UNUSED(windowId);
    Q_UNUSED(above);
#endif
}

bool panelWindowOnTop(WId windowId)
{
#ifdef Q_OS_WIN
    const auto window = reinterpret_cast<HWND>(windowId);
    RECT region;
    return IsWindowVisible(window) && !IsIconic(window)
        && (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST)
        && MonitorFromWindow(window, MONITOR_DEFAULTTONULL)
        && GetWindowRgnBox(window, &region) > NULLREGION;
#else
    Q_UNUSED(windowId);
    return true;
#endif
}

void restorePanelWindow(WId windowId, const QRegion &mask, qreal scale)
{
#ifdef Q_OS_WIN
    const auto window = reinterpret_cast<HWND>(windowId);
    // Qt skips identical masks, even when the new native window has no region yet.
    const auto nativeMask = QTransform::fromScale(scale, scale).map(mask).toHRGN();
    if (nativeMask && !SetWindowRgn(window, nativeMask, TRUE))
        DeleteObject(nativeMask);
    if (IsIconic(window))
        ShowWindow(window, SW_SHOWNOACTIVATE);
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
    Q_UNUSED(mask);
    Q_UNUSED(scale);
#endif
    enableWindowBackdrop(windowId);
}

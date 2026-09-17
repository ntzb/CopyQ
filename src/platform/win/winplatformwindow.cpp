// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/platformcommon.h"
#include "winplatformwindow.h"

#include "common/appconfig.h"
#include "common/log.h"
#include "common/sleeptimer.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QString>
#include <QVector>
#include <QWidget>

namespace {

QString windowTitle(HWND window)
{
    WCHAR buf[1024];
    GetWindowTextW(window, buf, 1024);
    return QString::fromUtf16(reinterpret_cast<ushort *>(buf));
}

INPUT createInput(WORD key, DWORD flags = 0)
{
    INPUT input;

    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.wScan = 0;
    input.ki.dwFlags = KEYEVENTF_UNICODE | flags;
    input.ki.time = 0;
    input.ki.dwExtraInfo = GetMessageExtraInfo();

    return input;
}

QString windowLogText(QString text, HWND window)
{
    const QString windowInfo =
            QString("%1").arg(reinterpret_cast<quintptr>(window))
            + " \"" + windowTitle(window) + "\"";

    text.prepend("Window " + windowInfo + ": ");

    const DWORD lastError = GetLastError();
    if (lastError != 0)
        text.append( QString(" (last error is %1)").arg(GetLastError()) );

    return text;
}

void logWindowWarning(const char *text, HWND window)
{
    log( windowLogText(text, window), LogWarning );
}

void logWindowDebug(const char *text, HWND window)
{
    COPYQ_LOG( windowLogText(text, window) );
}

DWORD windowProcessId(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    return processId;
}

/**
 * Return true if the target window (or another window of the same
 * application) is in the foreground.
 *
 * Comparing the process instead of the window handle avoids false negatives
 * for applications that move the focus to a different top level window of
 * their own when activated.
 */
bool isWindowActive(HWND window)
{
    const HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == window)
        return true;

    const DWORD processId = windowProcessId(window);
    return processId != 0 && processId == windowProcessId(foregroundWindow);
}

bool isWindowInForeground(HWND window)
{
    return GetForegroundWindow() == window;
}

bool waitForWindowActive(HWND window, int timeoutMs)
{
    // Note: Must not be called while thread input is attached to another
    // thread, otherwise the other application can stop processing messages.
    for (int elapsedMs = 0; elapsedMs < timeoutMs; elapsedMs += 5) {
        if ( isWindowActive(window) )
            return true;
        Sleep(5);
    }

    return isWindowActive(window);
}

/**
 * Claim the last input event for this process.
 *
 * Windows only allows the process that received the last input event to
 * change the foreground window. Any other process is silently refused:
 * SetForegroundWindow() still reports success, but it only flashes the task
 * bar button instead of activating the window. This is why pasting works
 * when the main window or the tray menu is opened with a global shortcut
 * (the key press is delivered to this process) but not when it is opened
 * with the mouse (the click is delivered to the shell).
 *
 * Injecting a key press satisfies the check. VK_NONAME is reserved and
 * ignored by applications, so it cannot disturb the target window.
 */
void claimLastInputEvent()
{
    INPUT input[] = {
        createInput(VK_NONAME),
        createInput(VK_NONAME, KEYEVENTF_KEYUP)
    };
    SendInput( 2, input, sizeof(INPUT) );
}

void setForegroundWindow(HWND window)
{
    const auto thisThreadId = GetCurrentThreadId();
    const auto foregroundThreadId = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    const auto targetThreadId = GetWindowThreadProcessId(window, nullptr);

    // Sharing the input queue with the foreground window lifts the
    // restrictions on changing the foreground window, and sharing it with
    // the target window is required for SetActiveWindow().
    const bool attachedForeground = foregroundThreadId != 0
            && foregroundThreadId != thisThreadId
            && AttachThreadInput(thisThreadId, foregroundThreadId, true);
    const bool attachedTarget = targetThreadId != 0
            && targetThreadId != thisThreadId
            && targetThreadId != foregroundThreadId
            && AttachThreadInput(thisThreadId, targetThreadId, true);

    COPYQ_LOG( windowLogText(
        QStringLiteral("Raising (attached foreground: %1, attached target: %2)")
        .arg(attachedForeground ? 1 : 0)
        .arg(attachedTarget ? 1 : 0), window) );

    SetForegroundWindow(window);
    BringWindowToTop(window);
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                 SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    // Note: Only activate the top level window. Setting the focus directly
    // would take it from the child widget that is supposed to be pasted to.
    SetActiveWindow(window);

    if (attachedTarget)
        AttachThreadInput(thisThreadId, targetThreadId, false);
    if (attachedForeground)
        AttachThreadInput(thisThreadId, foregroundThreadId, false);
}

bool raiseWindow(HWND window, int timeoutMs)
{
    if (!IsWindowVisible(window)) {
        logWindowWarning("Failed to raise: IsWindowVisible() == false", window);
        return false;
    }

    if ( isWindowInForeground(window) ) {
        logWindowDebug("Already in foreground", window);
        return true;
    }

    setForegroundWindow(window);
    if ( waitForWindowActive(window, timeoutMs) ) {
        logWindowDebug("Raised", window);
        return true;
    }

    logWindowDebug("Failed to raise, retrying with the last input event claimed", window);
    claimLastInputEvent();
    setForegroundWindow(window);
    if ( waitForWindowActive(window, timeoutMs) ) {
        logWindowDebug("Raised after claiming the last input event", window);
        return true;
    }

    logWindowWarning("Failed to raise: window did not become active", window);
    return false;
}

bool isKeyPressed(int key)
{
    return GetKeyState(key) & 0x8000;
}

bool isModifierPressed()
{
    return isKeyPressed(VK_LWIN)
        || isKeyPressed(VK_RWIN)
        || isKeyPressed(VK_LCONTROL)
        || isKeyPressed(VK_RCONTROL)
        || isKeyPressed(VK_LSHIFT)
        || isKeyPressed(VK_RSHIFT)
        || isKeyPressed(VK_LMENU)
        || isKeyPressed(VK_RMENU)
        || isKeyPressed(VK_MENU);
}

bool waitForModifiersReleased(const AppConfig &config)
{
    const int maxWaitForModsReleaseMs = config.option<Config::window_wait_for_modifier_released_ms>();
    if (maxWaitForModsReleaseMs >= 0) {
        SleepTimer t(maxWaitForModsReleaseMs);
        while (t.sleep()) {
            if (!isModifierPressed())
                return true;
        }
    }

    return !isModifierPressed();
}

bool sendInputs(QVector<INPUT> input, HWND wnd)
{
    const UINT numberOfAddedEvents = SendInput( input.size(), input.data(), sizeof(INPUT) );
    if (numberOfAddedEvents == 0u) {
        logWindowWarning("Failed to simulate key events", wnd);
        return false;
    }
    return true;
}

} // namespace

WinPlatformWindow::WinPlatformWindow(HWND window)
    : m_window(window)
{
}

bool WinPlatformWindow::matchesWidget(const QWidget *widget) const
{
    return widget
        && widget->windowHandle()
        && reinterpret_cast<HWND>(widget->winId()) == m_window;
}


QString WinPlatformWindow::getTitle()
{
    return windowTitle(m_window);
}

void WinPlatformWindow::raise()
{
    const AppConfig config;
    raiseWindow( m_window, config.option<Config::window_wait_raised_ms>() );
}

bool WinPlatformWindow::pasteFromClipboard()
{
    const AppConfig config;

    if ( pasteWithCtrlV(*this, config) )
        return sendKeyPress(VK_LCONTROL, 'V', config);

    return sendKeyPress(VK_LSHIFT, VK_INSERT, config);
}

bool WinPlatformWindow::copyToClipboard()
{
    const AppConfig config;

    const DWORD clipboardSequenceNumber = GetClipboardSequenceNumber();
    if ( !sendKeyPress(VK_LCONTROL, 'C', config) )
        return false;

    // Wait for clipboard to change.
    QElapsedTimer t;
    t.start();
    while ( clipboardSequenceNumber == GetClipboardSequenceNumber() && t.elapsed() < 2000 )
        QApplication::processEvents(QEventLoop::AllEvents, 100);

    return clipboardSequenceNumber != GetClipboardSequenceNumber();
}

bool WinPlatformWindow::sendKeyPress(WORD modifier, WORD key, const AppConfig &config)
{
    waitMs(config.option<Config::window_wait_before_raise_ms>());

    if ( !raiseWindow(m_window, config.option<Config::window_wait_raised_ms>()) )
        return false;

    waitMs(config.option<Config::window_wait_after_raised_ms>());

    // Wait for user to release modifiers.
    if (!waitForModifiersReleased(config)) {
        logWindowWarning("Failed to simulate key presses while modifiers are pressed", m_window);
        return false;
    }

    const int keyPressTimeMs = config.option<Config::window_key_press_time_ms>();
    if (keyPressTimeMs <= 0) {
        return sendInputs({
           createInput(modifier),
           createInput(key),
           createInput(key, KEYEVENTF_KEYUP),
           createInput(modifier, KEYEVENTF_KEYUP)
        }, m_window);
    }

    const bool sent = sendInputs({
        createInput(modifier),
        createInput(key)
    }, m_window);
    if (!sent)
        return false;

    waitMs(keyPressTimeMs);

    return sendInputs({
       createInput(key, KEYEVENTF_KEYUP),
       createInput(modifier, KEYEVENTF_KEYUP)
    }, m_window);
}

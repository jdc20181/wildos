#include "fakekeyboard.h"

#include "qwayland-fake-input.h"

#include <QGuiApplication>
#include <QWaylandClientExtensionTemplate>
#include <wayland-client.h>

namespace
{
// Linux input event codes (linux/input-event-codes.h).
constexpr uint32_t keyLeftCtrl = 29;
constexpr uint32_t keyLeftShift = 42;
constexpr uint32_t keyV = 47;
constexpr uint32_t pressed = 1;
constexpr uint32_t released = 0;
}

class FakeInput : public QWaylandClientExtensionTemplate<FakeInput>, public QtWayland::org_kde_kwin_fake_input
{
public:
    FakeInput()
        : QWaylandClientExtensionTemplate<FakeInput>(4)
    {
        initialize();
    }

    ~FakeInput() override
    {
        if (isActive()) {
            destroy();
        }
    }

    bool authenticated = false;
};

FakeKeyboard::FakeKeyboard(QObject *parent)
    : QObject(parent)
{
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        m_input = std::make_unique<FakeInput>();
    }
}

FakeKeyboard::~FakeKeyboard() = default;

bool FakeKeyboard::available() const
{
    return m_input && m_input->isActive();
}

bool FakeKeyboard::paste(bool shift)
{
    if (!available()) {
        return false;
    }
    if (!m_input->authenticated) {
        m_input->authenticate(QStringLiteral("Clipboard and emoji"), QStringLiteral("Paste what you picked into the app you are using"));
        m_input->authenticated = true;
    }
    m_input->keyboard_key(keyLeftCtrl, pressed);
    if (shift) {
        m_input->keyboard_key(keyLeftShift, pressed);
    }
    m_input->keyboard_key(keyV, pressed);
    m_input->keyboard_key(keyV, released);
    if (shift) {
        m_input->keyboard_key(keyLeftShift, released);
    }
    m_input->keyboard_key(keyLeftCtrl, released);
    if (auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
        wl_display_flush(app->display());
    }
    return true;
}

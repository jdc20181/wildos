#pragma once

#include <QObject>
#include <memory>

class FakeInput;

// Presses Ctrl+V in whichever window has the keyboard, through KWin's fake
// input protocol. It is how the clipboard and emoji panel puts what you
// picked into the app you were typing in: Wayland gives no other way for
// one app to type into another. KWin only offers the protocol to programs
// whose desktop file lists org_kde_kwin_fake_input.
class FakeKeyboard : public QObject
{
    Q_OBJECT

public:
    explicit FakeKeyboard(QObject *parent = nullptr);
    ~FakeKeyboard() override;

    bool available() const;
    // Ctrl+V, or Ctrl+Shift+V for terminals, which keep Ctrl+V for
    // themselves.
    bool paste(bool shift = false);

private:
    std::unique_ptr<FakeInput> m_input;
};

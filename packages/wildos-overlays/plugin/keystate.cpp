#include "keystate.h"

#include <KModifierKeyInfo>

KeyState::KeyState(QObject *parent)
    : QObject(parent)
    , m_info(new KModifierKeyInfo(this))
{
    m_caps = m_info->isKeyLocked(Qt::Key_CapsLock);
    m_num = m_info->isKeyLocked(Qt::Key_NumLock);
    connect(m_info, &KModifierKeyInfo::keyLocked, this, [this](Qt::Key key, bool locked) {
        if (key == Qt::Key_CapsLock && locked != m_caps) {
            m_caps = locked;
            Q_EMIT capsLockChanged();
            Q_EMIT toggled(true, locked);
        } else if (key == Qt::Key_NumLock && locked != m_num) {
            m_num = locked;
            Q_EMIT numLockChanged();
            Q_EMIT toggled(false, locked);
        }
    });
}

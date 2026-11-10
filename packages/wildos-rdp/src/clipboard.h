#pragma once

#include <QByteArray>
#include <QObject>

#include <freerdp/client/cliprdr.h>

#include <mutex>

// Copy and paste between Windows apps and the desktop: text and pictures,
// both ways. When Windows says it has something new on its clipboard the
// text and picture are fetched at once and put on the desktop's clipboard;
// when the desktop's clipboard changes, Windows is told which of the two
// it can ask for, and gets them when it asks.
class Clipboard : public QObject
{
    Q_OBJECT

public:
    explicit Clipboard(QObject *parent = nullptr);

    // On FreeRDP's channel threads.
    void attach(CliprdrClientContext *context);
    void detach(CliprdrClientContext *context);

private:
    static UINT monitorReady(CliprdrClientContext *context, const CLIPRDR_MONITOR_READY *ready);
    static UINT serverCapabilities(CliprdrClientContext *context, const CLIPRDR_CAPABILITIES *caps);
    static UINT serverFormatList(CliprdrClientContext *context, const CLIPRDR_FORMAT_LIST *list);
    static UINT serverFormatListResponse(CliprdrClientContext *context, const CLIPRDR_FORMAT_LIST_RESPONSE *response);
    static UINT serverDataRequest(CliprdrClientContext *context, const CLIPRDR_FORMAT_DATA_REQUEST *request);
    static UINT serverDataResponse(CliprdrClientContext *context, const CLIPRDR_FORMAT_DATA_RESPONSE *response);

    // On the GUI thread.
    void announce();
    void fetchNext();
    void received(quint32 format, const QByteArray &data);
    void answer(quint32 format);

    std::mutex m_lock;
    CliprdrClientContext *m_context = nullptr;
    bool m_ready = false;

    // What Windows offered last, and what has arrived of it so far.
    bool m_offerText = false;
    bool m_offerImage = false;
    quint32 m_requested = 0;
    QByteArray m_text;
    QByteArray m_image;
    bool m_settingOwn = false;
};

#include "notedocument.h"
#include "notestore.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

NoteDocument::NoteDocument(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(600);
    connect(&m_timer, &QTimer::timeout, this, &NoteDocument::save);
    NoteStore *store = NoteStore::instance();
    connect(store, &NoteStore::fileChanged, this, &NoteDocument::reloadFromDisk);
    connect(store, &NoteStore::pathMoved, this, [this](const QString &from, const QString &to) {
        if (m_path.isEmpty()) {
            return;
        }
        if (from.endsWith(u'/') && m_path.startsWith(from)) {
            m_path = to + m_path.mid(from.size());
        } else if (m_path == from) {
            m_path = to;
        } else {
            return;
        }
        Q_EMIT pathChanged();
    });
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &NoteDocument::save);
}

NoteDocument::~NoteDocument()
{
    save();
}

QString NoteDocument::path() const
{
    return m_path;
}

QString NoteDocument::notebook() const
{
    return m_path.isEmpty() ? QString() : NoteStore::instance()->notebookOf(m_path);
}

QString NoteDocument::noteDir() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
}

bool NoteDocument::isOpen() const
{
    return !m_path.isEmpty();
}

QString NoteDocument::title() const
{
    const QString heading = NoteFile::headingText(m_parts.titleLine);
    if (!heading.isEmpty() || m_createdHere) {
        return heading;
    }
    return m_fileTitle;
}

void NoteDocument::setTitle(const QString &title)
{
    if (m_path.isEmpty() || readOnly() || title == this->title()) {
        return;
    }
    if (!m_parts.titleLine.isEmpty() || m_fresh || m_createdHere) {
        NoteFile::setTitle(m_parts, title);
    } else {
        m_fileTitle = title;
    }
    m_fresh = false;
    m_renamePending = true;
    Q_EMIT titleChanged();
    changed();
}

bool NoteDocument::pinned() const
{
    return NoteFile::pinned(m_parts);
}

void NoteDocument::setPinned(bool pinned)
{
    if (m_path.isEmpty() || readOnly() || pinned == this->pinned()) {
        return;
    }
    NoteFile::setPinned(m_parts, pinned);
    Q_EMIT metaChanged();
    changed();
    // The list moves the note up at once rather than after the pause.
    save();
}

QString NoteDocument::colour() const
{
    return NoteFile::colour(m_parts);
}

void NoteDocument::setColour(const QString &colour)
{
    if (m_path.isEmpty() || readOnly() || colour == this->colour()) {
        return;
    }
    NoteFile::setColour(m_parts, colour);
    Q_EMIT metaChanged();
    changed();
    save();
}

QStringList NoteDocument::tags() const
{
    return m_tags;
}

bool NoteDocument::saved() const
{
    return !m_dirty;
}

QString NoteDocument::editedText() const
{
    return NoteStore::editedText(m_modified);
}

bool NoteDocument::readOnly() const
{
    return !m_parts.valid;
}

bool NoteDocument::load(const QString &path)
{
    if (path == m_path) {
        return true;
    }
    close();
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    m_written = file.readAll();
    m_parts = NoteFile::parse(m_written);
    m_path = QFileInfo(path).absoluteFilePath();
    m_fileTitle = QFileInfo(path).completeBaseName();
    m_modified = QFileInfo(path).lastModified();
    m_fresh = false;
    m_createdHere = false;
    m_dirty = false;
    m_renamePending = false;
    updateTags();
    Q_EMIT pathChanged();
    Q_EMIT titleChanged();
    Q_EMIT metaChanged();
    Q_EMIT savedChanged();
    Q_EMIT bodyReplaced();
    return true;
}

bool NoteDocument::create(const QString &notebook)
{
    const QString path = NoteStore::instance()->createNote(notebook);
    if (path.isEmpty() || !load(path)) {
        return false;
    }
    m_fresh = true;
    m_createdHere = true;
    Q_EMIT titleChanged();
    return true;
}

QString NoteDocument::body() const
{
    return m_parts.body;
}

void NoteDocument::setBody(const QString &body)
{
    if (m_path.isEmpty() || readOnly() || body == m_parts.body) {
        return;
    }
    m_parts.body = body;
    if (m_fresh) {
        // Text typed under an empty title still gives the note a heading
        // once a title is typed; until then the file name stands in.
        m_fresh = false;
        Q_EMIT titleChanged();
    }
    changed();
}

void NoteDocument::changed()
{
    const bool was = m_dirty;
    m_dirty = true;
    m_timer.start();
    updateTags();
    if (!was) {
        Q_EMIT savedChanged();
    }
}

void NoteDocument::updateTags()
{
    const QStringList tags = NoteFile::tags(m_parts.titleLine + u'\n' + m_parts.body);
    if (tags != m_tags) {
        m_tags = tags;
        Q_EMIT tagsChanged();
    }
}

void NoteDocument::save()
{
    m_timer.stop();
    if (!m_dirty || m_path.isEmpty() || readOnly()) {
        return;
    }
    const QByteArray bytes = NoteFile::serialize(m_parts);
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        // Kept as changed, so the next pause tries again.
        m_timer.start();
        return;
    }
    m_written = bytes;
    m_dirty = false;

    const QString oldPath = m_path;
    if (m_renamePending) {
        m_renamePending = false;
        QString want = NoteFile::headingText(m_parts.titleLine);
        if (want.isEmpty()) {
            want = m_fileTitle;
        }
        if (!want.trimmed().isEmpty()) {
            const QFileInfo info(m_path);
            const QString base = NoteFile::safeFileName(want);
            if (base != info.completeBaseName()) {
                const QString target = NoteStore::freePath(info.absolutePath(), base, m_path);
                if (target != m_path && QFile::rename(m_path, target)) {
                    m_path = target;
                    m_fileTitle = QFileInfo(target).completeBaseName();
                }
            }
        }
    }
    m_modified = QFileInfo(m_path).lastModified();
    NoteStore::instance()->noteWritten(oldPath, m_path);
    if (m_path != oldPath) {
        Q_EMIT pathChanged();
    }
    Q_EMIT savedChanged();
}

void NoteDocument::close()
{
    if (m_path.isEmpty()) {
        return;
    }
    save();
    const QString path = m_path;
    const bool empty = m_createdHere && m_written.isEmpty() && m_parts.body.isEmpty() && m_parts.titleLine.isEmpty();
    m_path.clear();
    m_parts = NoteParts();
    m_written.clear();
    m_fresh = false;
    m_createdHere = false;
    m_dirty = false;
    m_tags.clear();
    if (empty && QFileInfo(path).size() == 0) {
        // Only a note made here a moment ago and never typed in.
        QFile::remove(path);
        NoteStore::instance()->rescan();
    }
    Q_EMIT pathChanged();
    Q_EMIT titleChanged();
    Q_EMIT metaChanged();
    Q_EMIT tagsChanged();
    Q_EMIT savedChanged();
}

QString NoteDocument::markdown() const
{
    NoteParts plain = m_parts;
    plain.frontMatter.clear();
    plain.bom.clear();
    plain.crlf = false;
    return QString::fromUtf8(NoteFile::serialize(plain));
}

void NoteDocument::reloadFromDisk(const QString &path)
{
    if (path != m_path || m_dirty) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QByteArray bytes = file.readAll();
    if (bytes == m_written) {
        return;
    }
    m_written = bytes;
    m_parts = NoteFile::parse(bytes);
    m_modified = QFileInfo(path).lastModified();
    m_fresh = false;
    updateTags();
    Q_EMIT titleChanged();
    Q_EMIT metaChanged();
    Q_EMIT savedChanged();
    Q_EMIT bodyReplaced();
}

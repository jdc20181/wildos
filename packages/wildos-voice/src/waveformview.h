#pragma once

#include <QColor>
#include <QList>
#include <QQuickPaintedItem>
#include <QVariantList>

// The waveform (AppVoice.dc): 4px rounded bars 3px apart, as tall as the
// sound was loud.
//
// While recording ("live"), the newest bar sits at the playhead, 70% of
// the way across, older bars fade towards the left edge and the rest of
// the line is dotted. Otherwise the whole recording is fitted to the
// width, the part already played in `playedColor`.
class WaveformView : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList bars READ bars WRITE setBars NOTIFY barsChanged)
    Q_PROPERTY(bool live READ live WRITE setLive NOTIFY changed)
    // 0 to 1 through the recording, for playback.
    Q_PROPERTY(double progress READ progress WRITE setProgress NOTIFY changed)
    Q_PROPERTY(QColor barColor READ barColor WRITE setBarColor NOTIFY changed)
    Q_PROPERTY(QColor playedColor READ playedColor WRITE setPlayedColor NOTIFY changed)
    Q_PROPERTY(QColor dotColor READ dotColor WRITE setDotColor NOTIFY changed)
    Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor NOTIFY changed)
    Q_PROPERTY(bool showPlayhead READ showPlayhead WRITE setShowPlayhead NOTIFY changed)
    // Where the playhead is drawn, for the marker flags above it.
    Q_PROPERTY(double playheadX READ playheadX NOTIFY geometryChanged)
    Q_PROPERTY(int slotCount READ slotCount NOTIFY geometryChanged)

public:
    static constexpr int barWidth = 4;
    static constexpr int gap = 3;
    static constexpr double livePlayhead = 0.7;

    explicit WaveformView(QQuickItem *parent = nullptr);

    QVariantList bars() const;
    void setBars(const QVariantList &bars);
    bool live() const { return m_live; }
    void setLive(bool live);
    double progress() const { return m_progress; }
    void setProgress(double progress);
    QColor barColor() const { return m_barColor; }
    void setBarColor(const QColor &color);
    QColor playedColor() const { return m_playedColor; }
    void setPlayedColor(const QColor &color);
    QColor dotColor() const { return m_dotColor; }
    void setDotColor(const QColor &color);
    QColor playheadColor() const { return m_playheadColor; }
    void setPlayheadColor(const QColor &color);
    bool showPlayhead() const { return m_showPlayhead; }
    void setShowPlayhead(bool show);
    double playheadX() const;
    int slotCount() const;

    // Where a moment `agoMs` before the newest bar is drawn while
    // recording; -1 once it has scrolled off.
    Q_INVOKABLE double liveX(double agoMs) const;
    // Where a point `fraction` of the way through is drawn, and back.
    Q_INVOKABLE double fullX(double fraction) const;
    Q_INVOKABLE double fractionAt(double x) const;

    void paint(QPainter *painter) override;

Q_SIGNALS:
    void barsChanged();
    void changed();
    void geometryChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    double slotX(int slot) const;
    int shownSlots() const;
    double left() const;

    QList<float> m_bars;
    bool m_live = false;
    bool m_showPlayhead = true;
    double m_progress = 0;
    QColor m_barColor;
    QColor m_playedColor;
    QColor m_dotColor;
    QColor m_playheadColor;
};

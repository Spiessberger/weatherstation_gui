#pragma once

#include "panoramageometry.h"

#include <QAbstractAnimation>
#include <QImage>
#include <QQuickItem>
#include <QTimer>
#include <QVariantAnimation>

#include <functional>

// Shows a 360° panorama that wraps around endlessly. Drag or flick to pan, pinch or wheel to
// zoom; after idleTimeout ms without input it zooms back to fit and turns by itself.
class PanoramaView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(qreal autoscrollSpeed MEMBER m_speed NOTIFY autoscrollSpeedChanged) // degrees per second
    Q_PROPERTY(Direction autoscrollDirection MEMBER m_direction NOTIFY autoscrollDirectionChanged)
    Q_PROPERTY(int idleTimeout MEMBER m_idleTimeout NOTIFY idleTimeoutChanged) // ms

public:
    enum Direction { Left, Right }; // Right: the view turns right, so the content moves left
    Q_ENUM(Direction)

    explicit PanoramaView(QQuickItem *parent = nullptr);

    QImage image() const { return m_image; }
    void setImage(const QImage &image);

signals:
    void imageChanged();
    void autoscrollSpeedChanged();
    void autoscrollDirectionChanged();
    void idleTimeoutChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;
    void mousePressEvent(QMouseEvent *event) override { pointerEvent(event); }
    void mouseMoveEvent(QMouseEvent *event) override { pointerEvent(event); }
    void mouseReleaseEvent(QMouseEvent *event) override { pointerEvent(event); }
    void touchEvent(QTouchEvent *event) override { pointerEvent(event); }
    void mouseUngrabEvent() override { cancelInput(); }
    void touchUngrabEvent() override { cancelInput(); }
    void wheelEvent(QWheelEvent *event) override;

private:
    // Calls onTick once per frame of Qt Quick's animation clock, with the seconds since the last one.
    class Ticker : public QAbstractAnimation
    {
    public:
        std::function<void(qreal)> onTick;
        int duration() const override { return -1; }

    protected:
        // max(): ms starts over at 0 on start(), and when Qt's int ms clock wraps after 24.8 days
        void updateCurrentTime(int ms) override
        {
            onTick(std::max(0, ms - m_last) / 1000.0);
            m_last = ms;
        }

    private:
        int m_last = 0;
    };

    void pointerEvent(QPointerEvent *event);
    void cancelInput();
    void stopAutoscroll(const char *input);
    void resumeAutoscroll();
    void startTurning();
    void tick(qreal dt);
    void updateTicker();
    void relayout();

    QImage m_image;
    bool m_imageChanged = false; // tells the render thread to split the new image into strips
    qreal m_speed = 3;
    Direction m_direction = Right;
    int m_idleTimeout = 5000;

    PanoramaGeometry m_geo;
    bool m_autoscroll = true; // turning by itself, from launch on
    QPointF m_flick;          // velocity, item px/s
    Ticker m_ticker;
    QTimer m_idle;
    QVariantAnimation m_reset; // 0 → 1, eased: back to fit-height zoom and vertical center
    PanoramaGeometry m_resetFrom;

    // The points that are down: how many, their centroid and mean distance from it.
    int m_points = 0;
    QPointF m_centroid;
    qreal m_spread = 0;
    QPointF m_velocity; // of the centroid, item px/s
    quint64 m_lastMove = 0;
};

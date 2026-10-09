#include "panoramageometry.h"

#include <cmath>

void PanoramaGeometry::clamp()
{
    if (isEmpty())
        return;
    zoom = std::clamp(zoom, minZoom(), maxZoom());
    const qreal half = view.height() / 2 / zoom / image.height(); // half the visible height, as a fraction
    v = half >= 0.5 ? 0.5 : std::clamp(v, half, 1 - half);
    angle = wrapAngle(angle);
}

void PanoramaGeometry::turn(qreal degrees)
{
    angle = wrapAngle(angle + degrees);
}

void PanoramaGeometry::panBy(QPointF delta)
{
    if (isEmpty())
        return;
    angle -= delta.x() / pixelsPerDegree();
    v -= delta.y() / zoom / image.height();
    clamp();
}

void PanoramaGeometry::zoomAt(qreal factor, QPointF pos)
{
    if (isEmpty())
        return;
    const qreal oldZoom = zoom;
    zoom = std::clamp(zoom * factor, minZoom(), maxZoom());
    // Zooming around the view center moves the point under pos away from it; pan it back.
    panBy((QPointF(view.width(), view.height()) / 2 - pos) * (zoom / oldZoom - 1));
}

std::vector<PanoramaGeometry::Piece> PanoramaGeometry::visiblePieces(int stripWidth) const
{
    std::vector<Piece> pieces;
    if (isEmpty() || zoom <= 0 || stripWidth <= 0)
        return pieces;
    const qreal width = image.width();
    const qreal left = angle / 360 * width - view.width() / 2 / zoom; // image x at the item's left edge
    const qreal right = left + view.width() / zoom;
    const qreal top = v * image.height() - view.height() / 2 / zoom;
    // Neighbours compute their shared edge from the same values, rounded to float like the scene
    // graph's vertices, so their vertices are identical: no gap, not even a sub-pixel one.
    const auto itemX = [&](qreal x) { return qreal(float((x - left) * zoom)); };
    for (qreal copy = std::floor(left / width) * width; copy < right; copy += width) {
        for (int i = 0; i * stripWidth < width; ++i) {
            const qreal x0 = copy + i * stripWidth;
            const qreal x1 = copy + std::min<qreal>((i + 1) * stripWidth, width);
            if (x1 > left && x0 < right)
                pieces.push_back({i, QRectF(QPointF(itemX(x0), -top * zoom),
                                            QPointF(itemX(x1), (image.height() - top) * zoom))});
        }
    }
    return pieces;
}

qreal wrapAngle(qreal degrees)
{
    qreal a = std::fmod(degrees, 360.0);
    if (a < 0)
        a += 360;
    return a >= 360 ? 0 : a; // a tiny negative a rounds up to 360
}

QPointF flickStep(QPointF &velocity, qreal dt)
{
    const qreal speed = std::hypot(velocity.x(), velocity.y());
    const qreal newSpeed = speed - flickDeceleration * dt;
    if (newSpeed <= 0) { // it stops within this step, after speed² / 2a
        const QPointF distance = velocity * (speed / (2 * flickDeceleration));
        velocity = {};
        return distance;
    }
    const QPointF distance = velocity * ((speed + newSpeed) / 2 * dt / speed);
    velocity *= newSpeed / speed;
    return distance;
}

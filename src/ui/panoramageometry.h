#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include <algorithm>
#include <vector>

// The math behind PanoramaView, free of GUI types so tests run it without a QGuiApplication.
// Units: angles in degrees, image sizes in source pixels, view positions in item pixels.
// After changing image, view or dpr, call clamp().
struct PanoramaGeometry
{
    QSizeF image;      // the source panorama
    QSizeF view;       // the item
    qreal dpr = 1;     // device pixels per item pixel
    qreal angle = 0;   // direction at the view center, in [0, 360); 0 is the image's left edge
    qreal zoom = 0;    // item pixels per image pixel; clamp() turns 0 into fit-height
    qreal v = 0.5;     // image row at the view center, as a fraction of the image height

    bool isEmpty() const { return image.isEmpty() || view.isEmpty(); }
    qreal minZoom() const { return view.height() / image.height(); } // the image fills the view height
    qreal maxZoom() const { return std::max(minZoom(), 1 / dpr); }   // 1:1 device pixels, min wins
    qreal pixelsPerDegree() const { return image.width() * zoom / 360; }

    void clamp();                           // zoom limits, vertical edges, angle wrap
    void turn(qreal degrees);               // positive turns right: the content moves left
    void panBy(QPointF delta);              // the content follows a drag by delta
    void zoomAt(qreal factor, QPointF pos); // the image point under pos stays put

    // One copy of a texture strip and where it goes in the item. Strips are stripWidth wide
    // (the last one narrower), side by side; copies repeat them every image width.
    struct Piece
    {
        int strip;
        QRectF rect;
    };
    std::vector<Piece> visiblePieces(int stripWidth) const;
};

qreal wrapAngle(qreal degrees); // into [0, 360)

// Flick momentum, with Flickable's defaults.
constexpr qreal flickDeceleration = 1500; // item px/s²
constexpr qreal maxFlickVelocity = 2500;  // item px/s

// Advances a flick by dt seconds: returns the distance moved and slows velocity down,
// to exactly zero when the flick ends.
QPointF flickStep(QPointF &velocity, qreal dt);

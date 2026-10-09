#include "panoramageometry.h"

#include <gtest/gtest.h>

namespace {
// The test panorama in a 1280x800 view: fit-height zoom is 800/900.
PanoramaGeometry geometry(QSizeF image = {6000, 900}, QSizeF view = {1280, 800}, qreal dpr = 1)
{
    PanoramaGeometry g;
    g.image = image;
    g.view = view;
    g.dpr = dpr;
    g.clamp();
    return g;
}

// Unwrapped image coordinates of an item point.
QPointF imagePoint(const PanoramaGeometry &g, QPointF pos)
{
    const QPointF center(g.angle / 360 * g.image.width(), g.v * g.image.height());
    return center + (pos - QPointF(g.view.width(), g.view.height()) / 2) / g.zoom;
}

// The pieces must tile the view without gaps: each one starts exactly where the previous
// one ends, and the strips follow each other around the panorama.
void expectSeamless(const std::vector<PanoramaGeometry::Piece> &pieces, const PanoramaGeometry &g, int strips)
{
    ASSERT_FALSE(pieces.empty());
    EXPECT_LE(pieces.front().rect.left(), 0);
    EXPECT_GE(pieces.back().rect.right(), g.view.width());
    for (size_t i = 1; i < pieces.size(); ++i) {
        EXPECT_EQ(pieces[i].rect.left(), pieces[i - 1].rect.right()) << "gap before piece " << i;
        EXPECT_EQ(pieces[i].strip, (pieces[i - 1].strip + 1) % strips);
        EXPECT_EQ(pieces[i].rect.top(), pieces[0].rect.top());
        EXPECT_EQ(pieces[i].rect.height(), pieces[0].rect.height());
    }
}
} // namespace

TEST(PanoramaGeometry, WrapAngle)
{
    EXPECT_DOUBLE_EQ(wrapAngle(0), 0);
    EXPECT_DOUBLE_EQ(wrapAngle(359.5), 359.5);
    EXPECT_DOUBLE_EQ(wrapAngle(360), 0);
    EXPECT_DOUBLE_EQ(wrapAngle(370), 10);
    EXPECT_DOUBLE_EQ(wrapAngle(-10), 350);
    EXPECT_DOUBLE_EQ(wrapAngle(-730), 350);
    EXPECT_LT(wrapAngle(-1e-20), 360);
}

TEST(PanoramaGeometry, DegreesToPixels)
{
    PanoramaGeometry g = geometry({3600, 800}, {1280, 800});
    EXPECT_DOUBLE_EQ(g.pixelsPerDegree(), 10);
    g.zoom = 0.5;
    EXPECT_DOUBLE_EQ(g.pixelsPerDegree(), 5);
    g = geometry({3600, 800}, {1280, 800});
    g.panBy({-20, 0}); // dragging the content left by 20 px turns right by 2°
    EXPECT_DOUBLE_EQ(g.angle, 2);
    g.panBy({40, 0}); // and back past 0
    EXPECT_DOUBLE_EQ(g.angle, 358);
}

TEST(PanoramaGeometry, ZoomLimits)
{
    PanoramaGeometry g = geometry();
    EXPECT_DOUBLE_EQ(g.minZoom(), 800.0 / 900);
    EXPECT_DOUBLE_EQ(g.maxZoom(), 1);
    EXPECT_DOUBLE_EQ(g.zoom, g.minZoom()); // starts at fit-height

    g = geometry({6000, 900}, {1280, 400}, 2); // 1:1 device pixels is 0.5 item px per image px
    EXPECT_DOUBLE_EQ(g.maxZoom(), 0.5);
    g.zoom = 7;
    g.clamp();
    EXPECT_DOUBLE_EQ(g.zoom, 0.5);

    g = geometry({6000, 900}, {1280, 800}, 2); // fit-height (0.89) is above 1:1 (0.5): the minimum wins
    EXPECT_DOUBLE_EQ(g.maxZoom(), g.minZoom());
    g.zoomAt(2, {100, 100});
    EXPECT_DOUBLE_EQ(g.zoom, g.minZoom());
}

TEST(PanoramaGeometry, ResizeReappliesLimits)
{
    PanoramaGeometry g = geometry({6000, 900}, {1280, 400});
    g.zoomAt(100, {640, 200});
    EXPECT_DOUBLE_EQ(g.zoom, 1);
    g.view = {1280, 1000}; // taller than the image at 1:1: fit-height wins
    g.clamp();
    EXPECT_DOUBLE_EQ(g.zoom, 1000.0 / 900);
    EXPECT_DOUBLE_EQ(g.v, 0.5);
}

TEST(PanoramaGeometry, ZoomKeepsTheFocalPoint)
{
    PanoramaGeometry g = geometry();
    g.angle = 90;
    const QPointF pos(1000, 250);
    const QPointF before = imagePoint(g, pos);
    g.zoomAt(1.1, pos);
    EXPECT_DOUBLE_EQ(g.zoom, 1.1 * 800 / 900);
    EXPECT_NEAR(imagePoint(g, pos).x(), before.x(), 1e-9);
    EXPECT_NEAR(imagePoint(g, pos).y(), before.y(), 1e-9);
    g.zoomAt(100, pos); // clamped at 1:1, the point still stays put
    EXPECT_DOUBLE_EQ(g.zoom, 1);
    EXPECT_NEAR(imagePoint(g, pos).x(), before.x(), 1e-9);
    EXPECT_NEAR(imagePoint(g, pos).y(), before.y(), 1e-9);
}

TEST(PanoramaGeometry, VerticalPanStopsAtTheEdges)
{
    PanoramaGeometry g = geometry();
    g.panBy({0, 300}); // at fit-height it stays centered
    EXPECT_DOUBLE_EQ(g.v, 0.5);

    g.zoomAt(100, {640, 400}); // 1:1: 800 of 900 rows visible
    g.panBy({0, 1000});
    EXPECT_NEAR(g.visiblePieces(6000).front().rect.top(), 0, 1e-9); // the image top at the view top
    g.panBy({0, -30});
    EXPECT_NEAR(g.visiblePieces(6000).front().rect.top(), -30, 1e-9);
    g.panBy({0, -1000});
    EXPECT_NEAR(g.visiblePieces(6000).front().rect.bottom(), 800, 1e-9); // the image bottom at the view bottom
}

TEST(PanoramaGeometry, PiecesWrapAroundTheSeam)
{
    PanoramaGeometry g = geometry({6000, 900}, {1280, 800}, 1);
    g.zoom = 1;
    g.angle = 0; // the image's left edge at the view center: the right edge meets it there
    const auto pieces = g.visiblePieces(6000);
    ASSERT_EQ(pieces.size(), 2u);
    EXPECT_EQ(pieces[0].rect, QRectF(640 - 6000, -50, 6000, 900));
    EXPECT_EQ(pieces[1].rect, QRectF(640, -50, 6000, 900));
    expectSeamless(pieces, g, 1);
}

TEST(PanoramaGeometry, PiecesTileWithStrips)
{
    PanoramaGeometry g = geometry();
    for (qreal angle : {0.0, 0.1, 61.3, 180.0, 359.99}) {
        for (qreal zoom : {800.0 / 900, 0.95, 1.0}) {
            g.angle = angle;
            g.zoom = zoom;
            expectSeamless(g.visiblePieces(1024), g, 6); // the last strip is 880 px wide
        }
    }
    g.angle = 0;
    g.zoom = 1;
    const auto pieces = g.visiblePieces(1024); // image x -640 .. 640
    ASSERT_EQ(pieces.size(), 2u);
    EXPECT_EQ(pieces[0].strip, 5);
    EXPECT_EQ(pieces[0].rect.width(), 880);
    EXPECT_EQ(pieces[1].strip, 0);
    EXPECT_EQ(pieces[1].rect.width(), 1024);
}

TEST(PanoramaGeometry, PiecesRepeatWhenTheImageIsNarrowerThanTheView)
{
    const PanoramaGeometry g = geometry({500, 800}, {1280, 800});
    const auto pieces = g.visiblePieces(300); // image x -640 .. 640: parts of four copies
    EXPECT_EQ(pieces.size(), 6u);
    expectSeamless(pieces, g, 2);
}

TEST(PanoramaGeometry, TurnRightMovesTheContentLeft)
{
    PanoramaGeometry g = geometry();
    g.angle = 100;
    const qreal before = g.visiblePieces(6000).front().rect.left();
    g.turn(1);
    EXPECT_NEAR(g.visiblePieces(6000).front().rect.left(), before - g.pixelsPerDegree(), 1e-3);
}

TEST(PanoramaGeometry, AutoscrollTurnsOnceAMinuteAt6DegreesPerSecond)
{
    PanoramaGeometry g = geometry();
    g.angle = 10;
    for (int frame = 0; frame < 60 * 60; ++frame) // 60 s at 60 fps
        g.turn(6.0 / 60);
    EXPECT_NEAR(g.angle, 10, 1e-9);
    g.turn(-6.0 * 2); // turning left wraps below 0
    EXPECT_NEAR(g.angle, 358, 1e-9);
}

TEST(PanoramaGeometry, FlickDecelerates)
{
    QPointF velocity(900, 1200); // 1500 px/s: stops after 1 s and 750 px
    QPointF moved;
    int steps = 0;
    for (; !velocity.isNull() && steps < 1000; ++steps)
        moved += flickStep(velocity, 1.0 / 60);
    EXPECT_NEAR(steps, 60, 1); // rounding may leave a tiny speed for one more step
    EXPECT_NEAR(moved.x(), 450, 1e-9);
    EXPECT_NEAR(moved.y(), 600, 1e-9);

    velocity = {-300, 0}; // a slow flick stops within one long step
    EXPECT_NEAR(flickStep(velocity, 1).x(), -30, 1e-9);
    EXPECT_TRUE(velocity.isNull());
}

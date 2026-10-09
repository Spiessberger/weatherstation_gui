#include "panoramaview.h"

#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSGImageNode>

#include <cmath>
#include <memory>

Q_STATIC_LOGGING_CATEGORY(lcPanorama, "ws.ui.panorama")

namespace {
// ponytail: the smallest common GPU limit (e.g. Raspberry Pi VideoCore IV), so it fits every
// device; wider strips would need QRhi::TextureSizeMax via Qt::GuiPrivate
constexpr int maxTextureSize = 2048;

// Owns the textures of the current image's strips; the child image nodes only use them.
struct PanoramaNode : QSGNode
{
    std::vector<std::unique_ptr<QSGTexture>> strips;
    int stripWidth = 0;

    void deleteChildren()
    {
        while (QSGNode *child = firstChild())
            delete child;
    }
    ~PanoramaNode() override { deleteChildren(); } // before the textures they use
};
} // namespace

PanoramaView::PanoramaView(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setClip(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);

    m_ticker.onTick = [this](qreal dt) { tick(dt); };
    connect(this, &PanoramaView::autoscrollSpeedChanged, this, &PanoramaView::updateTicker); // 0: no frames
    m_idle.setSingleShot(true);
    m_idle.setTimerType(Qt::PreciseTimer); // a coarse timer may fire 5% early
    connect(&m_idle, &QTimer::timeout, this, &PanoramaView::resumeAutoscroll);
    m_reset.setDuration(1000);
    m_reset.setEasingCurve(QEasingCurve::InOutCubic);
    m_reset.setStartValue(0.0);
    m_reset.setEndValue(1.0);
    connect(&m_reset, &QVariantAnimation::valueChanged, this, [this](const QVariant &progress) {
        if (m_geo.isEmpty())
            return;
        m_geo.zoom = std::lerp(m_resetFrom.zoom, m_geo.minZoom(), progress.toReal());
        m_geo.v = std::lerp(m_resetFrom.v, 0.5, progress.toReal());
        m_geo.clamp();
        update();
    });
    connect(&m_reset, &QAbstractAnimation::finished, this, &PanoramaView::startTurning);
}

void PanoramaView::setImage(const QImage &image)
{
    if (image.cacheKey() == m_image.cacheKey())
        return;
    qCDebug(lcPanorama, "new image %dx%d", image.width(), image.height());
    m_image = image;
    m_imageChanged = true;
    // Another resolution keeps the view: same angle, same zoom relative to fit-height.
    m_geo.setImageSize(image.size());
    m_resetFrom.setImageSize(image.size()); // the start of a running reset, too
    relayout();
    updateTicker();
    emit imageChanged();
}

QSGNode *PanoramaView::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<PanoramaNode *>(oldNode);
    if (m_image.isNull()) {
        delete node;
        return nullptr;
    }
    if (!node)
        node = new PanoramaNode;

    // Split each image once into strips that fit in maxTextureSize. Their 1 px gutters repeat the
    // neighbouring columns, wrapping around the panorama, so linear filtering at the strip edges
    // blends the same pixels as the unsplit image would: no seams.
    if (node->strips.empty() || m_imageChanged) {
        m_imageChanged = false;
        node->deleteChildren();
        node->strips.clear(); // releases the previous image's textures
        const int width = m_image.width();
        const int height = m_image.height(); // ponytail: assumes height <= maxTextureSize; split rows too if not
        node->stripWidth = std::min(width, maxTextureSize - 2);
        for (int x = 0; x < width; x += node->stripWidth) {
            const int w = std::min(node->stripWidth, width - x);
            QImage strip = m_image.copy(x - 1, 0, w + 2, height);
            // ponytail: wrong gutters for indexed images (pixel() gives a colour, setPixel() wants an
            // index); convert those first if an image source ever delivers them
            for (int y = 0; y < height; ++y) {
                strip.setPixel(0, y, m_image.pixel((x - 1 + width) % width, y));
                strip.setPixel(w + 1, y, m_image.pixel((x + w) % width, y));
            }
            // ponytail: no mipmaps, so a view shorter than the image (minified) may shimmer while
            // turning; add QQuickWindow::TextureHasMipmaps if it does
            node->strips.emplace_back(window()->createTextureFromImage(strip));
        }
        qCDebug(lcPanorama, "tiling %dx%d for a %d px texture limit: %d strip(s) of %d px", width, height,
                maxTextureSize, int(node->strips.size()), node->stripWidth);
    }

    // One image node per visible strip copy, reused from frame to frame.
    QSGNode *child = node->firstChild();
    for (const PanoramaGeometry::Piece &piece : m_geo.visiblePieces(node->stripWidth)) {
        auto *imageNode = child ? static_cast<QSGImageNode *>(child) : window()->createImageNode();
        QSGTexture *texture = node->strips[piece.strip].get();
        imageNode->setFiltering(QSGTexture::Linear);
        if (imageNode->texture() != texture)
            imageNode->setTexture(texture);
        imageNode->setRect(piece.rect);
        imageNode->setSourceRect(QRectF(QPointF(1, 0), texture->textureSize() - QSize(2, 0))); // without gutters
        if (!child) // only now: the software renderer reads a node as soon as it is added
            node->appendChildNode(imageNode);
        child = imageNode->nextSibling();
    }
    while (child) { // fewer pieces than last frame
        QSGNode *next = child->nextSibling();
        delete child;
        child = next;
    }
    return node;
}

void PanoramaView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    relayout();
}

void PanoramaView::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged)
        relayout();
}

void PanoramaView::relayout()
{
    m_geo.view = size();
    m_geo.dpr = window() ? window()->effectiveDevicePixelRatio() : 1;
    m_geo.clamp();
    update();
}

void PanoramaView::pointerEvent(QPointerEvent *event)
{
    event->accept();
    if (event->isBeginEvent())
        stopAutoscroll(event->device()->type() == QInputDevice::DeviceType::TouchScreen ? "touch" : "mouse press");

    QList<QPointF> down;
    for (const QEventPoint &point : event->points())
        if (point.state() != QEventPoint::Released)
            down.append(point.position());
    QPointF centroid;
    for (QPointF pos : down)
        centroid += pos / down.size();
    qreal spread = 0;
    for (QPointF pos : down)
        spread += QLineF(pos, centroid).length() / down.size();

    if (down.isEmpty()) { // the last point is up: flick if it was still moving
        if (m_points == 1 && event->timestamp() - m_lastMove < 50) {
            const qreal speed = std::hypot(m_velocity.x(), m_velocity.y());
            m_flick = speed > maxFlickVelocity ? m_velocity * (maxFlickVelocity / speed) : m_velocity;
            updateTicker();
        }
        m_points = 0;
        m_idle.start(m_idleTimeout);
        return;
    }
    if (down.size() == m_points) { // moved: pan with the centroid, pinch with the spread
        const QPointF delta = centroid - m_centroid;
        m_geo.panBy(delta);
        if (m_points > 1 && m_spread > 0)
            m_geo.zoomAt(spread / m_spread, centroid);
        const qreal dt = (event->timestamp() - m_lastMove) / 1000.0;
        if (dt > 0)
            m_velocity = 0.3 * m_velocity + 0.7 * delta / dt; // smoothed, like QEventPoint::velocity()
        update();
    } else { // a point went down or up: continue from here
        m_velocity = {};
    }
    m_lastMove = event->timestamp();
    m_points = down.size();
    m_centroid = centroid;
    m_spread = spread;
}

void PanoramaView::cancelInput()
{
    if (!m_points) // already released
        return;
    m_points = 0;
    m_idle.start(m_idleTimeout);
}

void PanoramaView::wheelEvent(QWheelEvent *event)
{
    stopAutoscroll("wheel");
    m_geo.zoomAt(std::pow(2.0, event->angleDelta().y() / 480.0), event->position()); // a notch: 2^(1/4)
    update();
    if (!m_points)
        m_idle.start(m_idleTimeout);
}

void PanoramaView::stopAutoscroll(const char *input)
{
    if (m_autoscroll || m_reset.state() == QAbstractAnimation::Running)
        qCDebug(lcPanorama, "autoscroll stopped by %s at %.1f°", input, m_geo.angle);
    m_autoscroll = false;
    m_reset.stop();
    m_flick = {};
    m_idle.stop();
    updateTicker();
}

void PanoramaView::resumeAutoscroll()
{
    m_flick = {}; // a flick still running would fight the reset
    updateTicker();
    if (m_geo.isEmpty() || (qFuzzyCompare(m_geo.zoom, m_geo.minZoom()) && qFuzzyCompare(m_geo.v, 0.5))) {
        startTurning();
        return;
    }
    qCDebug(lcPanorama, "resetting view at %.1f°: zoom %.3f -> %.3f, vertical %.2f -> 0.50", m_geo.angle,
            m_geo.zoom, m_geo.minZoom(), m_geo.v);
    m_resetFrom = m_geo;
    m_reset.start();
}

void PanoramaView::startTurning()
{
    qCDebug(lcPanorama, "autoscroll resumed at %.1f°", m_geo.angle);
    m_autoscroll = true;
    updateTicker();
}

void PanoramaView::tick(qreal dt)
{
    if (!m_flick.isNull())
        m_geo.panBy(flickStep(m_flick, dt));
    if (m_autoscroll)
        m_geo.turn((m_direction == Right ? m_speed : -m_speed) * dt);
    update();
    updateTicker();
}

void PanoramaView::updateTicker()
{
    const bool run = (m_autoscroll && m_speed != 0 && !m_image.isNull()) || !m_flick.isNull();
    if (run != (m_ticker.state() == QAbstractAnimation::Running))
        run ? m_ticker.start() : m_ticker.stop();
}

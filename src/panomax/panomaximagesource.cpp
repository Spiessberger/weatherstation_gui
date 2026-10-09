#include "panomaximagesource.h"

#include <QLoggingCategory>

#include <algorithm>
#include <numeric>

using namespace Qt::StringLiterals;

Q_STATIC_LOGGING_CATEGORY(lcImage, "ws.panomax.image")

namespace Panomax {

ImageSource::ImageSource(QObject *parent)
    : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ImageSource::check);
    connect(this, &ImageSource::cameraIdChanged, this, &ImageSource::restart);
    connect(this, &ImageSource::sizeChanged, this, &ImageSource::restart);
    connect(this, &ImageSource::timestampChanged, this, &ImageSource::restart);
    restart();
}

void ImageSource::restart()
{
    // Camera-local wall-clock time, parsed field by field so that no time zone gets involved.
    m_date = QDate::fromString(m_timestamp.section(u'T', 0, 0), Qt::ISODate);
    m_time = QTime::fromString(m_timestamp.section(u'T', 1), Qt::ISODate);
    if (!m_date.isValid() || !m_time.isValid()) {
        if (!m_timestamp.isEmpty())
            qCWarning(lcImage).noquote() << "invalid timestamp" << m_timestamp << "- loading the latest image";
        m_date = {};
        m_time = {};
    }
    // ponytail: a superseded check's requests run to their end, only their results are dropped;
    // abort them if inputs start to change often (the Panomax browser).
    ++m_generation;
    m_shown.clear(); // load even if the new camera or size has an image of the same time
    m_timer.start(0); // one check after several properties changed in a row
}

void ImageSource::check()
{
    if (m_cameraId <= 0)
        return;
    // ponytail: no If-None-Match, so each check fetches the whole day (~0.5 KB gzipped); add it if checks get frequent.
    const QUrl url = dayUrl(m_cameraId, m_date);
    m_client.get(url, this, [this, gen = m_generation, url](int status, const QByteArray &body) {
        if (gen != m_generation)
            return;
        if (status == 0)
            return fail(u"no response from "_s + url.toString());
        QString error;
        const std::optional<Day> day = parseDay(body, &error);
        if (!day)
            return fail(status == 200 ? error : u"HTTP %1: %2"_s.arg(status).arg(error));

        const QTime time = chooseImage(*day, m_time);
        if (!time.isValid())
            return fail(u"no available image on "_s + day->date.toString(Qt::ISODate)
                        + (m_time.isValid() ? u" at or before "_s + m_time.toString(Qt::ISODate) : QString()));
        const QString shot = day->date.toString(Qt::ISODate) + u' ' + time.toString(Qt::ISODate);
        if (shot == m_shown) {
            qCDebug(lcImage).noquote() << "no change, newest image is still" << shot;
            return scheduleCheck();
        }
        qCInfo(lcImage).noquote() << (m_time.isValid() ? u"image at or before %1:"_s.arg(m_timestamp)
                                                        : u"newest image:"_s)
                                  << shot;

        const std::optional<SizeVariant> size = chooseSize(*day, m_size);
        if (!size) {
            QStringList names;
            for (const SizeVariant &s : day->sizes)
                names.append(s.name);
            return fail(u"no size \"%1\" on %2, it has: %3"_s.arg(m_size, day->date.toString(Qt::ISODate),
                                                                   names.join(u", ")));
        }
        qCInfo(lcImage, "size %s: %dx%d tiles of %dx%d px, %dx%d px", qUtf8Printable(size->name), size->cols,
               size->rows, size->tileSize.width(), size->tileSize.height(), size->size().width(),
               size->size().height());
        download(imageUrls(m_cameraId, *day, time, *size), *size, shot);
    });
}

void ImageSource::download(const QList<QUrl> &urls, const SizeVariant &size, const QString &shot)
{
    m_files = QList<QByteArray>(urls.size());
    m_pending = urls.size();
    m_downloadTimer.start();
    for (qsizetype i = 0; i < urls.size(); ++i) {
        m_client.get(urls[i], this, [this, gen = m_generation, i, url = urls[i], size, shot](int status, const QByteArray &body) {
            if (gen != m_generation)
                return;
            if (status != 200)
                return fail((status ? u"HTTP %1 for "_s.arg(status) : u"no response from "_s) + url.toString());
            m_files[i] = body;
            if (--m_pending == 0)
                decode(size, shot);
        });
    }
}

void ImageSource::decode(const SizeVariant &size, const QString &shot)
{
    const qint64 downloadMs = m_downloadTimer.elapsed();
    const qint64 bytes = std::accumulate(m_files.cbegin(), m_files.cend(), qint64(0),
                                         [](qint64 sum, const QByteArray &file) { return sum + file.size(); });
    // Decoding a panorama takes long enough to stall the UI.
    m_pool.start([this, gen = m_generation, files = std::exchange(m_files, {}), size, shot, downloadMs, bytes] {
        QElapsedTimer decodeTimer;
        decodeTimer.start();
        const QImage image = stitchTiles(files, size);
        QMetaObject::invokeMethod(this, [this, gen, image, shot, downloadMs, bytes, decodeMs = decodeTimer.elapsed()] {
            if (gen != m_generation)
                return;
            if (image.isNull())
                return fail(u"cannot decode %1 (%2 bytes)"_s.arg(shot).arg(bytes));
            m_image = image;
            m_shown = shot;
            qCInfo(lcImage, "loaded %s: %dx%d px, %lld bytes, download %lld ms, decode %lld ms", qUtf8Printable(shot),
                   image.width(), image.height(), bytes, downloadMs, decodeMs);
            emit imageChanged();
            if (m_date.isValid())
                qCDebug(lcImage) << "fixed timestamp: no further checks";
            else
                scheduleCheck();
        });
    });
}

void ImageSource::fail(const QString &error)
{
    ++m_generation; // ignore the rest of this check
    qCWarning(lcImage).noquote() << "check failed:" << error;
    scheduleCheck();
}

void ImageSource::scheduleCheck()
{
    const int interval = std::max(m_pollInterval, 1000); // a bad setting must not hammer the API
    qCDebug(lcImage) << "next check in" << interval / 1000.0 << "s";
    m_timer.start(interval);
}

} // namespace Panomax

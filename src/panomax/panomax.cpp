#include "panomax.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <algorithm>
#include <limits>

using namespace Qt::StringLiterals;

namespace Panomax {

std::optional<Day> parseDay(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonObject object = QJsonDocument::fromJson(json, &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        *error = u"invalid JSON: "_s + parseError.errorString();
        return {};
    }
    if (object.contains(u"status")) { // the API's error response: {"title","status","detail"}
        *error = object[u"title"].toString() + u": "_s + object[u"detail"].toString();
        return {};
    }

    Day day{QDate::fromString(object[u"date"].toString(), Qt::ISODate), object[u"storage"].toInt(-1), {}, {}};
    if (!day.date.isValid() || day.storage < 0) {
        *error = u"no valid date or storage"_s;
        return {};
    }
    for (const QJsonValue &value : object[u"sizes"].toArray()) {
        const SizeVariant size{value[u"resolution"].toString(), value[u"cols"].toInt(), value[u"rows"].toInt(),
                               {value[u"tileWidth"].toInt(), value[u"tileHeight"].toInt()}};
        // The name goes into URLs, the size into an image allocation. A malformed entry is dropped.
        constexpr qint64 maxSide = std::numeric_limits<int>::max();
        const bool valid = !size.name.isEmpty()
            && std::ranges::all_of(size.name, [](QChar c) { return c.isLetterOrNumber(); })
            && size.cols > 0 && size.rows > 0 && size.tileSize.width() > 0 && size.tileSize.height() > 0
            && qint64(size.cols) * size.tileSize.width() <= maxSide
            && qint64(size.rows) * size.tileSize.height() <= maxSide;
        if (valid)
            day.sizes.append(size);
    }
    for (const QJsonValue &value : object[u"images"].toArray()) {
        const ImageTime image{QTime::fromString(value[u"time"].toString(), Qt::ISODate),
                              value[u"available"].toBool()};
        if (image.time.isValid()) // a malformed entry is dropped
            day.images.append(image);
    }
    return day;
}

QTime chooseImage(const Day &day, QTime notAfter)
{
    QTime newest;
    for (const ImageTime &image : day.images) {
        if (image.available && (!notAfter.isValid() || image.time <= notAfter)
            && (!newest.isValid() || image.time > newest))
            newest = image.time;
    }
    return newest;
}

std::optional<SizeVariant> chooseSize(const Day &day, const QString &name)
{
    if (name != u"auto") {
        for (const SizeVariant &size : day.sizes) {
            if (size.name == name)
                return size;
        }
        return {};
    }
    // The smallest variant that still fills the 800 px high screen; of equal heights the narrower one.
    const auto key = [](const SizeVariant &size) { return std::pair(size.size().height(), size.size().width()); };
    std::optional<SizeVariant> best;
    for (const SizeVariant &size : day.sizes) {
        if (size.size().height() >= 800 && (!best || key(size) < key(*best)))
            best = size;
    }
    return best;
}

QUrl dayUrl(int camera, QDate date)
{
    QString url = u"https://api.panomax.com/1.0/cams/%1/images/day"_s.arg(camera);
    if (date.isValid())
        url += u'/' + date.toString(Qt::ISODate);
    return QUrl(url);
}

QList<QUrl> imageUrls(int camera, const Day &day, QTime time, const SizeVariant &size)
{
    const QString host = day.storage == 0 ? u"panodata"_s : u"panodata%1"_s.arg(day.storage);
    const QString base = u"https://%1.panomax.com/cams/%2/"_s.arg(host).arg(camera)
        + day.date.toString(u"yyyy/MM/dd/") + time.toString(u"HH-mm-ss_") + size.name;
    if (!size.tiled())
        return {QUrl(base + u".jpg")};
    // Tile suffix _{col}_{row}. The extra thin tile at index cols (and rows) is left out.
    QList<QUrl> urls;
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col)
            urls.append(QUrl(base + u"_%1_%2.jpg"_s.arg(col).arg(row)));
    }
    return urls;
}

QImage stitchTiles(const QList<QByteArray> &files, const SizeVariant &size)
{
    if (!size.tiled())
        return QImage::fromData(files.value(0));
    QImage image(size.size(), QImage::Format_RGB32);
    if (image.isNull()) // too big to allocate
        return {};
    QPainter painter(&image);
    for (qsizetype i = 0; i < files.size(); ++i) {
        const QImage tile = QImage::fromData(files[i]);
        if (tile.isNull())
            return {};
        painter.drawImage(int(i % size.cols) * size.tileSize.width(), int(i / size.cols) * size.tileSize.height(),
                          tile);
    }
    painter.end();
    return image;
}

} // namespace Panomax

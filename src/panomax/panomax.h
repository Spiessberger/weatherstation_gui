#pragma once

#include <QByteArray>
#include <QDate>
#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QTime>
#include <QUrl>

#include <optional>

// Panomax webcam data and the pure functions around it; see docs/panomax-api.md.
// A camera is just its int id. Dates and times are camera-local wall-clock time:
// they are used as they are, never converted through UTC or the device's time zone.
namespace Panomax {

// One entry of a day's "sizes": a file, or a grid of tile files, per image.
struct SizeVariant
{
    QString name; // "resolution" in the API: "default", "hd", "full", ...
    int cols = 0;
    int rows = 0;
    QSize tileSize;

    QSize size() const { return {cols * tileSize.width(), rows * tileSize.height()}; }
    bool tiled() const { return cols > 1 || rows > 1; }
};

struct ImageTime
{
    QTime time;
    bool available = false;
};

// An images/day response.
struct Day
{
    QDate date;
    int storage = 0; // image host number: panodata{storage}.panomax.com
    QList<SizeVariant> sizes;
    QList<ImageTime> images;
};

// Parses an images/day response. On failure returns nothing and sets *error, also to
// the message of the API's error response (HTTP 422 for a day without images).
// Malformed sizes and images entries are dropped, so one of them cannot block the day.
std::optional<Day> parseDay(const QByteArray &json, QString *error);

// The newest available image of the day, at or before notAfter if that is valid.
// Invalid if there is none.
QTime chooseImage(const Day &day, QTime notAfter = {});

// The variant called name, or for "auto" the smallest one at least 800 px high.
// Nothing if there is no such variant.
std::optional<SizeVariant> chooseSize(const Day &day, const QString &name);

// images/day of the newest day that has images, or of the given date.
QUrl dayUrl(int camera, QDate date = {});

// The files of one image: one URL, or one per tile, row by row.
QList<QUrl> imageUrls(int camera, const Day &day, QTime time, const SizeVariant &size);

// Decodes the files from imageUrls() into one image. Null if a file does not decode
// or the image is too big to allocate.
QImage stitchTiles(const QList<QByteArray> &files, const SizeVariant &size);

} // namespace Panomax

#include "panomax.h"

#include <QBuffer>
#include <QFile>
#include <gtest/gtest.h>

using namespace Panomax;
using namespace Qt::StringLiterals;

// Readable QString values in failure messages.
void PrintTo(const QString &s, std::ostream *os) { *os << '"' << s.toStdString() << '"'; }

namespace {
Day fixture(const char *name)
{
    QFile file(QStringLiteral(FIXTURES_DIR "/") + QLatin1StringView(name));
    if (!file.open(QIODevice::ReadOnly))
        ADD_FAILURE() << "cannot open " << file.fileName().toStdString();
    QString error;
    const std::optional<Day> day = parseDay(file.readAll(), &error);
    if (!day)
        ADD_FAILURE() << name << ": " << error.toStdString();
    return day.value_or(Day{});
}

QStringList urlStrings(const QList<QUrl> &urls)
{
    QStringList strings;
    for (const QUrl &url : urls)
        strings.append(url.toString());
    return strings;
}

QByteArray png(QSize size, QColor color)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    QByteArray data;
    QBuffer buffer(&data);
    image.save(&buffer, "PNG");
    return data;
}
} // namespace

TEST(PanomaxParse, Cam9015Latest)
{
    const Day day = fixture("cam9015-images-day-latest.json");
    EXPECT_EQ(day.date, QDate(2026, 10, 9));
    EXPECT_EQ(day.storage, 42);
    ASSERT_EQ(day.sizes.size(), 9);
    const SizeVariant hd = day.sizes[4];
    EXPECT_EQ(hd.name, u"hd"_s);
    EXPECT_EQ(hd.cols, 4);
    EXPECT_EQ(hd.rows, 1);
    EXPECT_EQ(hd.tileSize, QSize(1257, 1080));
    EXPECT_EQ(hd.size(), QSize(5028, 1080));
    EXPECT_TRUE(hd.tiled());
    EXPECT_FALSE(day.sizes[3].tiled()); // default
    ASSERT_EQ(day.images.size(), 113);
    EXPECT_EQ(day.images.first().time, QTime(0, 0));
    EXPECT_EQ(day.images.last().time, QTime(18, 40));
    EXPECT_TRUE(day.images.last().available);
}

TEST(PanomaxParse, OtherCameras)
{
    const Day turku = fixture("cam10035-images-day-latest.json");
    EXPECT_EQ(turku.images.size(), 103);
    ASSERT_EQ(turku.sizes.size(), 9);
    EXPECT_EQ(turku.sizes[6].name, u"full"_s);
    EXPECT_EQ(turku.sizes[6].size(), QSize(29424, 4636)); // 24x4 tiles

    const Day canmore = fixture("cam4480-images-day-latest.json");
    EXPECT_EQ(canmore.images.size(), 20);
    EXPECT_EQ(canmore.images.last().time, QTime(8, 0));
}

TEST(PanomaxParse, DisconnectedCameraStillReturnsItsLastDay)
{
    const Day day = fixture("cam1583-images-day-latest-disconnected.json");
    EXPECT_EQ(day.date, QDate(2025, 1, 31));
    EXPECT_EQ(day.storage, 15);
    ASSERT_EQ(day.images.size(), 1);
    EXPECT_EQ(day.images.first().time, QTime(9, 16, 2));
}

TEST(PanomaxParse, ErrorResponseGivesItsMessage)
{
    QFile file(QStringLiteral(FIXTURES_DIR "/cam9015-images-day-2026-10-10-unavailable.json"));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QString error;
    EXPECT_FALSE(parseDay(file.readAll(), &error));
    EXPECT_EQ(error, u"Date not found: The requested date is not available."_s);
}

TEST(PanomaxParse, RejectsBrokenData)
{
    const std::pair<QByteArray, QString> cases[] = {
        {"<html>404</html>", u"invalid JSON"_s},
        {R"({"storage":40})", u"no valid date"_s},
        {R"({"date":"2026-10-09","storage":-1})", u"no valid date"_s},
        {R"({"date":"2026-10-09","storage":40,"sizes":[{"resolution":"hd","cols":0,"rows":1,"tileWidth":1,"tileHeight":1}]})",
         u"invalid size entry"_s},
        {R"({"date":"2026-10-09","storage":40,"sizes":[{"resolution":"../x","cols":1,"rows":1,"tileWidth":1,"tileHeight":1}]})",
         u"invalid size entry"_s},
        {R"({"date":"2026-10-09","storage":40,"sizes":[{"resolution":"x","cols":2,"rows":1,"tileWidth":2000000000,"tileHeight":1}]})",
         u"invalid size entry"_s},
        {R"({"date":"2026-10-09","storage":40,"images":[{"time":"25:00:00","available":true}]})",
         u"invalid image time"_s},
    };
    for (const auto &[json, expected] : cases) {
        QString error;
        EXPECT_FALSE(parseDay(json, &error)) << json.constData();
        EXPECT_TRUE(error.startsWith(expected)) << json.constData() << " -> " << error.toStdString();
    }
}

TEST(PanomaxChooseImage, NewestAvailable)
{
    const Day day = fixture("cam9015-images-day-latest.json");
    EXPECT_EQ(chooseImage(day), QTime(18, 40));
    EXPECT_EQ(chooseImage(day, QTime(12, 5)), QTime(12, 0));
    EXPECT_EQ(chooseImage(day, QTime(12, 0)), QTime(12, 0));
    EXPECT_FALSE(chooseImage(fixture("cam9015-images-day-2026-10-08.json"), QTime(13, 49, 59)).isValid());
}

TEST(PanomaxChooseImage, SkipsUnavailable)
{
    Day day;
    day.images = {{QTime(10, 0), true}, {QTime(10, 10), true}, {QTime(10, 20), false}};
    EXPECT_EQ(chooseImage(day), QTime(10, 10));
    day.images[0].available = day.images[1].available = false;
    EXPECT_FALSE(chooseImage(day).isValid());
}

TEST(PanomaxChooseSize, AutoTakesSmallestAtLeast800High)
{
    for (const char *name : {"cam9015-images-day-latest.json", "cam10035-images-day-latest.json",
                             "cam4480-images-day-latest.json", "cam1583-images-day-latest-disconnected.json"}) {
        const std::optional<SizeVariant> size = chooseSize(fixture(name), u"auto"_s);
        ASSERT_TRUE(size) << name;
        EXPECT_EQ(size->name, u"default"_s) << name;
    }
    Day day;
    day.sizes = {{u"wide"_s, 1, 1, {9000, 800}}, {u"tall"_s, 1, 1, {100, 1080}}, {u"narrow"_s, 2, 1, {400, 800}},
                 {u"low"_s, 1, 1, {100, 799}}};
    EXPECT_EQ(chooseSize(day, u"auto"_s)->name, u"narrow"_s);
    day.sizes = {{u"low"_s, 1, 1, {100, 799}}};
    EXPECT_FALSE(chooseSize(day, u"auto"_s));
}

TEST(PanomaxChooseSize, ByName)
{
    const Day day = fixture("cam9015-images-day-latest.json");
    const std::optional<SizeVariant> full = chooseSize(day, u"full"_s);
    ASSERT_TRUE(full);
    EXPECT_EQ(full->size(), QSize(17505, 3760));
    EXPECT_FALSE(chooseSize(day, u"h5400"_s));
}

TEST(PanomaxUrls, Day)
{
    EXPECT_EQ(dayUrl(9015).toString(), u"https://api.panomax.com/1.0/cams/9015/images/day"_s);
    EXPECT_EQ(dayUrl(9015, QDate(2026, 10, 8)).toString(),
              u"https://api.panomax.com/1.0/cams/9015/images/day/2026-10-08"_s);
}

TEST(PanomaxUrls, Image)
{
    const Day day = fixture("cam9015-images-day-latest.json");
    EXPECT_EQ(urlStrings(imageUrls(9015, day, QTime(18, 40), *chooseSize(day, u"default"_s))),
              QStringList{u"https://panodata42.panomax.com/cams/9015/2026/10/09/18-40-00_default.jpg"_s});
    EXPECT_EQ(urlStrings(imageUrls(9015, day, QTime(7, 0, 5), *chooseSize(day, u"hd"_s))),
              (QStringList{u"https://panodata42.panomax.com/cams/9015/2026/10/09/07-00-05_hd_0_0.jpg"_s,
                           u"https://panodata42.panomax.com/cams/9015/2026/10/09/07-00-05_hd_1_0.jpg"_s,
                           u"https://panodata42.panomax.com/cams/9015/2026/10/09/07-00-05_hd_2_0.jpg"_s,
                           u"https://panodata42.panomax.com/cams/9015/2026/10/09/07-00-05_hd_3_0.jpg"_s}));
}

TEST(PanomaxUrls, StorageZeroAndTileRows)
{
    Day day = fixture("cam4480-images-day-latest.json");
    day.storage = 0;
    const QStringList urls = urlStrings(imageUrls(4480, day, QTime(8, 0), *chooseSize(day, u"full"_s)));
    ASSERT_EQ(urls.size(), 15 * 3);
    EXPECT_EQ(urls[1], u"https://panodata.panomax.com/cams/4480/2026/10/09/08-00-00_full_1_0.jpg"_s);
    EXPECT_EQ(urls[15], u"https://panodata.panomax.com/cams/4480/2026/10/09/08-00-00_full_0_1.jpg"_s);
    EXPECT_EQ(urls.last(), u"https://panodata.panomax.com/cams/4480/2026/10/09/08-00-00_full_14_2.jpg"_s);
}

TEST(PanomaxStitch, PlacesTilesRowByRow)
{
    const SizeVariant size{u"t"_s, 3, 2, {4, 3}};
    const QList<QColor> colors{Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow};
    QList<QByteArray> files;
    for (const QColor &color : colors)
        files.append(png(size.tileSize, color));
    const QImage image = stitchTiles(files, size);
    ASSERT_EQ(image.size(), QSize(12, 6));
    for (int i = 0; i < 6; ++i)
        EXPECT_EQ(image.pixelColor(i % 3 * 4 + 2, i / 3 * 3 + 1), colors[i]) << "tile " << i;
}

TEST(PanomaxStitch, SingleFileAndErrors)
{
    EXPECT_EQ(stitchTiles({png({14, 8}, Qt::red)}, {u"default"_s, 1, 1, {10, 8}}).size(), QSize(14, 8));
    EXPECT_TRUE(stitchTiles({"not an image"}, {u"default"_s, 1, 1, {10, 8}}).isNull());
    EXPECT_TRUE(stitchTiles({png({4, 3}, Qt::red), "broken"}, {u"t"_s, 2, 1, {4, 3}}).isNull());
}

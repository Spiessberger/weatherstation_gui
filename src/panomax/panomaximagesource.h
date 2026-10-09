#pragma once

#include "panomax.h"
#include "panomaxclient.h"

#include <QElapsedTimer>
#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace Panomax {

// The complete panorama of one camera, for PanoramaView { image: source.image }.
// Latest mode checks images/day every pollInterval and loads each new image; with a
// timestamp it loads that one image. On an error it keeps the last good image, logs
// the error and retries after pollInterval. Memory only.
class ImageSource : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PanomaxImageSource)
    Q_PROPERTY(int cameraId MEMBER m_cameraId NOTIFY cameraIdChanged FINAL)
    // A size variant name of the camera, or "auto": the smallest one at least 800 px high.
    Q_PROPERTY(QString size MEMBER m_size NOTIFY sizeChanged FINAL)
    // Camera-local "yyyy-MM-ddTHH:mm:ss": loads the newest image at or before it, once.
    // Empty or invalid: latest mode.
    Q_PROPERTY(QString timestamp MEMBER m_timestamp NOTIFY timestampChanged FINAL)
    // In ms, used from the next check on.
    Q_PROPERTY(int pollInterval MEMBER m_pollInterval NOTIFY pollIntervalChanged FINAL)
    Q_PROPERTY(QImage image READ image NOTIFY imageChanged FINAL)
    // False until the first image is there.
    Q_PROPERTY(bool ready READ ready NOTIFY imageChanged FINAL)

public:
    explicit ImageSource(QObject *parent = nullptr);

    QImage image() const { return m_image; }
    bool ready() const { return !m_image.isNull(); }

signals:
    void cameraIdChanged();
    void sizeChanged();
    void timestampChanged();
    void pollIntervalChanged();
    void imageChanged();

private:
    void restart();
    void check();
    void download(const QList<QUrl> &urls, const SizeVariant &size, const QString &shot);
    void decode(const SizeVariant &size, const QString &shot);
    void fail(const QString &error);
    void scheduleCheck();

    int m_cameraId = 0;
    QString m_size = QStringLiteral("auto");
    QString m_timestamp;
    int m_pollInterval = 60000;
    QImage m_image;

    QDate m_date; // of m_timestamp; both invalid in latest mode
    QTime m_time;
    QString m_shown; // camera-local "date time" of m_image
    int m_generation = 0; // bumped to drop the results of a running check
    QList<QByteArray> m_files; // being downloaded
    qsizetype m_pending = 0;
    QElapsedTimer m_downloadTimer;
    QTimer m_timer;
    Client m_client;
    // Destroyed before QObject: waits for a running decode, which posts its result to this.
    QThreadPool m_pool;
};

} // namespace Panomax

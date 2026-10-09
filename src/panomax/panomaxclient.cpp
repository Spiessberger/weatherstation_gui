#include "panomaxclient.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QNetworkReply>

using namespace std::chrono_literals;

Q_STATIC_LOGGING_CATEGORY(lcApi, "ws.panomax.api")

namespace Panomax {

Client::Client()
{
    // A hung request fails after 30 s without data, so it cannot stall polling.
    m_nam.setTransferTimeout(30s);
}

void Client::get(const QUrl &url, const QObject *context,
                 std::function<void(int status, const QByteArray &body)> done)
{
    qCDebug(lcApi).noquote() << "GET" << url.toString();
    QElapsedTimer timer;
    timer.start();
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, url, timer, done = std::move(done)] {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status < 400)
            status = 0; // the transfer broke off, maybe after the headers: no complete response
        const QByteArray body = status ? reply->readAll() : QByteArray();
        if (status == 0)
            qCWarning(lcApi).noquote() << url.toString() << "failed after" << timer.elapsed() << "ms:"
                                       << reply->errorString();
        else if (status >= 400)
            qCWarning(lcApi, "%s: HTTP %d, %lld bytes, %lld ms", qUtf8Printable(url.toString()), status,
                      qint64(body.size()), timer.elapsed());
        else
            qCDebug(lcApi, "%s: HTTP %d, %lld bytes, %lld ms", qUtf8Printable(url.toString()), status,
                    qint64(body.size()), timer.elapsed());
        done(status, body);
    });
}

} // namespace Panomax

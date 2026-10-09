#pragma once

#include <QNetworkAccessManager>

#include <functional>

namespace Panomax {

// HTTP GET for the API and the image files. Memory only: QNetworkAccessManager has no
// cache unless one is set. Logs every request, response and error to ws.panomax.api.
class Client
{
public:
    Client();

    // Calls done(status, body) once, in context's thread, unless context is gone by then.
    // status is the HTTP status, or 0 when no complete response came (also after 30 s
    // without data); the client has logged why.
    void get(const QUrl &url, const QObject *context,
             std::function<void(int status, const QByteArray &body)> done);

private:
    QNetworkAccessManager m_nam;
};

} // namespace Panomax

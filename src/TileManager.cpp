#include "TileManager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QUrl>

TileManager::TileManager(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_urlTemplate(QStringLiteral("https://tile.openstreetmap.org/%1/%2/%3.png"))
{
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                 + QStringLiteral("/maptiles");
    QDir().mkpath(m_cacheDir);

    connect(m_net, &QNetworkAccessManager::finished,
            this, &TileManager::onReplyFinished);
}

void TileManager::setUrlTemplate(const QString& tmpl) { m_urlTemplate = tmpl; }

void TileManager::setNetworkEnabled(bool enabled) { m_networkEnabled = enabled; }

void TileManager::setTileDirectory(const QString& dir) {
    if (dir.isEmpty() || dir == m_cacheDir)
        return;
    m_cacheDir = dir;
    QDir().mkpath(m_cacheDir);
    // Drop in-memory tiles so the new directory is read on the next request.
    m_memCache.clear();
}

QString TileManager::key(int z, int x, int y) {
    return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y);
}

QString TileManager::diskPath(int z, int x, int y) const {
    return QStringLiteral("%1/%2/%3/%4.png").arg(m_cacheDir).arg(z).arg(x).arg(y);
}

QPixmap TileManager::tile(int z, int x, int y) {
    const QString k = key(z, x, y);

    auto it = m_memCache.constFind(k);
    if (it != m_memCache.constEnd())
        return it.value();

    if (loadFromDisk(z, x, y))
        return m_memCache.value(k);

    if (m_networkEnabled && !m_inFlight.contains(k))
        startDownload(z, x, y);

    return placeholderTile(z, x, y);
}

bool TileManager::loadFromDisk(int z, int x, int y) {
    const QString path = diskPath(z, x, y);
    if (!QFile::exists(path))
        return false;
    QPixmap pm;
    if (!pm.load(path) || pm.isNull())
        return false;
    m_memCache.insert(key(z, x, y), pm);
    return true;
}

void TileManager::startDownload(int z, int x, int y) {
    const QString k = key(z, x, y);
    m_inFlight.insert(k);

    const QString urlStr = m_urlTemplate.arg(z).arg(x).arg(y);
    QNetworkRequest req{QUrl(urlStr)};
    // OSM tile usage policy requires a descriptive, identifying User-Agent.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QByteArrayLiteral("MapPointsVisualizer/1.0 (Qt; offline-capable)"));
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    // Stash z/x/y on the reply so the finished handler can identify the tile.
    QNetworkReply* reply = m_net->get(req);
    reply->setProperty("tz", z);
    reply->setProperty("tx", x);
    reply->setProperty("ty", y);
}

void TileManager::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    const int z = reply->property("tz").toInt();
    const int x = reply->property("tx").toInt();
    const int y = reply->property("ty").toInt();
    const QString k = key(z, x, y);
    m_inFlight.remove(k);

    if (reply->error() != QNetworkReply::NoError)
        return; // keep showing the placeholder; will retry on next request

    const QByteArray data = reply->readAll();
    QPixmap pm;
    if (!pm.loadFromData(data) || pm.isNull())
        return;

    m_memCache.insert(k, pm);

    // Persist to disk cache.
    const QString path = diskPath(z, x, y);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(data);

    emit tileReady(z, x, y);
}

QPixmap TileManager::placeholderTile(int z, int x, int y) {
    // A single shared appearance keyed only by parity, so we draw it once.
    const QString k = QStringLiteral("ph-%1").arg((x + y) & 1);
    auto it = m_placeholders.constFind(k);
    if (it != m_placeholders.constEnd())
        return it.value();

    const int s = tileSize();
    QPixmap pm(s, s);
    const bool alt = (x + y) & 1;
    pm.fill(alt ? QColor(0xE9, 0xEE, 0xF2) : QColor(0xF2, 0xF5, 0xF8));

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen grid(QColor(0xCF, 0xD8, 0xDC));
    grid.setWidth(1);
    p.setPen(grid);
    for (int i = 0; i <= s; i += 32) {
        p.drawLine(i, 0, i, s);
        p.drawLine(0, i, s, i);
    }
    p.end();

    m_placeholders.insert(k, pm);
    return pm;
}

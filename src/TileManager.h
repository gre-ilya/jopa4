#ifndef TILEMANAGER_H
#define TILEMANAGER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QPixmap>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Fetches 256x256 slippy-map raster tiles (default source: OpenStreetMap),
// caching them both on disk and in memory. Downloads happen asynchronously;
// tileReady() is emitted when a requested tile becomes available so the view
// can repaint. When a tile cannot be obtained (e.g. no network) a generated
// placeholder grid tile is returned so the application still works offline.
class TileManager : public QObject {
    Q_OBJECT
public:
    explicit TileManager(QObject* parent = nullptr);

    // Returns the tile pixmap if it is already in memory; otherwise returns a
    // placeholder and schedules an asynchronous load (disk, then network).
    QPixmap tile(int z, int x, int y);

    void setUrlTemplate(const QString& tmpl);   // e.g. https://tile.openstreetmap.org/%1/%2/%3.png
    void setNetworkEnabled(bool enabled);
    bool networkEnabled() const { return m_networkEnabled; }

    // Directory holding tiles in the standard z/x/y.png slippy-map layout.
    // Used both as the read source (checked before any network access, so a
    // pre-downloaded folder makes the app fully offline) and as the download
    // cache. Defaults to the platform cache location.
    void setTileDirectory(const QString& dir);
    QString tileDirectory() const { return m_cacheDir; }

    static int tileSize() { return 256; }

signals:
    void tileReady(int z, int x, int y);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static QString key(int z, int x, int y);
    QString diskPath(int z, int x, int y) const;
    bool loadFromDisk(int z, int x, int y);
    void startDownload(int z, int x, int y);
    QPixmap placeholderTile(int z, int x, int y);

    QNetworkAccessManager* m_net;
    QHash<QString, QPixmap> m_memCache;
    QSet<QString> m_inFlight;       // keys currently downloading
    QHash<QString, QPixmap> m_placeholders;
    QString m_urlTemplate;
    QString m_cacheDir;
    bool m_networkEnabled = true;
};

#endif // TILEMANAGER_H

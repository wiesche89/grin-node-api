#ifndef NODEOWNERAPI_H
#define NODEOWNERAPI_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include "peerinfodisplay.h"
#include "peerdata.h"
#include "status.h"
#include "result.h"
#include "jsonutil.h"

// https://docs.rs/grin_api/latest/grin_api/owner_rpc/trait.OwnerRpc.html
class NodeOwnerApi : public QObject
{
    Q_OBJECT
public:
    explicit NodeOwnerApi(const QString &apiUrl, const QString &apiKey, QObject *parent = nullptr);

    // --- Async-Aufrufe (bereits QML-fähig) ---
    Q_INVOKABLE void banPeerAsync(const QString &peerAddr);
    Q_INVOKABLE void compactChainAsync();
    Q_INVOKABLE void getConnectedPeersAsync();
    Q_INVOKABLE void getPeersAsync(const QString &peerAddr = QString());
    Q_INVOKABLE void getStatusAsync();
    Q_INVOKABLE void unbanPeerAsync(const QString &peerAddr);
    Q_INVOKABLE void validateChainAsync(bool assumeValidRangeproofsKernels);

    // --- Polling: jetzt QML-invokable + Stop-Methoden ---
    Q_INVOKABLE void startStatusPolling(int intervalMs);
    Q_INVOKABLE void stopStatusPolling();
    Q_INVOKABLE void startConnectedPeersPolling(int intervalMs);
    Q_INVOKABLE void stopConnectedPeersPolling();

signals:
    void banPeerFinished(Result<bool> result);
    void compactChainFinished(Result<bool> result);
    void getConnectedPeersFinished(Result<QList<PeerInfoDisplay> > result);
    void getPeersFinished(Result<QList<PeerData> > result);
    void getPeersFinishedQml(const QJsonArray &peers);
    void getStatusFinished(Result<Status> result);
    void unbanPeerFinished(Result<bool> result);
    void validateChainFinished(Result<bool> result);

    void statusUpdated(const Status &status);
    void connectedPeersUpdated(const QList<PeerInfoDisplay> &peers);

private slots:
    void handleStatusResult(const Result<Status> &r);
    void handleConnectedPeersResult(const Result<QList<PeerInfoDisplay> > &r);

private:
    void postAsync(const QString &method, const QJsonArray &params, std::function<void(const QJsonObject &, const QString &)> handler);

    QString m_apiUrl;
    QString m_apiKey;
    QNetworkAccessManager *m_networkManager{nullptr};

    // --- Polling-Timer ---
    QTimer *m_statusPollTimer{nullptr};
    QTimer *m_peersPollTimer{nullptr};
};

#endif // NODEOWNERAPI_H

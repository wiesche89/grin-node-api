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

class NodeOwnerApi : public QObject
{
    Q_OBJECT
public:
    explicit NodeOwnerApi(const QString &apiUrl, const QString &apiKey, QObject *parent = nullptr);

    // Async-Aufrufe
    Q_INVOKABLE void banPeerAsync(const QString &peerAddr);
    Q_INVOKABLE void compactChainAsync();
    Q_INVOKABLE void getConnectedPeersAsync();
    Q_INVOKABLE void getPeersAsync(const QString &peerAddr = QString());
    Q_INVOKABLE void getStatusAsync();
    Q_INVOKABLE void unbanPeerAsync(const QString &peerAddr);
    Q_INVOKABLE void validateChainAsync(bool assumeValidRangeproofsKernels);

    // Polling
    void startStatusPolling(int intervalMs);
    void startConnectedPeersPolling(int intervalMs);

signals:
    void banPeerFinished(Result<bool> result);
    void compactChainFinished(Result<bool> result);
    void getConnectedPeersFinished(Result<QList<PeerInfoDisplay>> result);
    void getPeersFinished(Result<QList<PeerData>> result);
    void getStatusFinished(Result<Status> result);
    void unbanPeerFinished(Result<bool> result);
    void validateChainFinished(Result<bool> result);

    void statusUpdated(const Status &status);
    void connectedPeersUpdated(const QList<PeerInfoDisplay> &peers);

private:
    void postAsync(const QString &method,
                   const QJsonArray &params,
                   std::function<void(const QJsonObject&, const QString&)> handler);

    QString m_apiUrl;
    QString m_apiKey;
    QNetworkAccessManager *m_networkManager;
};

#endif // NODEOWNERAPI_H

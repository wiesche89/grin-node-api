#include "nodeownerapi.h"

NodeOwnerApi::NodeOwnerApi(const QString &apiUrl, const QString &apiKey, QObject *parent)
    : QObject(parent),
    m_apiUrl(apiUrl),
    m_apiKey(apiKey),
    m_networkManager(new QNetworkAccessManager(this))
{
}

void NodeOwnerApi::postAsync(const QString &method,
                             const QJsonArray &params,
                             std::function<void(const QJsonObject&, const QString&)> handler)
{
    QNetworkRequest req(m_apiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", m_apiKey.toUtf8());

    QJsonObject payload;
    payload["jsonrpc"] = "2.0";
    payload["method"] = method;
    payload["params"] = params;
    payload["id"] = 1;

    QNetworkReply *reply = m_networkManager->post(req, QJsonDocument(payload).toJson());

    connect(reply, &QNetworkReply::finished, this, [reply, handler]() {
        QJsonObject obj;
        QString err;
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonParseError jerr;
            QJsonDocument doc = QJsonDocument::fromJson(response, &jerr);
            if (jerr.error != QJsonParseError::NoError) {
                err = QStringLiteral("JSON parse error: %1").arg(jerr.errorString());
            } else if (!doc.isObject()) {
                err = QStringLiteral("Response is not an object");
            } else {
                obj = doc.object();
            }
        } else {
            err = reply->errorString();
        }
        reply->deleteLater();
        handler(obj, err);
    });
}

void NodeOwnerApi::banPeerAsync(const QString &peerAddr)
{
    QJsonArray params; params.append(peerAddr);
    postAsync("ban_peer", params, [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit banPeerFinished(Result<bool>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit banPeerFinished(Result<bool>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        emit banPeerFinished(Result<bool>(ok.isNull() || ok.isObject()));
    });
}

void NodeOwnerApi::compactChainAsync()
{
    postAsync("compact_chain", QJsonArray(), [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit compactChainFinished(Result<bool>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit compactChainFinished(Result<bool>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        emit compactChainFinished(Result<bool>(ok.isNull() || ok.isObject()));
    });
}

void NodeOwnerApi::getConnectedPeersAsync()
{
    postAsync("get_connected_peers", QJsonArray(), [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit getConnectedPeersFinished(Result<QList<PeerInfoDisplay>>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit getConnectedPeersFinished(Result<QList<PeerInfoDisplay>>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        QList<PeerInfoDisplay> peers;
        for (auto v : ok.toArray()) {
            if (v.isObject()) peers.append(PeerInfoDisplay::fromJson(v.toObject()));
        }
        emit getConnectedPeersFinished(Result<QList<PeerInfoDisplay>>(peers));
    });
}

void NodeOwnerApi::getPeersAsync(const QString &peerAddr)
{
    QJsonArray params;
    if (!peerAddr.isEmpty()) params.append(peerAddr); else params.append(QJsonValue::Null);
    postAsync("get_peers", params, [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit getPeersFinished(Result<QList<PeerData>>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit getPeersFinished(Result<QList<PeerData>>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        QList<PeerData> peers;
        for (auto v : ok.toArray()) {
            if (v.isObject()) peers.append(PeerData::fromJson(v.toObject()));
        }
        emit getPeersFinished(Result<QList<PeerData>>(peers));
    });
}

void NodeOwnerApi::getStatusAsync()
{
    postAsync("get_status", QJsonArray(), [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit getStatusFinished(Result<Status>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkObject(o);
        QJsonObject ok;
        if (!res.unwrapOrLog(ok)) {
            emit getStatusFinished(Result<Status>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        emit getStatusFinished(Result<Status>(Status::fromJson(ok)));
    });
}

void NodeOwnerApi::unbanPeerAsync(const QString &peerAddr)
{
    QJsonArray params; params.append(peerAddr);
    postAsync("unban_peer", params, [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit unbanPeerFinished(Result<bool>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit unbanPeerFinished(Result<bool>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        emit unbanPeerFinished(Result<bool>(ok.isNull() || ok.isObject()));
    });
}

void NodeOwnerApi::validateChainAsync(bool assumeValidRangeproofsKernels)
{
    QJsonArray params; params.append(assumeValidRangeproofsKernels);
    postAsync("validate_chain", params, [this](const QJsonObject &o, const QString &err){
        if (!err.isEmpty()) {
            emit validateChainFinished(Result<bool>(Error(ErrorType::ApiOther, err)));
            return;
        }
        auto res = JsonUtil::extractOkValue(o);
        QJsonValue ok;
        if (!res.unwrapOrLog(ok)) {
            emit validateChainFinished(Result<bool>(Error(ErrorType::ApiOther, res.errorMessage())));
            return;
        }
        emit validateChainFinished(Result<bool>(ok.isNull() || ok.isObject()));
    });
}

void NodeOwnerApi::startStatusPolling(int intervalMs)
{
    connect(this, &NodeOwnerApi::getStatusFinished, this, [this](const Result<Status> &r){
        Status s;
        if (r.unwrapOrLog(s)) emit statusUpdated(s);
        else qWarning() << "[Status Polling]" << r.errorMessage();
    });
    auto t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this]{ getStatusAsync(); });
    t->start(intervalMs);
    getStatusAsync();
}

void NodeOwnerApi::startConnectedPeersPolling(int intervalMs)
{
    connect(this, &NodeOwnerApi::getConnectedPeersFinished, this, [this](const Result<QList<PeerInfoDisplay>> &r){
        QList<PeerInfoDisplay> peers;
        if (r.unwrapOrLog(peers)) emit connectedPeersUpdated(peers);
        else qWarning() << "[Peers Polling]" << r.errorMessage();
    });
    auto t = new QTimer(this);
    connect(t, &QTimer::timeout, this, [this]{ getConnectedPeersAsync(); });
    t->start(intervalMs);
    getConnectedPeersAsync();
}

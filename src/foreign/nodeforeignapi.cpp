#include "nodeforeignapi.h"
#include <QJsonValue>
#include <QDebug>

// ---------------------------------------------------------
// ctor
// ---------------------------------------------------------
NodeForeignApi::NodeForeignApi(QString apiUrl, QString apiKey) :
    m_apiUrl(std::move(apiUrl)),
    m_apiKey(std::move(apiKey)),
    m_networkManager(new QNetworkAccessManager(this)),
    m_mempoolPollTimer(new QTimer(this))
{
    m_mempoolPollTimer->setSingleShot(false);
    connect(m_mempoolPollTimer, &QTimer::timeout, this, &NodeForeignApi::onMempoolPollTick);
}

// ---------------------------------------------------------
// JSON-RPC Async POST
// ---------------------------------------------------------
void NodeForeignApi::postAsync(const QString &method, const QJsonArray &params, std::function<void(const QJsonObject &,
                                                                                                   const QString &)> handler)
{
    QNetworkRequest req{ QUrl(m_apiUrl) };
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_apiKey.isEmpty()) {
        req.setRawHeader("Authorization", m_apiKey.toUtf8());
    }

    const QJsonObject payload{
        { "jsonrpc", "2.0" },
        { "id", 1 },
        { "method", method },
        { "params", params }
    };
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply, handler]() {
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            handler(QJsonObject{}, err);
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            handler(QJsonObject{}, QStringLiteral("Parse error: %1").arg(pe.errorString()));
            return;
        }
        handler(doc.object(), QString{});
    });
}

// ---------------------------------------------------------
// Async-Methoden
// ---------------------------------------------------------
void NodeForeignApi::getBlockAsync(int height, const QString &hash, const QString &commit)
{
    QJsonArray params;
    params << height
           << (hash.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(hash))
           << (commit.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(commit));

    postAsync("get_block", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getBlockFinished(Result<BlockPrintable>::error(err));
            return;
        }
        emit getBlockFinished(parseBlockPrintable(obj));
    });
}

void NodeForeignApi::getBlocksAsync(int startHeight, int endHeight, int max, bool includeProof)
{
    QJsonArray params;
    params << startHeight << endHeight << max << includeProof;

    postAsync("get_blocks", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getBlocksFinished(Result<BlockListing>::error(err));
            return;
        }
        const BlockListing &bl = parseBlockListing(obj).value();
        emit blocksUpdated(bl.blocksVariant(), bl.lastRetrievedHeight());
        emit getBlocksFinished(bl);
    });
}

void NodeForeignApi::getHeaderAsync(int height, const QString &hash, const QString &commit)
{
    QJsonArray params;
    params << height
           << (hash.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(hash))
           << (commit.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(commit));

    postAsync("get_header", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getHeaderFinished(Result<BlockHeaderPrintable>::error(err));
            return;
        }

        const BlockHeaderPrintable &b = parseBlockHeaderPrintable(obj).value();
        emit headerUpdated(b);
        emit getHeaderFinished(b);
    });
}

void NodeForeignApi::getKernelAsync(const QString &excess, int minHeight, int maxHeight)
{
    QJsonArray params;
    params << excess << minHeight << maxHeight;
    postAsync("get_kernel", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getKernelFinished(Result<LocatedTxKernel>::error(err));
            return;
        }
        emit getKernelFinished(parseLocatedTxKernel(obj));
    });
}

void NodeForeignApi::getOutputsAsync(const QJsonArray &commits, int startHeight, int endHeight, bool includeProof, bool includeMerkleProof)
{
    QJsonArray params;
    params << commits << startHeight << endHeight << includeProof << includeMerkleProof;
    postAsync("get_outputs", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getOutputsFinished(Result<QList<OutputPrintable> >::error(err));
            return;
        }
        emit getOutputsFinished(parseOutputPrintableList(obj));
    });
}

void NodeForeignApi::getPmmrIndicesAsync(int startHeight, int endHeight)
{
    QJsonArray params;
    params << startHeight << endHeight;
    postAsync("get_pmmr_indices", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getPmmrIndicesFinished(Result<OutputListing>::error(err));
            return;
        }
        emit getPmmrIndicesFinished(parseOutputListing(obj));
    });
}

void NodeForeignApi::getPoolSizeAsync()
{
    postAsync("get_pool_size", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getPoolSizeFinished(Result<int>::error(err));
            return;
        }
        auto r = parseIntResult(obj);
        emit getPoolSizeFinished(r);
        if (!r.hasError()) {
            emit poolSizeUpdated(r.value());
        }
    });
}

void NodeForeignApi::getStempoolSizeAsync()
{
    postAsync("get_stempool_size", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getStempoolSizeFinished(Result<int>::error(err));
            return;
        }
        auto r = parseIntResult(obj);
        emit getStempoolSizeFinished(r);
        if (!r.hasError()) {
            emit stempoolSizeUpdated(r.value());
        }
    });
}

void NodeForeignApi::getTipAsync()
{
    postAsync("get_tip", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getTipFinished(Result<Tip>::error(err));
            return;
        }

        auto r = parseTipResult(obj);

        emit getTipFinished(r);
        if (!r.hasError()) {
            emit tipUpdated(r.value());
        }
    });
}

void NodeForeignApi::getUnconfirmedTransactionsAsync()
{
    postAsync("get_unconfirmed_transactions", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getUnconfirmedTransactionsFinished(Result<QList<PoolEntry> >::error(err));
            return;
        }
        auto r = parsePoolEntries(obj);
        emit getUnconfirmedTransactionsFinished(r);
        if (!r.hasError()) {
            emit unconfirmedTransactionsUpdated(r.value());
        }
    });
}

void NodeForeignApi::getUnspentOutputsAsync(int startHeight, int endHeight, int max, bool includeProof)
{
    QJsonArray params;
    params << startHeight << endHeight << max << includeProof;
    postAsync("get_unspent_outputs", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getUnspentOutputsFinished(Result<BlockListing>::error(err));
            return;
        }
        emit getUnspentOutputsFinished(parseBlockListing(obj));
    });
}

void NodeForeignApi::getVersionAsync()
{
    postAsync("get_version", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getVersionFinished(Result<NodeVersion>::error(err));
            return;
        }
        emit getVersionFinished(parseNodeVersion(obj));
    });
}

void NodeForeignApi::pushTransactionAsync(const Transaction &tx, bool fluff)
{
    // Annahme: Transaction hat toJson()/toObject(); sonst anpassen
    QJsonObject txObj = tx.toJson(); // ggf. tx.toObject()
    QJsonArray params;
    params << txObj << fluff;

    postAsync("push_transaction", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit pushTransactionFinished(Result<bool>::error(err));
            return;
        }
        emit pushTransactionFinished(parseBoolResult(obj));
    });
}

// ---------------------------------------------------------
// Polling
// ---------------------------------------------------------
void NodeForeignApi::startMempoolPolling(int intervalMs)
{
    if (intervalMs < 1000) {
        intervalMs = 1000;
    }
    m_mempoolPollTimer->start(intervalMs);
    onMempoolPollTick();
}

void NodeForeignApi::stopMempoolPolling()
{
    m_mempoolPollTimer->stop();
}

void NodeForeignApi::onMempoolPollTick()
{
    getTipAsync();
    getPoolSizeAsync();
    getStempoolSizeAsync();
    getUnconfirmedTransactionsAsync();
}

// ---------------------------------------------------------
// Parser
// ---------------------------------------------------------
Result<int> NodeForeignApi::parseIntResult(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkValue(rpcObj); // Result<QJsonValue>
    QJsonValue v;
    if (!r.unwrapOrLog(v)) {
        return Result<int>::error(r.errorMessage());
    }
    if (!v.isDouble()) {
        return Result<int>::error(QStringLiteral("Expected integer"));
    }
    return Result<int>(v.toInt());
}

Result<bool> NodeForeignApi::parseBoolResult(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkValue(rpcObj);
    QJsonValue v;
    if (!r.unwrapOrLog(v)) {
        return Result<bool>::error(r.errorMessage());
    }
    if (v.isBool()) {
        return Result<bool>(v.toBool());
    }
    if (v.isDouble()) {
        return Result<bool>(v.toInt() != 0);               // manche Endpunkte -> 0/1
    }
    return Result<bool>::error(QStringLiteral("Expected boolean"));
}

Result<Tip> NodeForeignApi::parseTipResult(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<Tip>::error(r.errorMessage());
    }
    Tip t;
    t = Tip::fromJson(obj);

    return Result<Tip>(t);
}

Result<BlockPrintable> NodeForeignApi::parseBlockPrintable(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<BlockPrintable>::error(r.errorMessage());
    }
    BlockPrintable b;
    b.fromJson(obj);
    return Result<BlockPrintable>(b);
}

Result<BlockListing> NodeForeignApi::parseBlockListing(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<BlockListing>::error(r.errorMessage());
    }
    BlockListing bl;
    bl.fromJson(obj);

    return Result<BlockListing>(bl);
}

Result<BlockHeaderPrintable> NodeForeignApi::parseBlockHeaderPrintable(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<BlockHeaderPrintable>::error(r.errorMessage());
    }
    BlockHeaderPrintable h;
    h.fromJson(obj);

    qDebug()<<Q_FUNC_INFO;
    qDebug()<<h.cuckooSolution();
    qDebug()<<h.toJson();

    return Result<BlockHeaderPrintable>(h);
}

Result<LocatedTxKernel> NodeForeignApi::parseLocatedTxKernel(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<LocatedTxKernel>::error(r.errorMessage());
    }
    LocatedTxKernel k;
    k.fromJson(obj);
    return Result<LocatedTxKernel>(k);
}

Result<QList<OutputPrintable> > NodeForeignApi::parseOutputPrintableList(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkValue(rpcObj);
    QJsonValue v;
    if (!r.unwrapOrLog(v)) {
        return Result<QList<OutputPrintable> >::error(r.errorMessage());
    }
    if (!v.isArray()) {
        return Result<QList<OutputPrintable> >::error(QStringLiteral("Expected array"));
    }
    QList<OutputPrintable> out;
    for (const QJsonValue &e : v.toArray()) {
        if (!e.isObject()) {
            continue;
        }
        OutputPrintable op;
        op.fromJson(e.toObject());
        out.push_back(op);
    }
    return Result<QList<OutputPrintable> >(out);
}

Result<OutputListing> NodeForeignApi::parseOutputListing(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<OutputListing>::error(r.errorMessage());
    }
    OutputListing l;
    l.fromJson(obj);
    return Result<OutputListing>(l);
}

Result<QList<PoolEntry> > NodeForeignApi::parsePoolEntries(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkValue(rpcObj);
    QJsonValue v;
    if (!r.unwrapOrLog(v)) {
        return Result<QList<PoolEntry> >::error(r.errorMessage());
    }
    if (!v.isArray()) {
        return Result<QList<PoolEntry> >::error(QStringLiteral("Expected array"));
    }
    QList<PoolEntry> out;
    out.reserve(v.toArray().size());
    for (const QJsonValue &e : v.toArray()) {
        if (!e.isObject()) {
            continue;
        }
        PoolEntry pe;
        pe.fromJson(e.toObject());
        out.push_back(pe);
    }
    return Result<QList<PoolEntry> >(out);
}

Result<NodeVersion> NodeForeignApi::parseNodeVersion(const QJsonObject &rpcObj)
{
    auto r = JsonUtil::extractOkObject(rpcObj);
    QJsonObject obj;
    if (!r.unwrapOrLog(obj)) {
        return Result<NodeVersion>::error(r.errorMessage());
    }
    NodeVersion nv;
    nv.fromJson(obj);
    return Result<NodeVersion>(nv);
}

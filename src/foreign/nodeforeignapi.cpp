#include "nodeforeignapi.h"
#include <QJsonValue>
#include <QSet>
#include <memory>
#include "wallet/grinwalletnodepushhelpers.h"

extern "C" {
#include "secp256k1.h"
}

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
            emit blockLookupFailed(err);
            emit getBlockFinished(Result<BlockPrintable>::error(err));
            return;
        }
        const auto parsed = parseBlockPrintable(obj);
        if (parsed.hasError()) {
            emit blockLookupFailed(parsed.errorMessage());
            emit getBlockFinished(parsed);
            return;
        }

        emit blockUpdated(parsed.value().toJson().toVariantMap());
        emit getBlockFinished(parsed);
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
        const auto parsed = parseBlockListing(obj);
        if (parsed.hasError()) {
            emit getBlocksFinished(parsed);
            return;
        }

        const BlockListing &bl = parsed.value();

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
            emit headerLookupFailed(err);
            emit getHeaderFinished(Result<BlockHeaderPrintable>::error(err));
            return;
        }
        const auto parsed = parseBlockHeaderPrintable(obj);
        if (parsed.hasError()) {
            emit headerLookupFailed(parsed.errorMessage());
            emit getHeaderFinished(parsed);
            return;
        }

        const BlockHeaderPrintable &b = parsed.value();
        emit headerUpdated(b);
        emit headerUpdatedQml(b.toJson().toVariantMap());
        emit getHeaderFinished(parsed);
    });
}

void NodeForeignApi::getKernelAsync(const QString &excess, int minHeight, int maxHeight)
{
    QJsonArray params;
    params << excess << minHeight << maxHeight;

    postAsync("get_kernel", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit kernelLookupFailed(err);
            emit getKernelFinished(Result<LocatedTxKernel>::error(err));
            return;
        }
        const auto parsed = parseLocatedTxKernel(obj);
        if (parsed.hasError()) {
            // A missing kernel is expected while a freshly broadcast transaction is still only in the mempool.
            if (parsed.errorMessage() != QStringLiteral("NotFound")) {
                emit kernelLookupFailed(parsed.errorMessage());
            }
            emit getKernelFinished(parsed);
            return;
        }

        const LocatedTxKernel &b = parsed.value();
        emit kernelUpdated(b);
        emit kernelUpdatedQml(b.toJson().toVariantMap());
        emit getKernelFinished(parsed);
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

void NodeForeignApi::getOutputCommitmentsAsync(const QJsonArray &commits)
{
    if (commits.isEmpty()) {
        emit getOutputCommitmentsFinished(Result<QList<OutputPrintable> >(QList<OutputPrintable>()));
        return;
    }

    QJsonArray params;
    // get_outputs expects commitments plus optional height bounds.
    // Use null bounds for commitment-only lookup to avoid broad range scans.
    params << commits
           << QJsonValue(QJsonValue::Null)
           << QJsonValue(QJsonValue::Null)
           << false
           << false;
    postAsync("get_outputs", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getOutputCommitmentsFinished(Result<QList<OutputPrintable> >::error(err));
            return;
        }

        const Result<QList<OutputPrintable> > parsed = parseOutputPrintableList(obj);
        if (parsed.hasError()) {
            emit getOutputCommitmentsFinished(Result<QList<OutputPrintable> >::error(parsed.errorMessage()));
            return;
        }
        emit getOutputCommitmentsFinished(parsed);
    });
}

void NodeForeignApi::getPmmrIndicesAsync(int startHeight, int endHeight)
{
    QJsonArray params;
    params << startHeight << endHeight;
    postAsync("get_pmmr_indices", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getPmmrIndicesFinished(Result<OutputListing>::error(err));
            emit pmmrIndicesLookupFailed(err);
            return;
        }
        auto r = parseOutputListing(obj);
        emit getPmmrIndicesFinished(r);
        if (r.hasError()) {
            emit pmmrIndicesLookupFailed(r.errorMessage());
            return;
        }

        QVariantList outputsVariant;
        const auto outputs = r.value().outputs();
        outputsVariant.reserve(outputs.size());
        for (const auto &output : outputs) {
            outputsVariant.append(output.toJson().toVariantMap());
        }
        emit pmmrIndicesUpdated(outputsVariant,
                               r.value().highestIndex(),
                               r.value().lastRetrievedIndex());
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
    params << startHeight
           << (endHeight < 0 ? QJsonValue(QJsonValue::Null) : QJsonValue(endHeight))
           << max
           << includeProof;
    postAsync("get_unspent_outputs", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getUnspentOutputsFinished(Result<OutputListing>::error(err));
            emit unspentOutputsLookupFailed(err);
            return;
        }

        QJsonObject payload;
        if (obj.contains("result") && obj.value("result").isObject()) {
            const QJsonObject resultObj = obj.value("result").toObject();
            if (resultObj.contains("Ok") && resultObj.value("Ok").isObject()) {
                payload = resultObj.value("Ok").toObject();
            } else {
                payload = resultObj;
            }
        }

        OutputListing listing;
        if (!payload.isEmpty()) {
            listing = OutputListing::fromJson(payload);
        }

        if (payload.isEmpty()) {
            auto r = parseOutputListing(obj);
            emit getUnspentOutputsFinished(r);
            if (r.hasError()) {
                emit unspentOutputsLookupFailed(r.errorMessage());
                return;
            }
            listing = r.value();
        }

        emit getUnspentOutputsFinished(Result<OutputListing>(listing));

        QVariantList outputsVariant;
        const auto outputs = listing.outputs();
        outputsVariant.reserve(outputs.size());
        for (const auto &output : outputs) {
            outputsVariant.append(output.toJson().toVariantMap());
        }
        emit unspentOutputsUpdated(outputsVariant,
                                   listing.highestIndex(),
                                   listing.lastRetrievedIndex());
    });
}

void NodeForeignApi::getVersionAsync()
{
    postAsync("get_version", QJsonArray{}, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            emit getVersionFinished(Result<NodeVersion>::error(err));
            return;
        }

        auto parsed = parseNodeVersion(obj);
        emit getVersionFinished(parsed);
    });
}

void NodeForeignApi::pushTransactionAsync(const Transaction &tx, bool fluff)
{
    struct PushDiagContext {
        quint64 preTipHeight = 0;
        QString preTipHash;
        QString txOffset;
    };

    const QJsonObject txObj = GrinWalletNodePushHelpers::serializeTransactionForNode(tx);
    const std::shared_ptr<PushDiagContext> diagCtx = std::make_shared<PushDiagContext>();
    diagCtx->txOffset = txObj.value(QStringLiteral("offset")).toString();

    const QJsonObject legacyKernelTxObj =
        GrinWalletNodePushHelpers::serializeTransactionForNodeLegacyKernel(tx);

    QJsonArray params;
    params << txObj << fluff;

    const QString txOffset = txObj.value(QStringLiteral("offset")).toString();

    const auto sendPushTransaction = [this, params, diagCtx, txObj, fluff, legacyKernelTxObj]() {
        postAsync("push_transaction", params, [this, params, diagCtx, txObj, fluff, legacyKernelTxObj](const QJsonObject &obj, const QString &err) {
            if (!err.isEmpty()) {
                emit pushTransactionFinished(Result<bool>::error(err));
                return;
            }
            const Result<bool> parsed = parseBoolResult(obj);
            if (parsed.hasError()) {
                const bool shouldRetryAsStem = fluff && parsed.errorMessage().contains(QStringLiteral("keychain"), Qt::CaseInsensitive);
                const bool shouldTryBinaryPoolPush = parsed.errorMessage().contains(QStringLiteral("keychain"), Qt::CaseInsensitive);

                const auto continueAfterBinaryFallback = [this,
                                                         params,
                                                         diagCtx,
                                                         txObj,
                                                         fluff,
                                                         legacyKernelTxObj,
                                                         parsed,
                                                         shouldRetryAsStem]() {
                if (shouldRetryAsStem) {
                    QJsonArray stemParams = params;
                    if (stemParams.size() >= 2) {
                        stemParams[1] = false;
                    }
                    postAsync("push_transaction", stemParams, [this, parsed, legacyKernelTxObj](const QJsonObject &retryObj, const QString &retryErr) {
                        if (!retryErr.isEmpty()) {
                            emit pushTransactionFinished(parsed);
                            return;
                        }

                        const Result<bool> retryParsed = parseBoolResult(retryObj);
                        if (retryParsed.hasError()) {
                            const bool shouldTryLegacyKernelPayload = retryParsed.errorMessage().contains(QStringLiteral("keychain"), Qt::CaseInsensitive);
                            if (!shouldTryLegacyKernelPayload) {
                                emit pushTransactionFinished(parsed);
                                return;
                            }

                            QJsonArray legacyStemParams;
                            legacyStemParams << legacyKernelTxObj << false;
                            postAsync("push_transaction", legacyStemParams,
                                      [this, parsed](const QJsonObject &legacyObj, const QString &legacyErr) {
                                if (!legacyErr.isEmpty()) {
                                    emit pushTransactionFinished(parsed);
                                    return;
                                }

                                const Result<bool> legacyParsed = parseBoolResult(legacyObj);
                                if (legacyParsed.hasError()) {
                                    emit pushTransactionFinished(parsed);
                                    return;
                                }

                                emit pushTransactionFinished(legacyParsed);
                            });
                            return;
                        }

                        emit pushTransactionFinished(retryParsed);
                    });
                    return;
                }

                    emit pushTransactionFinished(parsed);
                };

                if (shouldTryBinaryPoolPush) {
                    QString serializeError;
                    const QByteArray txBytes =
                        GrinWalletNodePushHelpers::serializeTransactionForPoolV1(
                            Transaction::fromJson(txObj), &serializeError);
                    if (txBytes.isEmpty()) {
                        continueAfterBinaryFallback();
                        return;
                    }

                    const QList<QUrl> restUrls =
                        GrinWalletNodePushHelpers::poolPushCandidateUrlsForApiUrl(m_apiUrl, fluff);
                    const QJsonObject restPayload{
                        { QStringLiteral("tx_hex"), QString::fromUtf8(txBytes.toHex()) }
                    };
                    const QByteArray restBody = QJsonDocument(restPayload).toJson(QJsonDocument::Compact);

                    const std::shared_ptr<int> restIndex = std::make_shared<int>(0);
                    const std::shared_ptr<std::function<void()>> tryNextRestUrl = std::make_shared<std::function<void()>>();
                    *tryNextRestUrl = [this,
                                      restUrls,
                                      restBody,
                                      txBytes,
                                      restIndex,
                                      tryNextRestUrl,
                                      continueAfterBinaryFallback]() {
                        if (*restIndex >= restUrls.size()) {
                            continueAfterBinaryFallback();
                            return;
                        }

                        const QUrl restUrl = restUrls.at(*restIndex);

                        QNetworkRequest restReq{restUrl};
                        restReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                        if (!m_apiKey.isEmpty()) {
                            restReq.setRawHeader("Authorization", m_apiKey.toUtf8());
                        }

                        QNetworkReply *restReply = m_networkManager->post(restReq, restBody);
                        connect(restReply, &QNetworkReply::finished, this, [this,
                                                                            restReply,
                                                                            restIndex,
                                                                            restUrls,
                                                                            tryNextRestUrl,
                                                                            continueAfterBinaryFallback]() {
                            const QByteArray responseBody = restReply->readAll();
                            const QNetworkReply::NetworkError networkError = restReply->error();
                            const QString errorString = restReply->errorString();
                            const int statusCode = restReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                            const QString attemptedUrl = restReply->url().toString();
                            restReply->deleteLater();

                            if (networkError == QNetworkReply::NoError && statusCode >= 200 && statusCode < 300) {
                                emit pushTransactionFinished(Result<bool>(true));
                                return;
                            }

                            Q_UNUSED(errorString)
                            Q_UNUSED(attemptedUrl)
                            Q_UNUSED(responseBody)

                            (*restIndex)++;
                            if (*restIndex < restUrls.size()) {
                                (*tryNextRestUrl)();
                                return;
                            }
                            continueAfterBinaryFallback();
                        });
                    };

                    (*tryNextRestUrl)();
                    return;
                }

                continueAfterBinaryFallback();
                return;
            }
            emit pushTransactionFinished(parsed);
        });
    };

    postAsync("get_tip", QJsonArray{}, [this, txOffset, sendPushTransaction, diagCtx](const QJsonObject &tipObj,
                                                                                       const QString &tipErr) {
        if (!tipErr.isEmpty()) {
            sendPushTransaction();
            return;
        }

        const Result<Tip> tip = parseTipResult(tipObj);
        if (tip.hasError()) {
            sendPushTransaction();
            return;
        }

        const quint64 tipHeight = tip.value().height();
        diagCtx->preTipHeight = tipHeight;
        diagCtx->preTipHash = tip.value().lastBlockPushed();
        QJsonArray headerParams;
        headerParams << static_cast<qint64>(tipHeight)
                     << QJsonValue(QJsonValue::Null)
                     << QJsonValue(QJsonValue::Null);
        postAsync("get_header", headerParams, [this, txOffset, tipHeight, sendPushTransaction](const QJsonObject &headerObj,
                                                                                                 const QString &headerErr) {
            if (!headerErr.isEmpty()) {
                sendPushTransaction();
                return;
            }

            const Result<BlockHeaderPrintable> header = parseBlockHeaderPrintable(headerObj);
            if (header.hasError()) {
                sendPushTransaction();
                return;
            }

            sendPushTransaction();
        });
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
    if (v.isNull() || v.isUndefined()) {
        return Result<bool>(true); // push_transaction on node 5.4.0 returns Ok: null on success
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
    OutputListing l = OutputListing::fromJson(obj);
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
        PoolEntry pe = PoolEntry::fromJson(e.toObject());
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

    NodeVersion nv = NodeVersion::fromJson(obj);
    return Result<NodeVersion>(nv);
}

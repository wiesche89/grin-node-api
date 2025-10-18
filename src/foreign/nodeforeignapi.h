#ifndef NODEFOREIGNAPI_H
#define NODEFOREIGNAPI_H

#include <QObject>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <functional>

#include "blockheaderprintable.h"
#include "blockprintable.h"
#include "blocklisting.h"
#include "locatedtxkernel.h"
#include "outputlisting.h"
#include "tip.h"
#include "poolentry.h"
#include "nodeversion.h"
#include "transaction.h"
#include "result.h"
#include "jsonutil.h"


// https://docs.rs/grin_api/latest/grin_api/foreign_rpc/trait.ForeignRpc.html
// https://docs.grin.mw/grin-rfcs/text/0007-node-api-v2/
class NodeForeignApi : public QObject
{
    Q_OBJECT
public:
    explicit NodeForeignApi(QString apiUrl, QString apiKey);

    // ------------------- Async-Methoden (QML-fähig) -------------------
    Q_INVOKABLE void getBlockAsync(int height, const QString &hash, const QString &commit);
    Q_INVOKABLE void getBlocksAsync(int startHeight, int endHeight, int max, bool includeProof);
    Q_INVOKABLE void getHeaderAsync(int height, const QString &hash, const QString &commit);
    Q_INVOKABLE void getKernelAsync(const QString &excess, int minHeight, int maxHeight);
    Q_INVOKABLE void getOutputsAsync(const QJsonArray &commits, int startHeight, int endHeight, bool includeProof, bool includeMerkleProof);
    Q_INVOKABLE void getPmmrIndicesAsync(int startHeight, int endHeight);
    Q_INVOKABLE void getPoolSizeAsync();
    Q_INVOKABLE void getStempoolSizeAsync();
    Q_INVOKABLE void getTipAsync();
    Q_INVOKABLE void getUnconfirmedTransactionsAsync();
    Q_INVOKABLE void getUnspentOutputsAsync(int startHeight, int endHeight, int max, bool includeProof);
    Q_INVOKABLE void getVersionAsync();
    Q_INVOKABLE void pushTransactionAsync(const Transaction &tx, bool fluff);

    // Optionales Gesamt-Polling (z. B. für Pool-Ansicht)
    Q_INVOKABLE void startMempoolPolling(int intervalMs);
    Q_INVOKABLE void stopMempoolPolling();

signals:
    void getBlockFinished(Result<BlockPrintable> result);

    void getBlocksFinished(Result<BlockListing> result);
    void blocksUpdated(const QVariantList &blocks, quint64 lastRetrievedHeight);

    void getHeaderFinished(Result<BlockHeaderPrintable> result);
    void headerUpdated(const BlockHeaderPrintable &header);

    void getKernelFinished(Result<LocatedTxKernel> result);
    void kernelUpdated(const LocatedTxKernel &header);

    void getOutputsFinished(Result<QList<OutputPrintable> > result);
    void getPmmrIndicesFinished(Result<OutputListing> result);
    void getPoolSizeFinished(Result<int> result);
    void getStempoolSizeFinished(Result<int> result);
    void getTipFinished(Result<Tip> result);
    void getUnconfirmedTransactionsFinished(Result<QList<PoolEntry> > result);
    void getUnspentOutputsFinished(Result<BlockListing> result);
    void getVersionFinished(Result<NodeVersion> result);
    void pushTransactionFinished(Result<bool> result);

    // Optionale Live-Updates (bei Polling)
    void tipUpdated(const Tip &tip);
    void poolSizeUpdated(int size);
    void stempoolSizeUpdated(int size);
    void unconfirmedTransactionsUpdated(const QList<PoolEntry> &entries);

private slots:
    void onMempoolPollTick();

private:
    // Gemeinsames Async-POST (JSON-RPC)
    void postAsync(const QString &method, const QJsonArray &params, std::function<void(const QJsonObject &, const QString &)> handler);

    // Parser
    static Result<int> parseIntResult(const QJsonObject &rpcObj);
    static Result<bool> parseBoolResult(const QJsonObject &rpcObj);
    static Result<Tip> parseTipResult(const QJsonObject &rpcObj);
    static Result<BlockPrintable> parseBlockPrintable(const QJsonObject &rpcObj);
    static Result<BlockListing> parseBlockListing(const QJsonObject &rpcObj);
    static Result<BlockHeaderPrintable> parseBlockHeaderPrintable(const QJsonObject &rpcObj);
    static Result<LocatedTxKernel> parseLocatedTxKernel(const QJsonObject &rpcObj);
    static Result<QList<OutputPrintable> > parseOutputPrintableList(const QJsonObject &rpcObj);
    static Result<OutputListing> parseOutputListing(const QJsonObject &rpcObj);
    static Result<QList<PoolEntry> > parsePoolEntries(const QJsonObject &rpcObj);
    static Result<NodeVersion> parseNodeVersion(const QJsonObject &rpcObj);

    QString m_apiUrl;
    QString m_apiKey;
    QNetworkAccessManager *m_networkManager{nullptr};

    QTimer *m_mempoolPollTimer{nullptr};
};

#endif // NODEFOREIGNAPI_H

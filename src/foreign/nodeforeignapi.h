#ifndef NODEFOREIGNAPI_H
#define NODEFOREIGNAPI_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
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
    struct RescanOutput {
        QString commitment;
        QString proof;
        quint64 blockHeight{0};
        bool spent{false};
        bool coinbase{false};
    };

    struct RescanBatchProgress {
        quint64 highestIndex{0};
        quint64 lastRetrievedIndex{0};
        int outputsProcessed{0};
    };

    using RescanOutputHandler = std::function<QString(const RescanOutput &)>;
    using RescanBatchFinishedHandler = std::function<void(const Result<RescanBatchProgress> &)>;

    explicit NodeForeignApi(QString apiUrl, QString apiKey);

    // ------------------- Async-Methoden (QML-fähig) -------------------
    Q_INVOKABLE void getBlockAsync(int height, const QString &hash, const QString &commit);
    Q_INVOKABLE void getBlocksAsync(int startHeight, int endHeight, int max, bool includeProof);
    Q_INVOKABLE void getHeaderAsync(int height, const QString &hash, const QString &commit);
    Q_INVOKABLE void getKernelAsync(const QString &excess, int minHeight, int maxHeight);
    Q_INVOKABLE void getOutputsAsync(const QJsonArray &commits, int startHeight, int endHeight, bool includeProof, bool includeMerkleProof);
    Q_INVOKABLE void getOutputCommitmentsAsync(const QJsonArray &commits);
    Q_INVOKABLE void getPmmrIndicesAsync(int startHeight, int endHeight);
    Q_INVOKABLE void getPoolSizeAsync();
    Q_INVOKABLE void getStempoolSizeAsync();
    Q_INVOKABLE void getTipAsync();
    Q_INVOKABLE void getUnconfirmedTransactionsAsync();
    Q_INVOKABLE void getUnspentOutputsAsync(int startHeight, int endHeight, int max, bool includeProof);
    Q_INVOKABLE void getVersionAsync();
    Q_INVOKABLE void pushTransactionAsync(const Transaction &tx, bool fluff);
    void getUnspentOutputsForRescanAsync(int startHeight,
                                         int endHeight,
                                         int max,
                                         bool includeProofForRescan,
                                         const RescanOutputHandler &outputHandler,
                                         const RescanBatchFinishedHandler &finishedHandler);

    // Optionales Gesamt-Polling (z. B. für Pool-Ansicht)
    Q_INVOKABLE void startMempoolPolling(int intervalMs);
    Q_INVOKABLE void stopMempoolPolling();

signals:
    void getBlockFinished(Result<BlockPrintable> result);
    void blockUpdated(const QVariantMap &block);
    void blockLookupFailed(const QString &message);

    void getBlocksFinished(Result<BlockListing> result);
    void blocksUpdated(const QVariantList &blocks, quint64 lastRetrievedHeight);

    void getHeaderFinished(Result<BlockHeaderPrintable> result);
    void headerUpdated(const BlockHeaderPrintable &header);
    void headerUpdatedQml(const QVariantMap &header);
    void headerLookupFailed(const QString &message);

    void getKernelFinished(Result<LocatedTxKernel> result);
    void kernelUpdated(const LocatedTxKernel &header);
    void kernelUpdatedQml(const QVariantMap &kernel);
    void kernelLookupFailed(const QString &message);

    void getOutputsFinished(Result<QList<OutputPrintable> > result);
    void getOutputCommitmentsFinished(Result<QList<OutputPrintable> > result);
    void getPmmrIndicesFinished(Result<OutputListing> result);
    void getPoolSizeFinished(Result<int> result);
    void getStempoolSizeFinished(Result<int> result);
    void getTipFinished(Result<Tip> result);
    void getUnconfirmedTransactionsFinished(Result<QList<PoolEntry> > result);
    void getUnspentOutputsFinished(Result<OutputListing> result);
    void getVersionFinished(Result<NodeVersion> result);
    void pushTransactionFinished(Result<bool> result);

    // Optionale Live-Updates (bei Polling)
    void tipUpdated(const Tip &tip);
    void poolSizeUpdated(int size);
    void stempoolSizeUpdated(int size);
    void unconfirmedTransactionsUpdated(const QList<PoolEntry> &entries);
    void unspentOutputsUpdated(const QVariantList &outputs, quint64 highestIndex, quint64 lastRetrievedIndex);
    void unspentOutputsLookupFailed(const QString &message);
    void pmmrIndicesUpdated(const QVariantList &outputs, quint64 highestIndex, quint64 lastRetrievedIndex);
    void pmmrIndicesLookupFailed(const QString &message);

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
    bool m_rescanRequestInFlight{false};

    QTimer *m_mempoolPollTimer{nullptr};
};

#endif // NODEFOREIGNAPI_H

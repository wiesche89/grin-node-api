#include "nodeforeignapi.h"
#include <QJsonValue>
#include <QDebug>
#include <QCryptographicHash>
#include <QSet>
#include <QUrlQuery>
#include <array>
#include <memory>
#include <mutex>
#include "outputfeatures.h"

extern "C" {
#include "secp256k1.h"
#include "secp256k1_aggsig.h"
#include "secp256k1_commitment.h"
}

namespace {

bool appendExactHexBytes(QByteArray *out, const QString &hex, int expectedSize)
{
    if (!out) {
        return false;
    }

    const QByteArray bytes = QByteArray::fromHex(hex.trimmed().toUtf8());
    if (bytes.size() != expectedSize) {
        return false;
    }

    out->append(bytes);
    return true;
}

void appendU64Network(QByteArray *out, quint64 value)
{
    if (!out) {
        return;
    }

    for (int shift = 56; shift >= 0; shift -= 8) {
        out->append(static_cast<char>((value >> shift) & 0xff));
    }
}

QByteArray serializeTransactionForPoolV1(const Transaction &tx, QString *errorOut)
{
    QByteArray encoded;

    if (!appendExactHexBytes(&encoded, tx.offset().hex(), 32)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid transaction offset for v1 binary serialization.");
        }
        return QByteArray();
    }

    const TransactionBody body = tx.body();
    const QVector<Input> inputs = body.inputs();
    const QVector<Output> outputs = body.outputs();
    const QVector<TxKernel> kernels = body.kernels();

    appendU64Network(&encoded, static_cast<quint64>(inputs.size()));
    appendU64Network(&encoded, static_cast<quint64>(outputs.size()));
    appendU64Network(&encoded, static_cast<quint64>(kernels.size()));

    for (const Input &input : inputs) {
        const quint8 feature = input.features() == OutputFeatures::Coinbase ? 1 : 0;
        encoded.append(static_cast<char>(feature));
        if (!appendExactHexBytes(&encoded, input.commit().hex(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid input commitment for v1 binary serialization.");
            }
            return QByteArray();
        }
    }

    for (const Output &output : outputs) {
        const QString featureName = output.features().trimmed();
        const quint8 feature = featureName == QStringLiteral("Coinbase") ? 1 : 0;
        encoded.append(static_cast<char>(feature));
        if (!appendExactHexBytes(&encoded, output.commit(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid output commitment for v1 binary serialization.");
            }
            return QByteArray();
        }

        const QByteArray proof = QByteArray::fromHex(output.proof().trimmed().toUtf8());
        if (proof.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Missing output proof for v1 binary serialization.");
            }
            return QByteArray();
        }

        appendU64Network(&encoded, static_cast<quint64>(proof.size()));
        encoded.append(proof);
    }

    for (const TxKernel &kernel : kernels) {
        const QString featureName = kernel.features().trimmed();
        quint8 featureByte = 0;
        if (featureName.isEmpty() || featureName == QStringLiteral("Plain")) {
            featureByte = 0;
        } else if (featureName == QStringLiteral("Coinbase")) {
            featureByte = 1;
        } else {
            if (errorOut) {
                *errorOut = QStringLiteral("Unsupported kernel feature for v1 binary serialization: %1").arg(featureName);
            }
            return QByteArray();
        }

        encoded.append(static_cast<char>(featureByte));
        appendU64Network(&encoded, featureByte == 1 ? 0 : static_cast<quint64>(kernel.fee()));
        appendU64Network(&encoded, 0);

        if (!appendExactHexBytes(&encoded, kernel.excess(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid kernel excess for v1 binary serialization.");
            }
            return QByteArray();
        }
        if (!appendExactHexBytes(&encoded, kernel.excessSig(), 64)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid kernel signature for v1 binary serialization.");
            }
            return QByteArray();
        }
    }

    return encoded;
}

QList<QUrl> poolPushCandidateUrlsForApiUrl(const QString &apiUrl, bool fluff)
{
    QUrl url(apiUrl);
    QString path = url.path();
    if (path.endsWith(QStringLiteral("/v2/foreign"))) {
        path.chop(QStringLiteral("/v2/foreign").size());
    } else if (path.endsWith(QStringLiteral("/v2/foreign/"))) {
        path.chop(QStringLiteral("/v2/foreign/").size());
    }

    if (!path.endsWith('/')) {
        path.append('/');
    }

    const QString basePath = path;
    const QStringList relativeCandidates{
        QStringLiteral("v1/pool/push"),
        QStringLiteral("v1/pool/push/"),
        QStringLiteral("v1/pool/push_tx"),
        QStringLiteral("v1/pool/push_tx/")
    };

    QList<QUrl> urls;
    urls.reserve(relativeCandidates.size());
    for (const QString &candidate : relativeCandidates) {
        QUrl candidateUrl = url;
        candidateUrl.setPath(basePath + candidate);
        QUrlQuery query;
        if (fluff) {
            query.addQueryItem(QStringLiteral("fluff"), QString());
        }
        candidateUrl.setQuery(query);
        urls.append(candidateUrl);
    }
    return urls;
}

QString inputFeatureName(OutputFeatures::Feature feature)
{
    return feature == OutputFeatures::Coinbase
        ? QStringLiteral("Coinbase")
        : QStringLiteral("Plain");
}

QJsonObject serializeKernelFeatures(const TxKernel &kernel)
{
    QJsonObject featureArgs;
    if (kernel.features().isEmpty() || kernel.features() == QStringLiteral("Plain")) {
        featureArgs.insert(QStringLiteral("fee"), static_cast<qint64>(kernel.fee()));
    }

    QJsonObject features;
    features.insert(kernel.features().isEmpty() ? QStringLiteral("Plain") : kernel.features(), featureArgs);
    return features;
}

QJsonObject serializeTransactionForNode(const Transaction &tx)
{
    QJsonObject txJson;
    txJson.insert(QStringLiteral("offset"), tx.offset().hex());

    const TransactionBody body = tx.body();

    QJsonArray inputsJson;
    const QVector<Input> inputs = body.inputs();
    for (const Input &input : inputs) {
        QJsonObject inputJson;
        inputJson.insert(QStringLiteral("features"), inputFeatureName(input.features()));
        inputJson.insert(QStringLiteral("commit"), input.commit().hex());
        inputsJson.append(inputJson);
    }

    QJsonArray outputsJson;
    const QVector<Output> outputs = body.outputs();
    for (const Output &output : outputs) {
        QJsonObject outputJson;
        outputJson.insert(QStringLiteral("features"), output.features());
        outputJson.insert(QStringLiteral("commit"), output.commit());
        outputJson.insert(QStringLiteral("proof"), output.proof());
        outputsJson.append(outputJson);
    }

    QJsonArray kernelsJson;
    const QVector<TxKernel> kernels = body.kernels();
    for (const TxKernel &kernel : kernels) {
        QJsonObject kernelJson;
        kernelJson.insert(QStringLiteral("features"), serializeKernelFeatures(kernel));
        kernelJson.insert(QStringLiteral("excess"), kernel.excess());
        kernelJson.insert(QStringLiteral("excess_sig"), kernel.excessSig());
        kernelsJson.append(kernelJson);
    }

    QJsonObject bodyJson;
    bodyJson.insert(QStringLiteral("inputs"), inputsJson);
    bodyJson.insert(QStringLiteral("outputs"), outputsJson);
    bodyJson.insert(QStringLiteral("kernels"), kernelsJson);
    txJson.insert(QStringLiteral("body"), bodyJson);
    return txJson;
}

QJsonObject serializeTransactionForNodeLegacyKernel(const Transaction &tx)
{
    QJsonObject txJson;
    txJson.insert(QStringLiteral("offset"), tx.offset().hex());

    const TransactionBody body = tx.body();

    QJsonArray inputsJson;
    const QVector<Input> inputs = body.inputs();
    for (const Input &input : inputs) {
        QJsonObject inputJson;
        inputJson.insert(QStringLiteral("features"), inputFeatureName(input.features()));
        inputJson.insert(QStringLiteral("commit"), input.commit().hex());
        inputsJson.append(inputJson);
    }

    QJsonArray outputsJson;
    const QVector<Output> outputs = body.outputs();
    for (const Output &output : outputs) {
        QJsonObject outputJson;
        outputJson.insert(QStringLiteral("features"), output.features());
        outputJson.insert(QStringLiteral("commit"), output.commit());
        outputJson.insert(QStringLiteral("proof"), output.proof());
        outputsJson.append(outputJson);
    }

    QJsonArray kernelsJson;
    const QVector<TxKernel> kernels = body.kernels();
    for (const TxKernel &kernel : kernels) {
        QJsonObject kernelJson;
        const QString kernelFeature = kernel.features().isEmpty() ? QStringLiteral("Plain") : kernel.features();
        kernelJson.insert(QStringLiteral("features"), kernelFeature);
        kernelJson.insert(QStringLiteral("fee"), static_cast<qint64>(kernel.fee()));
        kernelJson.insert(QStringLiteral("excess"), kernel.excess());
        kernelJson.insert(QStringLiteral("excess_sig"), kernel.excessSig());
        kernelsJson.append(kernelJson);
    }

    QJsonObject bodyJson;
    bodyJson.insert(QStringLiteral("inputs"), inputsJson);
    bodyJson.insert(QStringLiteral("outputs"), outputsJson);
    bodyJson.insert(QStringLiteral("kernels"), kernelsJson);
    txJson.insert(QStringLiteral("body"), bodyJson);
    return txJson;
}

QString prettyJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void logRpcJson(const QString &label, const QJsonObject &object)
{
    Q_UNUSED(label)
    Q_UNUSED(object)
}

constexpr char kSecpOrderHex[] =
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141";

bool parseScalarHex(const QString &hex, std::array<unsigned char, 32> *out)
{
    if (!out) {
        return false;
    }
    const QByteArray raw = QByteArray::fromHex(hex.trimmed().toUtf8());
    if (raw.size() != 32) {
        return false;
    }
    for (int i = 0; i < 32; ++i) {
        (*out)[i] = static_cast<unsigned char>(raw.at(i));
    }
    return true;
}

bool isZeroScalar(const std::array<unsigned char, 32> &value)
{
    for (int i = 0; i < 32; ++i) {
        if (value[i] != 0) {
            return false;
        }
    }
    return true;
}

int compareScalars(const std::array<unsigned char, 32> &left,
                   const std::array<unsigned char, 32> &right)
{
    for (int i = 0; i < 32; ++i) {
        if (left[i] < right[i]) {
            return -1;
        }
        if (left[i] > right[i]) {
            return 1;
        }
    }
    return 0;
}

std::array<unsigned char, 32> secpOrderScalar()
{
    std::array<unsigned char, 32> order{};
    const QByteArray raw = QByteArray::fromHex(QByteArray(kSecpOrderHex));
    if (raw.size() == 32) {
        for (int i = 0; i < 32; ++i) {
            order[i] = static_cast<unsigned char>(raw.at(i));
        }
    }
    return order;
}

std::array<unsigned char, 32> subtractScalars(const std::array<unsigned char, 32> &left,
                                              const std::array<unsigned char, 32> &right)
{
    std::array<unsigned char, 32> result{};
    int borrow = 0;
    for (int i = 31; i >= 0; --i) {
        int value = static_cast<int>(left[i]) - static_cast<int>(right[i]) - borrow;
        if (value < 0) {
            value += 256;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = static_cast<unsigned char>(value);
    }
    return result;
}

std::array<unsigned char, 32> addScalarsModN(const std::array<unsigned char, 32> &left,
                                             const std::array<unsigned char, 32> &right)
{
    std::array<unsigned char, 32> sum{};
    int carry = 0;
    for (int i = 31; i >= 0; --i) {
        const int value = static_cast<int>(left[i]) + static_cast<int>(right[i]) + carry;
        sum[i] = static_cast<unsigned char>(value & 0xff);
        carry = value >> 8;
    }

    const std::array<unsigned char, 32> order = secpOrderScalar();
    if (carry > 0 || compareScalars(sum, order) >= 0) {
        sum = subtractScalars(sum, order);
    }
    return sum;
}

QString scalarToHex(const std::array<unsigned char, 32> &value)
{
    QByteArray raw(32, Qt::Uninitialized);
    for (int i = 0; i < 32; ++i) {
        raw[i] = static_cast<char>(value[i]);
    }
    return QString::fromUtf8(raw.toHex());
}

bool isValidSecpScalar(const std::array<unsigned char, 32> &value)
{
    const std::array<unsigned char, 32> order = secpOrderScalar();
    return !isZeroScalar(value) && compareScalars(value, order) < 0;
}

secp256k1_context *diagSecpContext()
{
    static secp256k1_context *context = nullptr;
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        context = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    });
    return context;
}

bool secpSecretValid(const std::array<unsigned char, 32> &value)
{
    secp256k1_context *context = diagSecpContext();
    if (!context) {
        return false;
    }
    return secp256k1_ec_seckey_verify(context, value.data()) == 1;
}

bool secpAddOffsets(const std::array<unsigned char, 32> &headerOffset,
                    const std::array<unsigned char, 32> &txOffset,
                    std::array<unsigned char, 32> *combinedOut)
{
    if (!combinedOut) {
        return false;
    }
    secp256k1_context *context = diagSecpContext();
    if (!context) {
        return false;
    }

    std::array<unsigned char, 32> combined = headerOffset;
    const int rc = secp256k1_ec_privkey_tweak_add(context, combined.data(), txOffset.data());
    if (rc != 1) {
        return false;
    }

    *combinedOut = combined;
    return true;
}

bool isLowerHex(const QString &value)
{
    if (value.isEmpty()) {
        return false;
    }
    for (QChar ch : value) {
        const ushort c = ch.unicode();
        const bool isDigit = c >= '0' && c <= '9';
        const bool isLower = c >= 'a' && c <= 'f';
        if (!isDigit && !isLower) {
            return false;
        }
    }
    return true;
}

void appendU64Be(QByteArray &out, quint64 value)
{
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

void appendU64Le(QByteArray &out, quint64 value)
{
    for (int shift = 0; shift <= 56; shift += 8) {
        out.append(static_cast<char>((value >> shift) & 0xff));
    }
}

void logPushPayloadDiagnostics(const QJsonObject &txObj)
{
    Q_UNUSED(txObj)
}

void logOffsetDiagnostics(const QString &txOffsetHex,
                          const QString &headerOffsetHex,
                          quint64 headerHeight)
{
    Q_UNUSED(txOffsetHex)
    Q_UNUSED(headerOffsetHex)
    Q_UNUSED(headerHeight)
}

void logKernelCommitmentDiagnostics(const Transaction &tx)
{
    Q_UNUSED(tx)
}

void logKernelExcessValidation(const Transaction &tx)
{
    Q_UNUSED(tx)
}

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
    logRpcJson(QStringLiteral("request method=%1").arg(method), payload);

    QNetworkReply *reply = m_networkManager->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [reply, handler, method]() {
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
        logRpcJson(QStringLiteral("response method=%1").arg(method), doc.object());
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

    const QJsonObject txObj = serializeTransactionForNode(tx);
    const std::shared_ptr<PushDiagContext> diagCtx = std::make_shared<PushDiagContext>();
    diagCtx->txOffset = txObj.value(QStringLiteral("offset")).toString();

    logPushPayloadDiagnostics(txObj);

    const QJsonObject legacyKernelTxObj = serializeTransactionForNodeLegacyKernel(tx);

    QJsonArray params;
    params << txObj << fluff;

    const QString txOffset = txObj.value(QStringLiteral("offset")).toString();

    const auto logPrePushKernelDiagnostics = [this, &tx]() {
        Q_UNUSED(tx)
    };

    logPrePushKernelDiagnostics();
    logKernelCommitmentDiagnostics(tx);
    logKernelExcessValidation(tx);

    const auto logKernelSignatureDiag = [this, &tx]() {
        Q_UNUSED(tx)
    };
    logKernelSignatureDiag();

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
                    const QByteArray txBytes = serializeTransactionForPoolV1(Transaction::fromJson(txObj), &serializeError);
                    if (txBytes.isEmpty()) {
                        continueAfterBinaryFallback();
                        return;
                    }

                    const QList<QUrl> restUrls = poolPushCandidateUrlsForApiUrl(m_apiUrl, fluff);
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

            logOffsetDiagnostics(txOffset, header.value().totalKernelOffset(), tipHeight);
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

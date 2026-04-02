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
    qDebug().noquote() << QStringLiteral("[NodeRpcJson] %1\n%2").arg(label, prettyJson(object));
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
    const QString offset = txObj.value(QStringLiteral("offset")).toString();
    const QJsonObject body = txObj.value(QStringLiteral("body")).toObject();
    const QJsonArray inputs = body.value(QStringLiteral("inputs")).toArray();
    const QJsonArray outputs = body.value(QStringLiteral("outputs")).toArray();
    const QJsonArray kernels = body.value(QStringLiteral("kernels")).toArray();

    qDebug() << "[NodePushTxFieldDiag]"
             << "offsetLen=" << offset.size()
             << "offsetHex=" << isLowerHex(offset)
             << "inputs=" << inputs.size()
             << "outputs=" << outputs.size()
             << "kernels=" << kernels.size();

    for (int i = 0; i < inputs.size(); ++i) {
        const QJsonObject in = inputs.at(i).toObject();
        const QString commit = in.value(QStringLiteral("commit")).toString();
        qDebug() << "[NodePushTxFieldDiag] input"
                 << i
                 << "features=" << in.value(QStringLiteral("features")).toString()
                 << "commitLen=" << commit.size()
                 << "commitHex=" << isLowerHex(commit);
    }

    for (int i = 0; i < outputs.size(); ++i) {
        const QJsonObject out = outputs.at(i).toObject();
        const QString commit = out.value(QStringLiteral("commit")).toString();
        const QString proof = out.value(QStringLiteral("proof")).toString();
        qDebug() << "[NodePushTxFieldDiag] output"
                 << i
                 << "features=" << out.value(QStringLiteral("features")).toString()
                 << "commitLen=" << commit.size()
                 << "commitHex=" << isLowerHex(commit)
                 << "proofLen=" << proof.size()
                 << "proofHex=" << isLowerHex(proof);
    }

    for (int i = 0; i < kernels.size(); ++i) {
        const QJsonObject kernel = kernels.at(i).toObject();
        const QString excess = kernel.value(QStringLiteral("excess")).toString();
        const QString excessSig = kernel.value(QStringLiteral("excess_sig")).toString();
        const QJsonObject features = kernel.value(QStringLiteral("features")).toObject();
        qDebug() << "[NodePushTxFieldDiag] kernel"
                 << i
                 << "featureKeys=" << features.keys().join(QStringLiteral(","))
                 << "excessLen=" << excess.size()
                 << "excessHex=" << isLowerHex(excess)
                 << "sigLen=" << excessSig.size()
                 << "sigHex=" << isLowerHex(excessSig);
    }
}

void logOffsetDiagnostics(const QString &txOffsetHex,
                          const QString &headerOffsetHex,
                          quint64 headerHeight)
{
    std::array<unsigned char, 32> txOffset{};
    std::array<unsigned char, 32> headerOffset{};
    if (!parseScalarHex(txOffsetHex, &txOffset) || !parseScalarHex(headerOffsetHex, &headerOffset)) {
        qWarning() << "[NodePushTxOffsetDiag]"
                   << "height=" << headerHeight
                   << "error=failed to parse tx/header offsets"
                   << "txOffset=" << txOffsetHex
                   << "headerOffset=" << headerOffsetHex;
        return;
    }

    const std::array<unsigned char, 32> combined = addScalarsModN(headerOffset, txOffset);
    const bool txOffsetMathValid = isValidSecpScalar(txOffset);
    const bool headerOffsetMathValid = isValidSecpScalar(headerOffset);
    const bool txOffsetSecpValid = secpSecretValid(txOffset);
    const bool headerOffsetSecpValid = secpSecretValid(headerOffset);

    std::array<unsigned char, 32> secpCombined{};
    const bool secpAddOk = secpAddOffsets(headerOffset, txOffset, &secpCombined);

    qDebug() << "[NodePushTxOffsetDiag]"
             << "height=" << headerHeight
             << "txOffsetMathValid=" << txOffsetMathValid
             << "headerOffsetMathValid=" << headerOffsetMathValid
             << "txOffsetSecpValid=" << txOffsetSecpValid
             << "headerOffsetSecpValid=" << headerOffsetSecpValid
             << "combinedOffset=" << scalarToHex(combined)
             << "combinedIsZero=" << isZeroScalar(combined)
             << "combinedMathValid=" << isValidSecpScalar(combined)
             << "secpAddOk=" << secpAddOk
             << "secpCombinedOffset=" << (secpAddOk ? scalarToHex(secpCombined) : QStringLiteral("<invalid>"))
             << "secpCombinedIsZero=" << (secpAddOk ? isZeroScalar(secpCombined) : true)
             << "secpCombinedValid=" << (secpAddOk ? secpSecretValid(secpCombined) : false)
             << "secpMatchesMath=" << (secpAddOk ? (secpCombined == combined) : false);
}

void logKernelCommitmentDiagnostics(const Transaction &tx)
{
    const TransactionBody body = tx.body();
    const QVector<Input> inputs = body.inputs();
    const QVector<Output> outputs = body.outputs();
    const QVector<TxKernel> kernels = body.kernels();

    // Log input/output/kernel structure (commitments are Pedersen, not EC pubkeys)
    // Validation of commitment arithmetic is done by the node, not here
    
    for (int i = 0; i < inputs.size(); ++i) {
        const QString commitHex = inputs.at(i).commit().hex();
        qDebug() << "[NodePushTxKernelStructDiag] input"
                 << i
                 << "commitLen=" << commitHex.size()
                 << "commitStart=" << commitHex.left(6)
                 << "isHex=" << isLowerHex(commitHex)
                 << "feature=" << (inputs.at(i).features() == OutputFeatures::Coinbase ? "Coinbase" : "Plain");
    }

    for (int i = 0; i < outputs.size(); ++i) {
        const QString commitHex = outputs.at(i).commit();
        const QString proofHex = outputs.at(i).proof();
        qDebug() << "[NodePushTxKernelStructDiag] output"
                 << i
                 << "commitLen=" << commitHex.size()
                 << "commitStart=" << commitHex.left(6)
                 << "commitHex=" << isLowerHex(commitHex)
                 << "proofLen=" << proofHex.size()
                 << "proofHex=" << isLowerHex(proofHex)
                 << "feature=" << outputs.at(i).features();
    }

    for (int i = 0; i < kernels.size(); ++i) {
        const TxKernel &kernel = kernels.at(i);
        const QString excessHex = kernel.excess();
        const QString sigHex = kernel.excessSig();
        
        // Check kernel structure
        const bool excessFormatValid = excessHex.size() == 66 && isLowerHex(excessHex);
        const bool sigFormatValid = sigHex.size() == 128 && isLowerHex(sigHex);
        const bool feeValid = kernel.fee() > 0;
        
        // Excess should start with 08 or 09 (compressed point prefix)
        const bool excessPointFormatValid = excessHex.startsWith("08") || excessHex.startsWith("09");
        
        // Signature should be two 64-byte (128-hex-char) scalars: r || s
        const QString sigR = sigHex.mid(0, 64);
        const QString sigS = sigHex.mid(64, 64);
        const bool sigRValid = sigR.size() == 64 && isLowerHex(sigR);
        const bool sigSValid = sigS.size() == 64 && isLowerHex(sigS);
        
        qDebug() << "[NodePushTxKernelStructDiag] kernel"
                 << i
                 << "excessLen=" << excessHex.size()
                 << "excessStart=" << excessHex.left(6)
                 << "excessHex=" << isLowerHex(excessHex)
                 << "excessPointFormat=" << excessPointFormatValid
                 << "sigLen=" << sigHex.size()
                 << "sigStart=" << sigHex.left(6)
                 << "sigHex=" << isLowerHex(sigHex)
                 << "sigRValid=" << sigRValid
                 << "sigSValid=" << sigSValid
                 << "fee=" << kernel.fee()
                 << "feeValid=" << feeValid
                 << "excessFormatOk=" << excessFormatValid
                 << "sigFormatOk=" << sigFormatValid
                 << "feature=" << kernel.features();
    }

    qDebug() << "[NodePushTxKernelStructDiag]"
             << "inputsCount=" << inputs.size()
             << "outputsCount=" << outputs.size()
             << "kernelsCount=" << kernels.size();
}

void logKernelExcessValidation(const Transaction &tx)
{
    // Kernel excess is a PEDERSEN COMMITMENT (secp256k1_pedersen_commitment_serialize),
    // not a regular EC point. Prefix 08/09 = Pedersen, 02/03 = regular EC point.
    
    const TransactionBody body = tx.body();
    const QVector<TxKernel> kernels = body.kernels();
    
    if (kernels.empty()) {
        qDebug() << "[NodePushTxKernelExcessDiag] no kernels";
        return;
    }

    const TxKernel &kernel = kernels.at(0);
    const QString excessHex = kernel.excess();
    const QByteArray excessBytes = QByteArray::fromHex(excessHex.toUtf8());

    // Validate excess is 33 bytes (Pedersen commitment)
    bool excessSizeOk = (excessBytes.size() == 33);
    unsigned char prefixByte = excessSizeOk ? static_cast<unsigned char>(excessBytes[0]) : 0;
    
    // Pedersen commitment prefix: 08 (even) or 09 (odd)
    // Regular EC point prefix: 02 (even) or 03 (odd)
    bool isPedessenCommitment = (prefixByte == 0x08 || prefixByte == 0x09);
    bool isRegularPoint = (prefixByte == 0x02 || prefixByte == 0x03);

    qDebug() << "[NodePushTxKernelExcessDiag]"
             << "excessStart=" << excessHex.left(16)
             << "len=" << excessBytes.size()
             << "prefixByte=" << QString::number(prefixByte, 16)
             << "isPedessenCommitment=" << isPedessenCommitment
             << "isRegularPoint=" << isRegularPoint
             << "hasValidPrefix=" << (isPedessenCommitment || isRegularPoint);
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
            qWarning() << "[NodeRpcJson] response error" << "method=" << method << "error=" << err;
            reply->deleteLater();
            handler(QJsonObject{}, err);
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning().noquote() << QStringLiteral("[NodeRpcJson] response parse error method=%1 error=%2 raw=\n%3")
                                        .arg(method,
                                             pe.errorString(),
                                             QString::fromUtf8(data));
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
            emit kernelLookupFailed(parsed.errorMessage());
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
    qDebug() << "[NodeForeignApi] get_output_commitments request"
             << "commitCount=" << commits.size();
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
    qDebug() << "[NodeForeignApi] get_pmmr_indices request"
             << "startHeight=" << startHeight
             << "endHeight=" << endHeight;
    postAsync("get_pmmr_indices", params, [this](const QJsonObject &obj, const QString &err) {
        if (!err.isEmpty()) {
            qDebug() << "[NodeForeignApi] get_pmmr_indices error:" << err;
            emit getPmmrIndicesFinished(Result<OutputListing>::error(err));
            emit pmmrIndicesLookupFailed(err);
            return;
        }
        qDebug() << "[NodeForeignApi] get_pmmr_indices raw response:" << obj;
        auto r = parseOutputListing(obj);
        emit getPmmrIndicesFinished(r);
        if (r.hasError()) {
            qDebug() << "[NodeForeignApi] get_pmmr_indices parse error:" << r.errorMessage();
            emit pmmrIndicesLookupFailed(r.errorMessage());
            return;
        }

        qDebug() << "[NodeForeignApi] get_pmmr_indices parsed"
                 << "highestIndex=" << r.value().highestIndex()
                 << "lastRetrievedIndex=" << r.value().lastRetrievedIndex()
                 << "outputs=" << r.value().outputs().size();

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
                qWarning() << "[NodeForeignApi] get_unspent_outputs parse failed:" << r.errorMessage()
                           << "raw:" << obj;
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
            qWarning() << "[NodeForeignApi] get_version network/error:" << err;
            emit getVersionFinished(Result<NodeVersion>::error(err));
            return;
        }

        auto parsed = parseNodeVersion(obj);
        if (parsed.hasError()) {
            qWarning() << "[NodeForeignApi] get_version parse error:" << parsed.errorMessage();
        }

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

    qDebug() << "[NodePushTx]"
             << "fluff=" << fluff
             << "offset=" << txObj.value(QStringLiteral("offset")).toString()
             << "inputs=" << txObj.value(QStringLiteral("body")).toObject().value(QStringLiteral("inputs")).toArray().size()
             << "outputs=" << txObj.value(QStringLiteral("body")).toObject().value(QStringLiteral("outputs")).toArray().size()
             << "kernels=" << txObj.value(QStringLiteral("body")).toObject().value(QStringLiteral("kernels")).toArray().size();
    logPushPayloadDiagnostics(txObj);
    qDebug().noquote() << QStringLiteral("[NodePushTx] tx-json\n%1").arg(prettyJson(tx.toJson()));
    qDebug().noquote() << QStringLiteral("[NodePushTx] payload\n%1").arg(prettyJson(txObj));

    const QJsonObject legacyKernelTxObj = serializeTransactionForNodeLegacyKernel(tx);

    QJsonArray params;
    params << txObj << fluff;

    const QString txOffset = txObj.value(QStringLiteral("offset")).toString();

    const auto logPrePushKernelDiagnostics = [this, &tx]() {
        const QVector<TxKernel> kernels = tx.body().kernels();
        qDebug() << "[NodePushTxKernelPrediag]"
                 << "kernelCount=" << kernels.size()
                 << "input1Commit=" << (tx.body().inputs().size() > 0 ? tx.body().inputs().at(0).commit().hex().left(16) : QStringLiteral("<none>"))
                 << "output1Commit=" << (tx.body().outputs().size() > 0 ? tx.body().outputs().at(0).commit().left(16) : QStringLiteral("<none>"));
        for (int i = 0; i < kernels.size(); ++i) {
            const TxKernel &kernel = kernels.at(i);
            qDebug() << "[NodePushTxKernelPrediag] kernel"
                     << i
                     << "excess=" << kernel.excess().left(16)
                     << "excess_sig=" << kernel.excessSig().left(16)
                     << "fee=" << kernel.fee()
                     << "features=" << kernel.features();
        }
    };

    logPrePushKernelDiagnostics();
    logKernelCommitmentDiagnostics(tx);
    logKernelExcessValidation(tx);

    const auto logKernelSignatureDiag = [this, &tx]() {
        const QVector<TxKernel> kernels = tx.body().kernels();
        for (int i = 0; i < kernels.size(); ++i) {
            const TxKernel &kernel = kernels.at(i);
            const QString excess = kernel.excess();
            const QString sig = kernel.excessSig();
            const qint64 fee = kernel.fee();
            const QString features = kernel.features();
            
            // Kernel message should be: feature_byte || fee_u64_le (for Plain features)
            // Then blake2b-256 hashed
            quint8 featureByte = 0;  // Plain
            if (features == "Coinbase") featureByte = 1;
            else if (features == "HeightLocked") featureByte = 2;

            QByteArray serializedBe;
            QByteArray serializedLe;
            serializedBe.append(static_cast<char>(featureByte));
            serializedLe.append(static_cast<char>(featureByte));
            if (featureByte != 1) {
                appendU64Be(serializedBe, static_cast<quint64>(fee));
                appendU64Le(serializedLe, static_cast<quint64>(fee));
            }

            const QByteArray messageHashBe = QCryptographicHash::hash(serializedBe, QCryptographicHash::Blake2b_256);
            const QByteArray messageHashLe = QCryptographicHash::hash(serializedLe, QCryptographicHash::Blake2b_256);

            bool excessParseOk = false;
            bool excessToPubkeyOk = false;
            bool localVerifyBE = false;
            bool localVerifyLE = false;
            bool localVerifyBE_NoPubkeyTotal = false;
            bool localVerifyLE_NoPubkeyTotal = false;

            const QByteArray excessBytes = QByteArray::fromHex(excess.toUtf8());
            const QByteArray sigBytes = QByteArray::fromHex(sig.toUtf8());
            if (excessBytes.size() == 33 && sigBytes.size() == 64) {
                secp256k1_pedersen_commitment excessCommitment;
                excessParseOk = secp256k1_pedersen_commitment_parse(
                    diagSecpContext(),
                    &excessCommitment,
                    reinterpret_cast<const unsigned char *>(excessBytes.constData())) == 1;

                if (excessParseOk) {
                    secp256k1_pubkey excessPubkey;
                    excessToPubkeyOk = secp256k1_pedersen_commitment_to_pubkey(
                        diagSecpContext(),
                        &excessPubkey,
                        &excessCommitment) == 1;

                    if (excessToPubkeyOk) {
                        localVerifyBE = secp256k1_aggsig_verify_single(
                            diagSecpContext(),
                            reinterpret_cast<const unsigned char *>(sigBytes.constData()),
                            reinterpret_cast<const unsigned char *>(messageHashBe.constData()),
                            0,
                            &excessPubkey,
                            &excessPubkey,
                            0,
                            0) == 1;

                        localVerifyLE = secp256k1_aggsig_verify_single(
                            diagSecpContext(),
                            reinterpret_cast<const unsigned char *>(sigBytes.constData()),
                            reinterpret_cast<const unsigned char *>(messageHashLe.constData()),
                            0,
                            &excessPubkey,
                            &excessPubkey,
                            0,
                            0) == 1;

                        localVerifyBE_NoPubkeyTotal = secp256k1_aggsig_verify_single(
                            diagSecpContext(),
                            reinterpret_cast<const unsigned char *>(sigBytes.constData()),
                            reinterpret_cast<const unsigned char *>(messageHashBe.constData()),
                            0,
                            &excessPubkey,
                            0,
                            0,
                            0) == 1;

                        localVerifyLE_NoPubkeyTotal = secp256k1_aggsig_verify_single(
                            diagSecpContext(),
                            reinterpret_cast<const unsigned char *>(sigBytes.constData()),
                            reinterpret_cast<const unsigned char *>(messageHashLe.constData()),
                            0,
                            &excessPubkey,
                            0,
                            0,
                            0) == 1;
                    }
                }
            }
            
            // Extract R and S from signature (r || s format, 64 hex chars each)
            const QString rHex = sig.mid(0, 64);
            const QString sHex = sig.mid(64, 64);
            
            qDebug() << "[NodePushTxKernelSigDiag] kernel"
                     << i
                     << "feature=" << features
                     << "featureByte=" << static_cast<int>(featureByte)
                     << "feeNanogrin=" << fee
                     << "msgHashBE=" << QString::fromUtf8(messageHashBe.toHex())
                     << "msgHashLE=" << QString::fromUtf8(messageHashLe.toHex())
                     << "excessParseOk=" << excessParseOk
                     << "excessToPubkeyOk=" << excessToPubkeyOk
                     << "localVerifyBE=" << localVerifyBE
                     << "localVerifyLE=" << localVerifyLE
                     << "localVerifyBE_NoPubkeyTotal=" << localVerifyBE_NoPubkeyTotal
                     << "localVerifyLE_NoPubkeyTotal=" << localVerifyLE_NoPubkeyTotal
                     << "excessStart=" << excess.left(16)
                     << "sigRStart=" << rHex.left(16)
                     << "sigSStart=" << sHex.left(16);
        }
    };
    logKernelSignatureDiag();

    const auto sendPushTransaction = [this, params, diagCtx, txObj, fluff, legacyKernelTxObj]() {
        postAsync("push_transaction", params, [this, params, diagCtx, txObj, fluff, legacyKernelTxObj](const QJsonObject &obj, const QString &err) {
            if (!err.isEmpty()) {
                qWarning() << "[NodePushTx] network/error=" << err;
                emit pushTransactionFinished(Result<bool>::error(err));
                return;
            }
            qDebug().noquote() << QStringLiteral("[NodePushTx] rpcResponse\n%1").arg(prettyJson(obj));
            const Result<bool> parsed = parseBoolResult(obj);
            if (parsed.hasError()) {
                qWarning() << "[NodePushTx] rpc parse/error=" << parsed.errorMessage();
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

                // Some node builds expose validate_tx on a different API surface.
                // If available, this can provide a more specific rejection reason than push_transaction.
                {
                    QJsonArray validateParams;
                    validateParams << txObj;
                    postAsync("validate_tx", validateParams,
                              [this](const QJsonObject &validateObj, const QString &validateErr) {
                        if (!validateErr.isEmpty()) {
                            qWarning() << "[NodePushTxValidateDiag] validate_tx error=" << validateErr;
                            return;
                        }
                        qWarning().noquote() << QStringLiteral("[NodePushTxValidateDiag] validate_tx response\n%1")
                                                     .arg(prettyJson(validateObj));
                    });
                }

                const QJsonObject bodyObj = txObj.value(QStringLiteral("body")).toObject();
                const QJsonArray outputs = bodyObj.value(QStringLiteral("outputs")).toArray();
                QJsonArray outputCommits;
                for (const QJsonValue &outputValue : outputs) {
                    const QJsonObject outputObj = outputValue.toObject();
                    const QString commit = outputObj.value(QStringLiteral("commit")).toString();
                    if (!commit.isEmpty()) {
                        outputCommits.append(commit);
                    }
                }

                if (!outputCommits.isEmpty()) {
                    QJsonArray outputLookupParams;
                    outputLookupParams << outputCommits
                                       << QJsonValue(QJsonValue::Null)
                                       << QJsonValue(QJsonValue::Null)
                                       << false
                                       << false;
                    postAsync("get_outputs", outputLookupParams,
                              [this, outputCommits](const QJsonObject &outObj, const QString &outErr) {
                        if (!outErr.isEmpty()) {
                            qWarning() << "[NodePushTxPostErrorDiag] output lookup error=" << outErr;
                            return;
                        }

                        const Result<QList<OutputPrintable> > parsedOutputs = parseOutputPrintableList(outObj);
                        if (parsedOutputs.hasError()) {
                            qWarning() << "[NodePushTxPostErrorDiag] output lookup parse/error=" << parsedOutputs.errorMessage();
                            return;
                        }

                        qWarning() << "[NodePushTxPostErrorDiag]"
                                   << "requestedOutputCommits=" << outputCommits.size()
                                   << "existingOutputs=" << parsedOutputs.value().size();
                        const QList<OutputPrintable> existingOutputs = parsedOutputs.value();
                        for (int i = 0; i < existingOutputs.size(); ++i) {
                            const OutputPrintable &existing = existingOutputs.at(i);
                            qWarning() << "[NodePushTxPostErrorDiag] existingOutput"
                                       << i
                                       << "commit=" << existing.commit().hex()
                                       << "spent=" << existing.spent()
                                       << "height=" << existing.blockHeight()
                                       << "type=" << static_cast<int>(existing.outputType());
                        }
                    });
                }

                const QJsonArray kernels = bodyObj.value(QStringLiteral("kernels")).toArray();
                if (!kernels.isEmpty()) {
                    const QString excess = kernels.at(0).toObject().value(QStringLiteral("excess")).toString();
                    if (!excess.isEmpty()) {
                        QJsonArray kernelLookupParams;
                        kernelLookupParams << excess
                                           << QJsonValue(QJsonValue::Null)
                                           << QJsonValue(QJsonValue::Null);
                        postAsync("get_kernel", kernelLookupParams,
                                  [excess](const QJsonObject &kernelObj, const QString &kernelErr) {
                            if (!kernelErr.isEmpty()) {
                                qWarning() << "[NodePushTxPostErrorDiag] kernel lookup error=" << kernelErr;
                                return;
                            }

                            qWarning().noquote() << QStringLiteral("[NodePushTxPostErrorDiag] kernel lookup excess=%1 response=\n%2")
                                                         .arg(excess.left(16), prettyJson(kernelObj));
                        });
                    }
                }

                postAsync("get_tip", QJsonArray{}, [this, diagCtx](const QJsonObject &tipObj, const QString &tipErr) {
                    if (!tipErr.isEmpty()) {
                        qWarning() << "[NodePushTxPostErrorDiag] get_tip error=" << tipErr;
                        return;
                    }
                    const Result<Tip> postTip = parseTipResult(tipObj);
                    if (postTip.hasError()) {
                        qWarning() << "[NodePushTxPostErrorDiag] get_tip parse/error=" << postTip.errorMessage();
                        return;
                    }

                    const quint64 postHeight = postTip.value().height();
                    const QString postHash = postTip.value().lastBlockPushed();
                    qWarning() << "[NodePushTxPostErrorDiag]"
                               << "preHeight=" << diagCtx->preTipHeight
                               << "postHeight=" << postHeight
                               << "preHash=" << diagCtx->preTipHash
                               << "postHash=" << postHash
                               << "tipChanged=" << (diagCtx->preTipHeight != postHeight || diagCtx->preTipHash != postHash);

                    QJsonArray headerParams;
                    headerParams << static_cast<qint64>(postHeight)
                                 << QJsonValue(QJsonValue::Null)
                                 << QJsonValue(QJsonValue::Null);
                    postAsync("get_header", headerParams, [this, diagCtx, postHeight](const QJsonObject &headerObj,
                                                                                        const QString &headerErr) {
                        if (!headerErr.isEmpty()) {
                            qWarning() << "[NodePushTxPostErrorDiag] get_header error=" << headerErr;
                            return;
                        }
                        const Result<BlockHeaderPrintable> header = this->parseBlockHeaderPrintable(headerObj);
                        if (header.hasError()) {
                            qWarning() << "[NodePushTxPostErrorDiag] get_header parse/error=" << header.errorMessage();
                            return;
                        }
                        logOffsetDiagnostics(diagCtx->txOffset, header.value().totalKernelOffset(), postHeight);
                    });
                });

                postAsync("get_unconfirmed_transactions", QJsonArray{},
                          [this, bodyObj](const QJsonObject &poolObj, const QString &poolErr) {
                    if (!poolErr.isEmpty()) {
                        qWarning() << "[NodePushTxPostErrorDiag] get_unconfirmed_transactions error=" << poolErr;
                        return;
                    }

                    const Result<QList<PoolEntry> > poolEntries = parsePoolEntries(poolObj);
                    if (poolEntries.hasError()) {
                        qWarning() << "[NodePushTxPostErrorDiag] get_unconfirmed_transactions parse/error="
                                   << poolEntries.errorMessage();
                        return;
                    }

                    const QJsonArray inputArray = bodyObj.value(QStringLiteral("inputs")).toArray();
                    const QJsonArray outputArray = bodyObj.value(QStringLiteral("outputs")).toArray();
                    const QJsonArray kernelArray = bodyObj.value(QStringLiteral("kernels")).toArray();

                    QSet<QString> ourInputCommits;
                    QSet<QString> ourOutputCommits;
                    for (const QJsonValue &inputValue : inputArray) {
                        const QString commit = inputValue.toObject().value(QStringLiteral("commit")).toString().trimmed().toLower();
                        if (!commit.isEmpty()) {
                            ourInputCommits.insert(commit);
                        }
                    }
                    for (const QJsonValue &outputValue : outputArray) {
                        const QString commit = outputValue.toObject().value(QStringLiteral("commit")).toString().trimmed().toLower();
                        if (!commit.isEmpty()) {
                            ourOutputCommits.insert(commit);
                        }
                    }
                    const QString ourKernelExcess = kernelArray.isEmpty()
                        ? QString()
                        : kernelArray.at(0).toObject().value(QStringLiteral("excess")).toString().trimmed().toLower();

                    int inputConflicts = 0;
                    int outputConflicts = 0;
                    int kernelConflicts = 0;
                    int loggedDetails = 0;

                    const QList<PoolEntry> entries = poolEntries.value();
                    for (int i = 0; i < entries.size(); ++i) {
                        const Transaction poolTx = entries.at(i).tx();
                        const TransactionBody poolBody = poolTx.body();
                        bool hasInputConflict = false;
                        bool hasOutputConflict = false;
                        bool hasKernelConflict = false;

                        const QVector<Input> poolInputs = poolBody.inputs();
                        for (const Input &input : poolInputs) {
                            const QString commit = input.commit().hex().trimmed().toLower();
                            if (ourInputCommits.contains(commit)) {
                                hasInputConflict = true;
                                break;
                            }
                        }

                        const QVector<Output> poolOutputs = poolBody.outputs();
                        for (const Output &output : poolOutputs) {
                            const QString commit = output.commit().trimmed().toLower();
                            if (ourOutputCommits.contains(commit)) {
                                hasOutputConflict = true;
                                break;
                            }
                        }

                        if (!ourKernelExcess.isEmpty()) {
                            const QVector<TxKernel> poolKernels = poolBody.kernels();
                            for (const TxKernel &kernel : poolKernels) {
                                if (kernel.excess().trimmed().toLower() == ourKernelExcess) {
                                    hasKernelConflict = true;
                                    break;
                                }
                            }
                        }

                        if (hasInputConflict) {
                            ++inputConflicts;
                        }
                        if (hasOutputConflict) {
                            ++outputConflicts;
                        }
                        if (hasKernelConflict) {
                            ++kernelConflicts;
                        }

                        if ((hasInputConflict || hasOutputConflict || hasKernelConflict) && loggedDetails < 5) {
                            ++loggedDetails;
                            qWarning() << "[NodePushTxPostErrorDiag] unconfirmed conflict"
                                       << "index=" << i
                                       << "src=" << static_cast<int>(entries.at(i).src())
                                       << "inputConflict=" << hasInputConflict
                                       << "outputConflict=" << hasOutputConflict
                                       << "kernelConflict=" << hasKernelConflict
                                       << "txInputs=" << poolInputs.size()
                                       << "txOutputs=" << poolOutputs.size()
                                       << "txKernels=" << poolBody.kernels().size();
                        }
                    }

                    qWarning() << "[NodePushTxPostErrorDiag]"
                               << "unconfirmedEntries=" << entries.size()
                               << "inputConflicts=" << inputConflicts
                               << "outputConflicts=" << outputConflicts
                               << "kernelConflicts=" << kernelConflicts;
                });

                if (shouldRetryAsStem) {
                    qWarning() << "[NodePushTxRetryStem] retrying push_transaction with fluff=false after keychain-style rejection";
                    QJsonArray stemParams = params;
                    if (stemParams.size() >= 2) {
                        stemParams[1] = false;
                    }
                    postAsync("push_transaction", stemParams, [this, parsed, legacyKernelTxObj](const QJsonObject &retryObj, const QString &retryErr) {
                        if (!retryErr.isEmpty()) {
                            qWarning() << "[NodePushTxRetryStem] network/error=" << retryErr;
                            emit pushTransactionFinished(parsed);
                            return;
                        }

                        qDebug().noquote() << QStringLiteral("[NodePushTxRetryStem] rpcResponse\n%1").arg(prettyJson(retryObj));
                        const Result<bool> retryParsed = parseBoolResult(retryObj);
                        if (retryParsed.hasError()) {
                            qWarning() << "[NodePushTxRetryStem] rpc parse/error=" << retryParsed.errorMessage();

                            const bool shouldTryLegacyKernelPayload = retryParsed.errorMessage().contains(QStringLiteral("keychain"), Qt::CaseInsensitive);
                            if (!shouldTryLegacyKernelPayload) {
                                emit pushTransactionFinished(parsed);
                                return;
                            }

                            qWarning() << "[NodePushTxRetryLegacyKernel] retrying push_transaction with legacy kernel JSON fields";
                            QJsonArray legacyStemParams;
                            legacyStemParams << legacyKernelTxObj << false;
                            postAsync("push_transaction", legacyStemParams,
                                      [this, parsed](const QJsonObject &legacyObj, const QString &legacyErr) {
                                if (!legacyErr.isEmpty()) {
                                    qWarning() << "[NodePushTxRetryLegacyKernel] network/error=" << legacyErr;
                                    emit pushTransactionFinished(parsed);
                                    return;
                                }

                                qDebug().noquote() << QStringLiteral("[NodePushTxRetryLegacyKernel] rpcResponse\n%1")
                                                          .arg(prettyJson(legacyObj));
                                const Result<bool> legacyParsed = parseBoolResult(legacyObj);
                                if (legacyParsed.hasError()) {
                                    qWarning() << "[NodePushTxRetryLegacyKernel] rpc parse/error=" << legacyParsed.errorMessage();
                                    emit pushTransactionFinished(parsed);
                                    return;
                                }

                                qWarning() << "[NodePushTxRetryLegacyKernel] success with legacy kernel JSON payload";
                                emit pushTransactionFinished(legacyParsed);
                            });
                            return;
                        }

                        qWarning() << "[NodePushTxRetryStem] success with fluff=false";
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
                        qWarning() << "[NodePushTxBinaryFallback] serialization error=" << serializeError;
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
                        qWarning() << "[NodePushTxBinaryFallback] trying REST candidate"
                                   << (*restIndex + 1) << "/" << restUrls.size()
                                   << "url=" << restUrl.toString()
                                   << "bytes=" << txBytes.size();

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
                                qWarning() << "[NodePushTxBinaryFallback] success"
                                           << "status=" << statusCode
                                           << "url=" << attemptedUrl;
                                emit pushTransactionFinished(Result<bool>(true));
                                return;
                            }

                            qWarning().noquote() << QStringLiteral("[NodePushTxBinaryFallback] failed status=%1 error=%2 url=%3 body=\n%4")
                                                        .arg(QString::number(statusCode),
                                                             errorString,
                                                             attemptedUrl,
                                                             QString::fromUtf8(responseBody));

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
            qWarning() << "[NodePushTxOffsetDiag] get_tip error=" << tipErr;
            sendPushTransaction();
            return;
        }

        const Result<Tip> tip = parseTipResult(tipObj);
        if (tip.hasError()) {
            qWarning() << "[NodePushTxOffsetDiag] get_tip parse/error=" << tip.errorMessage();
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
                qWarning() << "[NodePushTxOffsetDiag] get_header error=" << headerErr;
                sendPushTransaction();
                return;
            }

            const Result<BlockHeaderPrintable> header = parseBlockHeaderPrintable(headerObj);
            if (header.hasError()) {
                qWarning() << "[NodePushTxOffsetDiag] get_header parse/error=" << header.errorMessage();
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
        qWarning() << "[NodeForeignApi] parseNodeVersion extractOkObject failed for:" << rpcObj;
        return Result<NodeVersion>::error(r.errorMessage());
    }

    NodeVersion nv = NodeVersion::fromJson(obj);
    return Result<NodeVersion>(nv);
}

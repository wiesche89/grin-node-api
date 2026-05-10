#include "txkernel.h"

/**
 * @brief TxKernel::TxKernel
 */
TxKernel::TxKernel() :
    m_features(""),
    m_excess(""),
    m_excessSig("")
{
}

/**
 * @brief TxKernel::features
 * @return
 */
QString TxKernel::features() const
{
    return m_features;
}

qulonglong TxKernel::fee() const
{
    return m_fee;
}

/**
 * @brief TxKernel::excess
 * @return
 */
QString TxKernel::excess() const
{
    return m_excess;
}

/**
 * @brief TxKernel::excessSig
 * @return
 */
QString TxKernel::excessSig() const
{
    return m_excessSig;
}

/**
 * @brief TxKernel::setFeatures
 * @param features
 */
void TxKernel::setFeatures(const QString &features)
{
    m_features = features;
}

void TxKernel::setFee(qulonglong fee)
{
    m_fee = fee;
}

/**
 * @brief TxKernel::setExcess
 * @param excess
 */
void TxKernel::setExcess(const QString &excess)
{
    m_excess = excess;
}

/**
 * @brief TxKernel::setExcessSig
 * @param excessSig
 */
void TxKernel::setExcessSig(const QString &excessSig)
{
    m_excessSig = excessSig;
}

/**
 * @brief TxKernel::fromJson
 * @param json
 */
void TxKernel::fromJson(const QJsonObject &json)
{
    if (json.contains("features") && json["features"].isString()) {
        m_features = json["features"].toString();
    } else if (json.contains("features") && json["features"].isObject()) {
        const QJsonObject featuresObj = json["features"].toObject();
        const QStringList keys = featuresObj.keys();
        if (!keys.isEmpty()) {
            m_features = keys.first();
            const QJsonValue nested = featuresObj.value(m_features);
            if (nested.isObject()) {
                const QJsonObject nestedObj = nested.toObject();
                if (nestedObj.contains("fee") && nestedObj["fee"].isDouble()) {
                    m_fee = nestedObj["fee"].toVariant().toULongLong();
                }
            }
        }
    } else if (json.contains("features") && json["features"].isDouble()) {
        // Handle integer features (0 -> "Plain", 1 -> "Coinbase")
        int featureInt = json["features"].toInt();
        m_features = (featureInt == 1) ? QStringLiteral("Coinbase") : QStringLiteral("Plain");
    }

    if (json.contains("fee") && json["fee"].isDouble()) {
        m_fee = json["fee"].toVariant().toULongLong();
    }

    if (json.contains("excess") && json["excess"].isString()) {
        m_excess = json["excess"].toString();
    }

    if (json.contains("excess_sig") && json["excess_sig"].isString()) {
        m_excessSig = json["excess_sig"].toString();
    }
}

/**
 * @brief TxKernel::toJson
 * @return
 */
QJsonObject TxKernel::toJson() const
{
    QJsonObject json;
    
    // Grin v5 format: features is a nested object {"Plain": {"fee": ...}} or {"Coinbase": {}}
    // instead of flat structure with separate fee field
    QJsonObject feeObj;
    feeObj["fee"] = static_cast<qint64>(m_fee);
    
    QJsonObject featuresObj;
    featuresObj[m_features] = feeObj;  // {"Plain": {"fee": ...}} or {"Coinbase": {"fee": ...}}
    
    json["features"] = featuresObj;
    json["excess"] = m_excess;
    json["excess_sig"] = m_excessSig;
    return json;
}

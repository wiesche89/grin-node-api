#include "transaction.h"

/**
 * @brief Transaction::fromJson
 * @param obj
 * @return
 */
Transaction Transaction::fromJson(const QJsonObject &obj)
{
    Transaction tx;
    if (obj.contains("tx_id") && obj["tx_id"].isString()) {
        tx.m_txId = obj["tx_id"].toString();
    } else if (obj.contains("id") && obj["id"].isString()) {
        tx.m_txId = obj["id"].toString();
    }
    if (obj.contains("offset") && obj["offset"].isString()) {
        tx.m_offset.setHex(obj["offset"].toString());
    } else if (obj.contains("offset") && obj["offset"].isObject()) {
        tx.m_offset = BlindingFactor::fromJson(obj["offset"].toObject());
    }
    if (obj.contains("body") && obj["body"].isObject()) {
        tx.m_body = TransactionBody::fromJson(obj["body"].toObject());
    }
    return tx;
}

/**
 * @brief Transaction::toJson
 * @return
 */
QJsonObject Transaction::toJson() const
{
    QJsonObject obj;
    obj["tx_id"] = m_txId;
    obj["offset"] = m_offset.hex();
    obj["body"] = m_body.toJson();
    return obj;
}

QString Transaction::txId() const
{
    return m_txId;
}

/**
 * @brief Transaction::offset
 * @return
 */
BlindingFactor Transaction::offset() const
{
    return m_offset;
}

/**
 * @brief Transaction::body
 * @return
 */
TransactionBody Transaction::body() const
{
    return m_body;
}

/**
 * @brief Transaction::setOffset
 * @param offset
 */
void Transaction::setOffset(const BlindingFactor &offset)
{
    m_offset = offset;
}

void Transaction::setTxId(const QString &txId)
{
    m_txId = txId;
}

/**
 * @brief Transaction::setBody
 * @param body
 */
void Transaction::setBody(const TransactionBody &body)
{
    m_body = body;
}

#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

#include "transactionbody.h"
#include "blindingfactor.h"

class Transaction
{
    Q_GADGET
    // Expose properties to Qt's meta-object system
    Q_PROPERTY(QString txId READ txId WRITE setTxId)
    Q_PROPERTY(BlindingFactor offset READ offset WRITE setOffset)
    Q_PROPERTY(TransactionBody body READ body WRITE setBody)

public:
    Transaction() = default;

    // JSON serialization / deserialization
    static Transaction fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    // Getters
    QString txId() const;
    BlindingFactor offset() const;
    TransactionBody body() const;

    // Setters
    void setTxId(const QString &txId);
    void setOffset(const BlindingFactor &offset);
    void setBody(const TransactionBody &body);

private:
    QString m_txId;
    BlindingFactor m_offset;
    TransactionBody m_body;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(Transaction)

#endif // TRANSACTION_H

#ifndef POOLENTRY_H
#define POOLENTRY_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>

#include "txsource.h"
#include "transaction.h"

class PoolEntry
{
    Q_GADGET
    // Expose properties to the Qt meta-object system
    Q_PROPERTY(TxSourceWrapper::TxSource src READ src WRITE setSrc)
    Q_PROPERTY(QDateTime txAt READ txAt WRITE setTxAt)
    Q_PROPERTY(Transaction tx READ tx WRITE setTx)

public:
    PoolEntry() = default;

    // JSON serialization / deserialization
    static PoolEntry fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

    // Getters
    TxSourceWrapper::TxSource src() const;
    QDateTime txAt() const;
    Transaction tx() const;

    // Setters
    void setSrc(TxSourceWrapper::TxSource src);
    void setTxAt(const QDateTime &txAt);
    void setTx(const Transaction &tx);

private:
    TxSourceWrapper::TxSource m_src;
    QDateTime m_txAt;
    Transaction m_tx;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(PoolEntry)

#endif // POOLENTRY_H

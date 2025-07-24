#ifndef LOCATEDTXKERNEL_H
#define LOCATEDTXKERNEL_H

#include "txkernel.h"

class LocatedTxKernel
{
    Q_GADGET
    // Expose properties to the meta-object system
    Q_PROPERTY(TxKernel txKernel READ txKernel WRITE setTxKernel)
    Q_PROPERTY(quint64 height READ height WRITE setHeight)
    Q_PROPERTY(quint64 mmrIndex READ mmrIndex WRITE setMmrIndex)

public:
    LocatedTxKernel();
    LocatedTxKernel(const TxKernel &txKernel, quint64 height, quint64 mmrIndex);

    // Getter
    TxKernel txKernel() const;
    quint64 height() const;
    quint64 mmrIndex() const;

    // Setter
    void setTxKernel(const TxKernel &txKernel);
    void setHeight(quint64 height);
    void setMmrIndex(quint64 mmrIndex);

    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    TxKernel m_txKernel;
    quint64 m_height;
    quint64 m_mmrIndex;
};

// Register this type with the Qt meta-object system
Q_DECLARE_METATYPE(LocatedTxKernel)

#endif // LOCATEDTXKERNEL_H

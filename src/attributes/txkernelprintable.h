#ifndef TXKERNELPRINTABLE_H
#define TXKERNELPRINTABLE_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

class TxKernelPrintable
{
    Q_GADGET
    // Expose properties to Qt's meta-object system
    Q_PROPERTY(QString features READ features WRITE setFeatures)
    Q_PROPERTY(quint64 feeShift READ feeShift WRITE setFeeShift)
    Q_PROPERTY(quint64 fee READ fee WRITE setFee)
    Q_PROPERTY(quint64 lockHeight READ lockHeight WRITE setLockHeight)
    Q_PROPERTY(QString excess READ excess WRITE setExcess)
    Q_PROPERTY(QString excessSig READ excessSig WRITE setExcessSig)

public:
    TxKernelPrintable();

    // Getters
    QString features() const;
    quint64 feeShift() const;
    quint64 fee() const;
    quint64 lockHeight() const;
    QString excess() const;
    QString excessSig() const;

    // Setters
    void setFeatures(const QString &features);
    void setFeeShift(quint64 feeShift);
    void setFee(quint64 fee);
    void setLockHeight(quint64 lockHeight);
    void setExcess(const QString &excess);
    void setExcessSig(const QString &excessSig);

    // JSON serialization / deserialization
    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    QString m_features;
    quint64 m_feeShift;
    quint64 m_fee;
    quint64 m_lockHeight;
    QString m_excess;
    QString m_excessSig;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(TxKernelPrintable)

#endif // TXKERNELPRINTABLE_H

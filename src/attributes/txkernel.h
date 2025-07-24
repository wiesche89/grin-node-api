#ifndef TXKERNEL_H
#define TXKERNEL_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

class TxKernel
{
    Q_GADGET
    // Expose properties to Qt's meta-object system
    Q_PROPERTY(QString features READ features WRITE setFeatures)
    Q_PROPERTY(QString excess READ excess WRITE setExcess)
    Q_PROPERTY(QString excessSig READ excessSig WRITE setExcessSig)

public:
    TxKernel();

    // Getters
    QString features() const;
    QString excess() const;
    QString excessSig() const;

    // Setters
    void setFeatures(const QString &features);
    void setExcess(const QString &excess);
    void setExcessSig(const QString &excessSig);

    // JSON serialization / deserialization
    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    QString m_features;
    QString m_excess;
    QString m_excessSig;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(TxKernel)

#endif // TXKERNEL_H

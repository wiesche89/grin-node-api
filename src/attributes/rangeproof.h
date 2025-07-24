#ifndef RANGEPROOF_H
#define RANGEPROOF_H

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>

class RangeProof
{
    Q_GADGET
    // Expose properties to Qt's meta-object system
    Q_PROPERTY(QByteArray proof READ proof WRITE setProof)
    Q_PROPERTY(int plen READ plen WRITE setPlen)

public:
    explicit RangeProof();

    // Getters
    QByteArray proof() const;
    int plen() const;

    // Setters
    void setProof(const QByteArray &proof);
    void setPlen(int plen);

    // JSON serialization / deserialization
    QJsonObject toJson() const;
    static RangeProof fromJson(const QJsonObject &json);

private:
    QByteArray m_proof; // expected size ~5134 bytes
    int m_plen;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(RangeProof)

#endif // RANGEPROOF_H

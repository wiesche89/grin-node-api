#ifndef MERKLEPROOF_H
#define MERKLEPROOF_H

#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>

class MerkleProof
{
    Q_GADGET
    // Expose properties to the Qt meta-object system
    Q_PROPERTY(quint64 mmrSize READ mmrSize WRITE setMmrSize)
    Q_PROPERTY(QVector<QString> path READ path WRITE setPath)

public:
    MerkleProof();

    quint64 mmrSize() const;
    void setMmrSize(quint64 size);

    QVector<QString> path() const;
    void setPath(const QVector<QString> &path);

    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    quint64 m_mmrSize;
    QVector<QString> m_path;
};

// Register this type with Qt meta-object system
Q_DECLARE_METATYPE(MerkleProof)

#endif // MERKLEPROOF_H

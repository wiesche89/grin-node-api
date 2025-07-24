#ifndef BLOCKLISTING_H
#define BLOCKLISTING_H

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

#include "blockprintable.h"

class BlockListing
{
    Q_GADGET
    Q_PROPERTY(quint64 lastRetrievedHeight READ lastRetrievedHeight WRITE setLastRetrievedHeight)
    Q_PROPERTY(QVector<BlockPrintable> blocks READ blocks WRITE setBlocks)

public:
    BlockListing();

    // Getter
    quint64 lastRetrievedHeight() const;
    QVector<BlockPrintable> blocks() const;

    // Setter
    void setLastRetrievedHeight(quint64 height);
    void setBlocks(const QVector<BlockPrintable> &blocks);

    // JSON-Parsing
    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    quint64 m_lastRetrievedHeight;
    QVector<BlockPrintable> m_blocks;
};

Q_DECLARE_METATYPE(BlockListing)

#endif // BLOCKLISTING_H

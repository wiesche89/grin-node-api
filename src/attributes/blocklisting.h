// BlockListing.h
#ifndef BLOCKLISTING_H
#define BLOCKLISTING_H

#include <QVector>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>

#include "blockprintable.h"

/**
 * QML-freundliches Listing von Blöcken.
 * - Bewahrt native Struktur (QVector<BlockPrintable>)
 * - Bietet zusätzlich blocksVariant (QVariantList) für QML-Iterationen (Repeater/ListView)
 * - Praktische Convenience-Properties (count, hasBlocks, first/last height)
 */
class BlockListing
{
    Q_GADGET

    // Native Properties (C++-seitig praktisch)
    Q_PROPERTY(quint64 lastRetrievedHeight READ lastRetrievedHeight WRITE setLastRetrievedHeight)
    Q_PROPERTY(QVector<BlockPrintable> blocks READ blocks WRITE setBlocks)

    // QML-freundliche Spiegelungen
    Q_PROPERTY(QVariantList blocksVariant READ blocksVariant)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(bool hasBlocks READ hasBlocks)
    Q_PROPERTY(quint64 firstHeight READ firstHeight)
    Q_PROPERTY(quint64 lastHeight READ lastHeight)

public:
    BlockListing();

    // --- Getter (native) ---
    quint64 lastRetrievedHeight() const;
    QVector<BlockPrintable> blocks() const;

    // --- Setter (native) ---
    void setLastRetrievedHeight(quint64 height);
    void setBlocks(const QVector<BlockPrintable> &blocks);

    // --- QML-geeignete Abbildung ---
    QVariantList blocksVariant() const;

    // --- Convenience ---
    int count() const { return m_blocks.size(); }
    bool hasBlocks() const { return !m_blocks.isEmpty(); }
    quint64 firstHeight() const;
    quint64 lastHeight() const;

    // --- Mutators / Utils ---
    void clear();
    void append(const BlockPrintable &bp);

    // --- JSON ---
    void fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    quint64 m_lastRetrievedHeight = 0;
    QVector<BlockPrintable> m_blocks;

    // Hilfsfunktion für Variant-Mapping eines BlockPrintable
    static QVariantMap blockToVariantMap(const BlockPrintable &bp);
};

Q_DECLARE_METATYPE(BlockListing)

#endif // BLOCKLISTING_H

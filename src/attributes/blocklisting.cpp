// blocklisting.cpp
#include "blocklisting.h"
#include "blockprintable.h"

#include <QtAlgorithms>
#include <QVariant>
#include <QJsonValue>

BlockListing::BlockListing() = default;

// ----------------- Native Getter/Setter -----------------
quint64 BlockListing::lastRetrievedHeight() const { return m_lastRetrievedHeight; }
QVector<BlockPrintable> BlockListing::blocks() const { return m_blocks; }

void BlockListing::setLastRetrievedHeight(quint64 height) { m_lastRetrievedHeight = height; }
void BlockListing::setBlocks(const QVector<BlockPrintable> &blocks) { m_blocks = blocks; }

// ----------------- QML-Variant-Liste -----------------
QVariantList BlockListing::blocksVariant() const
{
    QVariantList list;
    list.reserve(m_blocks.size());
    for (const auto &bp : m_blocks)
        list.push_back(blockToVariantMap(bp));
    return list;
}

// ----------------- Convenience -----------------
quint64 BlockListing::firstHeight() const
{
    if (m_blocks.isEmpty()) return 0;
    // Annahme: Header.height spiegelt die Blockhöhe
    quint64 minH = std::numeric_limits<quint64>::max();
    for (const auto &bp : m_blocks) {
        const auto h = static_cast<quint64>(bp.header().height());
        if (h < minH) minH = h;
    }
    return (minH == std::numeric_limits<quint64>::max()) ? 0 : minH;
}

quint64 BlockListing::lastHeight() const
{
    if (m_blocks.isEmpty()) return 0;
    quint64 maxH = 0;
    for (const auto &bp : m_blocks) {
        const auto h = static_cast<quint64>(bp.header().height());
        if (h > maxH) maxH = h;
    }
    return maxH;
}

void BlockListing::clear()
{
    m_blocks.clear();
    m_lastRetrievedHeight = 0;
}

void BlockListing::append(const BlockPrintable &bp)
{
    m_blocks.push_back(bp);
}

// ----------------- JSON -----------------
void BlockListing::fromJson(const QJsonObject &json)
{
    // lastRetrievedHeight (snake_case oder camelCase akzeptieren)
    if (json.contains(QStringLiteral("last_retrieved_height")) && json.value(QStringLiteral("last_retrieved_height")).isDouble()) {
        m_lastRetrievedHeight = static_cast<quint64>(json.value(QStringLiteral("last_retrieved_height")).toDouble());
    } else if (json.contains(QStringLiteral("lastRetrievedHeight")) && json.value(QStringLiteral("lastRetrievedHeight")).isDouble()) {
        m_lastRetrievedHeight = static_cast<quint64>(json.value(QStringLiteral("lastRetrievedHeight")).toDouble());
    } else {
        m_lastRetrievedHeight = 0;
    }

    m_blocks.clear();

    // Blöcke: „blocks“ oder „items“ akzeptieren
    QJsonArray arr;
    if (json.contains(QStringLiteral("blocks")) && json.value(QStringLiteral("blocks")).isArray()) {
        arr = json.value(QStringLiteral("blocks")).toArray();
    } else if (json.contains(QStringLiteral("items")) && json.value(QStringLiteral("items")).isArray()) {
        arr = json.value(QStringLiteral("items")).toArray();
    }

    m_blocks.reserve(arr.size());
    for (const auto &v : arr) {
        if (!v.isObject()) continue;
        BlockPrintable bp;
        bp.fromJson(v.toObject());
        m_blocks.push_back(bp);
    }
}

QJsonObject BlockListing::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("last_retrieved_height"), static_cast<double>(m_lastRetrievedHeight));

    QJsonArray blocksArray;
    for (const auto &bp : m_blocks) {
        blocksArray.push_back(bp.toJson());
    }

    json.insert(QStringLiteral("blocks"), blocksArray);
    return json;
}

// ----------------- Helper: Block → QVariantMap -----------------
QVariantMap BlockListing::blockToVariantMap(const BlockPrintable &bp)
{
    // Bevorzugt: nutze toJson() des Blocks (komplett & konsistent),
    // dann erhalten wir auch header.* sauber.
    const QJsonObject obj = bp.toJson();
    QVariantMap m = obj.toVariantMap();

    // Ergänze ein paar bequeme flache Spiegel-Felder (falls QML sie direkt nutzt)
    // Header-Felder sicher spiegeln:
    const BlockHeaderPrintable &hdr = bp.header();
    m.insert(QStringLiteral("height"),        static_cast<qulonglong>(hdr.height()));
    m.insert(QStringLiteral("hash"),          hdr.hash());
    m.insert(QStringLiteral("timestamp"),     static_cast<QString>(hdr.timestamp()));
    m.insert(QStringLiteral("total_difficulty"), static_cast<qulonglong>(hdr.totalDifficulty()));
    m.insert(QStringLiteral("kernel_root"),   hdr.kernelRoot());
    m.insert(QStringLiteral("output_root"),   hdr.outputRoot());

    // Zähler (praktisch in Kacheln)
    m.insert(QStringLiteral("num_inputs"),  bp.inputs().size());
    m.insert(QStringLiteral("num_outputs"), bp.outputs().size());
    m.insert(QStringLiteral("num_kernels"), bp.kernels().size());

    return m;
}

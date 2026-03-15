#include "capabilities.h"

namespace {
struct CapabilityFlag
{
    quint32 bit;
    const char *name;
};

const CapabilityFlag kCapabilityFlags[] = {
    { 0x01u, "HEADER_HIST" },
    { 0x02u, "TXHASHSET_HIST" },
    { 0x04u, "PEER_LIST" },
    { 0x08u, "TX_KERNEL_HASH" },
    { 0x10u, "PIBD_HIST" },
    { 0x20u, "BLOCK_HIST" },
    { 0x40u, "PIBD_HIST_1" }
};
}

/**
 * @brief Capabilities::Capabilities
 */
Capabilities::Capabilities()
{
}

/**
 * @brief Capabilities::Capabilities
 * @param obj
 */
Capabilities::Capabilities(const QJsonObject &obj) : m_obj(obj)
{
}

/**
 * @brief Capabilities::toJson
 * @return
 */
QJsonObject Capabilities::toJson() const
{
    return m_obj;
}

/**
 * @brief Capabilities::fromJson
 * @param obj
 * @return
 */
Capabilities Capabilities::fromJson(const QJsonObject &obj)
{
    return Capabilities(obj);
}

quint32 Capabilities::bits() const
{
    return static_cast<quint32>(m_obj.value("bits").toInt());
}

QString Capabilities::toString() const
{
    const quint32 value = bits();
    if (value == 0u)
        return QStringLiteral("UNKNOWN");

    QStringList names;
    for (const CapabilityFlag &flag : kCapabilityFlags) {
        if ((value & flag.bit) != 0u)
            names.append(QLatin1String(flag.name));
    }

    if (names.isEmpty())
        return QString::number(value);

    return names.join(QStringLiteral(", "));
}

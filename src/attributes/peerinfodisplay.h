#ifndef PEERINFODISPLAY_H
#define PEERINFODISPLAY_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

#include "capabilities.h"
#include "protocolversion.h"
#include "peeraddr.h"
#include "direction.h"
#include "difficulty.h"

class PeerInfoDisplay
{
    Q_GADGET
    Q_PROPERTY(QString userAgent READ userAgent CONSTANT)
    Q_PROPERTY(quint64 height READ height CONSTANT)
    Q_PROPERTY(PeerAddr addr READ addr CONSTANT)
    Q_PROPERTY(ProtocolVersion version READ version CONSTANT)
    Q_PROPERTY(Direction direction READ direction CONSTANT)
    Q_PROPERTY(Capabilities capabilities READ capabilities CONSTANT)
    Q_PROPERTY(Difficulty totalDifficulty READ totalDifficulty CONSTANT)

public:
    PeerInfoDisplay();

    // Getters
    Capabilities capabilities() const;
    QString userAgent() const;
    ProtocolVersion version() const;
    PeerAddr addr() const;
    Direction direction() const;
    Difficulty totalDifficulty() const;
    quint64 height() const;

    // Setters
    void setCapabilities(const Capabilities &capabilities);
    void setUserAgent(const QString &userAgent);
    void setVersion(const ProtocolVersion &version);
    void setAddr(const PeerAddr &addr);
    void setDirection(const Direction &direction);
    void setTotalDifficulty(const Difficulty &difficulty);
    void setHeight(quint64 height);

    // JSON serialization / deserialization
    QJsonObject toJson() const;
    static PeerInfoDisplay fromJson(const QJsonObject &obj);

private:
    Capabilities m_capabilities;
    QString m_userAgent;
    ProtocolVersion m_version;
    PeerAddr m_addr;
    Direction m_direction;
    Difficulty m_totalDifficulty;
    quint64 m_height;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(PeerInfoDisplay)

#endif // PEERINFODISPLAY_H

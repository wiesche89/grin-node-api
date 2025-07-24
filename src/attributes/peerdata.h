#ifndef PEERDATA_H
#define PEERDATA_H

#include <QString>
#include <QJsonObject>
#include <QVariant>
#include <QMetaType>

#include "peeraddr.h"
#include "capabilities.h"

class PeerData
{
    Q_GADGET

public:
    // Enum for peer state
    enum State {
        Healthy = 0,
        Banned = 1,
        Defunct = 2
    };
    Q_ENUM(State)

    // Enum for ban reason
    enum ReasonForBan {
        None = 0,
        BadBlock = 1,
        BadCompactBlock = 2,
        BadBlockHeader = 3,
        BadTxHashSet = 4,
        ManualBan = 5,
        FraudHeight = 6,
        BadHandshake = 7
    };
    Q_ENUM(ReasonForBan)

    // Properties exposed to Qt's meta-object system
    Q_PROPERTY(PeerAddr addr READ getAddr WRITE setAddr)
    Q_PROPERTY(Capabilities capabilities READ getCapabilities WRITE setCapabilities)
    Q_PROPERTY(QString userAgent READ getUserAgent WRITE setUserAgent)
    Q_PROPERTY(State flags READ getFlags WRITE setFlags)
    Q_PROPERTY(qint64 lastBanned READ getLastBanned WRITE setLastBanned)
    Q_PROPERTY(ReasonForBan banReason READ getBanReason WRITE setBanReason)
    Q_PROPERTY(qint64 lastConnected READ getLastConnected WRITE setLastConnected)

public:
    PeerData();

    // Getters
    PeerAddr getAddr() const;
    Capabilities getCapabilities() const;
    QString getUserAgent() const;
    State getFlags() const;
    qint64 getLastBanned() const;
    ReasonForBan getBanReason() const;
    qint64 getLastConnected() const;

    // Setters
    void setAddr(const PeerAddr &addr);
    void setCapabilities(const Capabilities &capabilities);
    void setUserAgent(const QString &userAgent);
    void setFlags(State flags);
    void setLastBanned(qint64 lastBanned);
    void setBanReason(ReasonForBan reason);
    void setLastConnected(qint64 lastConnected);

    // JSON serialization / deserialization
    static PeerData fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

private:
    PeerAddr m_addr;
    Capabilities m_capabilities;
    QString m_userAgent;
    State m_flags;
    qint64 m_lastBanned;
    ReasonForBan m_banReason;
    qint64 m_lastConnected;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(PeerData)

#endif // PEERDATA_H

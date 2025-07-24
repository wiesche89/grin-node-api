#ifndef STATUS_H
#define STATUS_H

#include <QString>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>

#include "tip.h"
#include "syncinfo.h"

class Status
{
    Q_GADGET
    // Expose properties to Qt meta-object system
    Q_PROPERTY(QString chain READ getChain WRITE setChain)
    Q_PROPERTY(quint32 protocolVersion READ getProtocolVersion WRITE setProtocolVersion)
    Q_PROPERTY(QString userAgent READ getUserAgent WRITE setUserAgent)
    Q_PROPERTY(quint32 connections READ getConnections WRITE setConnections)
    Q_PROPERTY(Tip tip READ getTip WRITE setTip)
    Q_PROPERTY(QString syncStatus READ getSyncStatus WRITE setSyncStatus)
    Q_PROPERTY(SyncInfo syncInfo READ syncInfo)

public:
    Status();

    // Getters
    QString getChain() const;
    quint32 getProtocolVersion() const;
    QString getUserAgent() const;
    quint32 getConnections() const;
    Tip getTip() const;
    QString getSyncStatus() const;

    // Setters
    void setChain(const QString &chain);
    void setProtocolVersion(quint32 version);
    void setUserAgent(const QString &userAgent);
    void setConnections(quint32 connections);
    void setTip(const Tip &tip);
    void setSyncStatus(const QString &syncStatus);

    SyncInfo syncInfo() const
    {
        return m_syncInfo;
    }

    // JSON serialization / deserialization
    static Status fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;

private:
    QString m_chain;
    quint32 m_protocolVersion;
    QString m_userAgent;
    quint32 m_connections;
    Tip m_tip;
    QString m_syncStatus;
    SyncInfo m_syncInfo;
};

// Register this type with Qt meta-object system
Q_DECLARE_METATYPE(Status)

#endif // STATUS_H

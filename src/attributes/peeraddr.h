#ifndef PEERADDR_H
#define PEERADDR_H

#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>

class PeerAddr
{
    Q_GADGET
    Q_PROPERTY(QJsonObject obj READ toJson)
    Q_PROPERTY(QString asString READ toString CONSTANT)

public:
    PeerAddr();
    explicit PeerAddr(const QJsonObject &obj);

    QJsonObject toJson() const;
    static PeerAddr fromJson(const QJsonObject &obj);

    QString toString() const
    {
        return m_obj.value("addr").toString();
    }

private:
    QJsonObject m_obj;
};
Q_DECLARE_METATYPE(PeerAddr)

#endif // PEERADDR_H

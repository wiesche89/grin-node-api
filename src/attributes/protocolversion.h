#ifndef PROTOCOLVERSION_H
#define PROTOCOLVERSION_H

#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>

class ProtocolVersion
{
    Q_GADGET
    Q_PROPERTY(QJsonObject obj READ toJson)
    Q_PROPERTY(QString asString READ toString CONSTANT)

public:
    ProtocolVersion();
    explicit ProtocolVersion(const QJsonObject &obj);

    QJsonObject toJson() const;
    static ProtocolVersion fromJson(const QJsonObject &obj);

    QString toString() const
    {
        return QString::number(m_obj.value("version").toInt());
    }

private:
    QJsonObject m_obj;
};
Q_DECLARE_METATYPE(ProtocolVersion)

#endif // PROTOCOLVERSION_H

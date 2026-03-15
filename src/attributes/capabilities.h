#ifndef CAPABILITIES_H
#define CAPABILITIES_H

#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>

class Capabilities
{
    Q_GADGET
    Q_PROPERTY(QJsonObject obj READ toJson)
    Q_PROPERTY(QString asString READ toString CONSTANT)

public:
    Capabilities();
    explicit Capabilities(const QJsonObject &obj);

    QJsonObject toJson() const;
    static Capabilities fromJson(const QJsonObject &obj);

    QString toString() const;

private:
    quint32 bits() const;
    QJsonObject m_obj;
};
Q_DECLARE_METATYPE(Capabilities)

#endif // CAPABILITIES_H

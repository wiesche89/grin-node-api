#ifndef DIRECTION_H
#define DIRECTION_H

#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>

class Direction
{
    Q_GADGET
    Q_PROPERTY(QJsonObject obj READ toJson)
    Q_PROPERTY(QString asString READ toString CONSTANT)

public:
    Direction();
    explicit Direction(const QJsonObject &obj);

    QJsonObject toJson() const;
    static Direction fromJson(const QJsonObject &obj);

    QString toString() const
    {
        return m_obj.value("direction").toString();
    }

private:
    QJsonObject m_obj;
};
Q_DECLARE_METATYPE(Direction)

#endif // DIRECTION_H

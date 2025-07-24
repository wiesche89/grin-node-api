#ifndef DIFFICULTY_H
#define DIFFICULTY_H

#include <QJsonObject>
#include <QJsonDocument>
#include <QMetaType>

class Difficulty
{
    Q_GADGET
    Q_PROPERTY(QJsonObject obj READ toJson)
    Q_PROPERTY(QString asString READ toString CONSTANT)

public:
    Difficulty();
    explicit Difficulty(const QJsonObject &obj);

    QJsonObject toJson() const;
    static Difficulty fromJson(const QJsonObject &obj);

    QString toString() const
    {
        // Achtung: falls m_obj direkt nur einen Wert enthält, anpassen
        // Beispiel: m_obj["total_difficulty"]
        return QString::number(m_obj.value("total_difficulty").toVariant().toULongLong());
    }

private:
    QJsonObject m_obj;
};
Q_DECLARE_METATYPE(Difficulty)

#endif // DIFFICULTY_H

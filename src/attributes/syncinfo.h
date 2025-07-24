#ifndef SYNCINFO_H
#define SYNCINFO_H

#include <QString>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaType>

// kein QObject, sondern ein Q_GADGET
class SyncInfo
{
    Q_GADGET
    Q_PROPERTY(QString jsonString READ jsonString)
    Q_PROPERTY(bool isObject READ isObject)
    Q_PROPERTY(bool isArray READ isArray)
    Q_PROPERTY(bool isNull READ isNull)

public:
    SyncInfo()
    {
    }

    explicit SyncInfo(const QJsonValue &val) : m_value(val)
    {
    }

    static SyncInfo fromJson(const QJsonValue &val)
    {
        return SyncInfo(val);
    }

    QString jsonString() const
    {
        if (m_value.isObject() || m_value.isArray()) {
            QJsonDocument doc(m_value.toObject());
            if (m_value.isArray()) {
                doc = QJsonDocument(m_value.toArray());
            }
            return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        }
        if (m_value.isString()) {
            return m_value.toString();
        }
        if (m_value.isDouble()) {
            return QString::number(m_value.toDouble());
        }
        if (m_value.isBool()) {
            return m_value.toBool() ? "true" : "false";
        }
        return QString();
    }

    bool isObject() const
    {
        return m_value.isObject();
    }

    bool isArray() const
    {
        return m_value.isArray();
    }

    bool isNull() const
    {
        return m_value.isNull();
    }

    QJsonValue toJson() const
    {
        return m_value;
    }

    Q_INVOKABLE QString getString(const QString &key) const
    {
        if (!m_value.isObject()) {
            return {};
        }
        return m_value.toObject().value(key).toString();
    }

    Q_INVOKABLE double getNumber(const QString &key) const
    {
        if (!m_value.isObject()) {
            return 0.0;
        }
        return m_value.toObject().value(key).toDouble();
    }

    Q_INVOKABLE bool getBool(const QString &key) const
    {
        if (!m_value.isObject()) {
            return false;
        }
        return m_value.toObject().value(key).toBool();
    }

private:
    QJsonValue m_value;
};

Q_DECLARE_METATYPE(SyncInfo)

#endif // SYNCINFO_H

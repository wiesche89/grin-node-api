#ifndef TIP_H
#define TIP_H

#include <QString>
#include <QJsonObject>
#include <QMetaType>

class Tip
{
    Q_GADGET
    // Expose properties to Qt meta-object system
    Q_PROPERTY(quint64 height READ height WRITE setHeight)
    Q_PROPERTY(QString lastBlockPushed READ lastBlockPushed WRITE setLastBlockPushed)
    Q_PROPERTY(QString prevBlockToLast READ prevBlockToLast WRITE setPrevBlockToLast)
    Q_PROPERTY(quint64 totalDifficulty READ totalDifficulty WRITE setTotalDifficulty)

public:
    Tip();
    Tip(quint64 height, const QString &lastBlockPushed, const QString &prevBlockToLast, quint64 totalDifficulty);

    // Getters
    quint64 height() const;
    QString lastBlockPushed() const;
    QString prevBlockToLast() const;
    quint64 totalDifficulty() const;

    // Setters
    void setHeight(quint64 height);
    void setLastBlockPushed(const QString &lastBlockPushed);
    void setPrevBlockToLast(const QString &prevBlockToLast);
    void setTotalDifficulty(quint64 totalDifficulty);

    // JSON serialization / deserialization
    static Tip fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    quint64 m_height;
    QString m_lastBlockPushed;
    QString m_prevBlockToLast;
    quint64 m_totalDifficulty;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(Tip)

#endif // TIP_H

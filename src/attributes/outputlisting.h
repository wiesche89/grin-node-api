#ifndef OUTPUTLISTING_H
#define OUTPUTLISTING_H

#include <QList>
#include <QJsonObject>
#include <QJsonArray>

#include "outputprintable.h"

class OutputListing
{
    Q_GADGET
    // Expose properties to the Qt meta-object system
    Q_PROPERTY(quint64 highestIndex READ highestIndex WRITE setHighestIndex)
    Q_PROPERTY(quint64 lastRetrievedIndex READ lastRetrievedIndex WRITE setLastRetrievedIndex)
    Q_PROPERTY(QList<OutputPrintable> outputs READ outputs WRITE setOutputs)

public:
    OutputListing();

    quint64 highestIndex() const;
    void setHighestIndex(quint64 index);

    quint64 lastRetrievedIndex() const;
    void setLastRetrievedIndex(quint64 index);

    const QList<OutputPrintable> &outputs() const;
    void setOutputs(const QList<OutputPrintable> &outputs);
    void addOutput(const OutputPrintable &output);

    // JSON serialization
    static OutputListing fromJson(const QJsonObject &json);
    QJsonObject toJson() const;

private:
    quint64 m_highestIndex;
    quint64 m_lastRetrievedIndex;
    QList<OutputPrintable> m_outputs;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(OutputListing)

#endif // OUTPUTLISTING_H

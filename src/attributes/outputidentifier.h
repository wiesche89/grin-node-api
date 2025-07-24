#ifndef OUTPUTIDENTIFIER_H
#define OUTPUTIDENTIFIER_H

#include <QObject>
#include <QJsonObject>

#include "commitment.h"
#include "outputfeatures.h"

class OutputIdentifier
{
    Q_GADGET
    // Expose the properties to the Qt meta-object system
    Q_PROPERTY(OutputFeatures::Feature features READ features WRITE setFeatures)
    Q_PROPERTY(Commitment commit READ commit WRITE setCommit)

public:
    explicit OutputIdentifier();
    OutputIdentifier(OutputFeatures::Feature features, Commitment commit);

    OutputFeatures::Feature features() const;
    void setFeatures(OutputFeatures::Feature features);

    Commitment commit() const;
    void setCommit(Commitment commit);

    QJsonObject toJson() const;
    static OutputIdentifier fromJson(const QJsonObject &json);

private:
    OutputFeatures::Feature m_features;
    Commitment m_commit;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(OutputIdentifier)

#endif // OUTPUTIDENTIFIER_H

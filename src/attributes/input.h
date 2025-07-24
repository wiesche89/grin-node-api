#ifndef INPUT_H
#define INPUT_H

#include <QObject>
#include <QJsonObject>

#include "commitment.h"
#include "outputfeatures.h"

class Input
{
    Q_GADGET
    // Expose the feature and commit as properties
    Q_PROPERTY(OutputFeatures::Feature features READ features WRITE setFeatures)
    Q_PROPERTY(Commitment commit READ commit WRITE setCommit)

public:
    Input();
    Input(OutputFeatures::Feature features, Commitment commit);

    OutputFeatures::Feature features() const;
    void setFeatures(OutputFeatures::Feature features);

    Commitment commit() const;
    void setCommit(Commitment commit);

    QJsonObject toJson() const;
    static Input fromJson(const QJsonObject &json);

private:
    OutputFeatures::Feature m_features;
    Commitment m_commit;
};

// Register with Qt meta-object system
Q_DECLARE_METATYPE(Input)

#endif // INPUT_H

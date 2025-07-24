#ifndef OUTPUTFEATURES_H
#define OUTPUTFEATURES_H

#include <QObject>

class OutputFeatures
{
    Q_GADGET

public:
    enum Feature {
        Plain = 0,
        Coinbase = 1
    };
    Q_ENUM(Feature)
};

// Register the enum type with Qt's meta-object system
Q_DECLARE_METATYPE(OutputFeatures::Feature)

#endif // OUTPUTFEATURES_H

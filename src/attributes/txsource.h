#ifndef TXSOURCE_H
#define TXSOURCE_H

#include <QString>
#include <QMetaType>

class TxSourceWrapper
{
    Q_GADGET
public:
    // Enum describing the transaction source
    enum TxSource {
        PushApi,
        Broadcast,
        Fluff,
        EmbargoExpired,
        Deaggregate
    };
    Q_ENUM(TxSource)
};

// Register the enum type with Qt's meta-object system
Q_DECLARE_METATYPE(TxSourceWrapper::TxSource)

// Utility conversion functions
QString txSourceToString(TxSourceWrapper::TxSource src);
TxSourceWrapper::TxSource txSourceFromString(const QString &str);

#endif // TXSOURCE_H

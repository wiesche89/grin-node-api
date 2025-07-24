#include "txsource.h"

/**
 * @brief txSourceToString
 * @param src
 * @return
 */
QString txSourceToString(TxSourceWrapper::TxSource src)
{
    switch (src) {
    case TxSourceWrapper::TxSource::PushApi:
        return "PushApi";
    case TxSourceWrapper::TxSource::Broadcast:
        return "Broadcast";
    case TxSourceWrapper::TxSource::Fluff:
        return "Fluff";
    case TxSourceWrapper::TxSource::EmbargoExpired:
        return "EmbargoExpired";
    case TxSourceWrapper::TxSource::Deaggregate:
        return "Deaggregate";
    }
    return {};
}

/**
 * @brief txSourceFromString
 * @param str
 * @return
 */
TxSourceWrapper::TxSource txSourceFromString(const QString &str)
{
    if (str == "PushApi") {
        return TxSourceWrapper::TxSource::PushApi;
    }
    if (str == "Broadcast") {
        return TxSourceWrapper::TxSource::Broadcast;
    }
    if (str == "Fluff") {
        return TxSourceWrapper::TxSource::Fluff;
    }
    if (str == "EmbargoExpired") {
        return TxSourceWrapper::TxSource::EmbargoExpired;
    }
    if (str == "Deaggregate") {
        return TxSourceWrapper::TxSource::Deaggregate;
    }
    // Default or error handling
    return TxSourceWrapper::TxSource::PushApi; // Default fallback
}

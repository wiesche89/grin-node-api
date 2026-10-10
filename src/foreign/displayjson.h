#pragma once

#include <QByteArray>

// Quote large integer tokens before JSON decoding, including unsigned 64-bit values.
// Strings, fractions and exponents retain their original representation.
inline QByteArray displayJson(const QByteArray &json)
{
    QByteArray result;
    result.reserve(json.size());
    for (qsizetype i = 0; i < json.size();) {
        const qsizetype start = i;
        if (json[i] == '"') {
            ++i;
            while (i < json.size()) {
                if (json[i] == '\\') i += 2;
                else if (json[i++] == '"') break;
            }
            result += json.mid(start, i - start);
        } else if (json[i] == '-' || (json[i] >= '0' && json[i] <= '9')) {
            if (json[i] == '-') ++i;
            const qsizetype digits = i;
            while (i < json.size() && json[i] >= '0' && json[i] <= '9') ++i;
            const bool large = i - digits >= 16 && json[digits] != '0';
            const bool integer = i == json.size() || (json[i] != '.' && json[i] != 'e' && json[i] != 'E');
            if (!integer) {
                while (i < json.size() && ((json[i] >= '0' && json[i] <= '9') || json[i] == '.'
                       || json[i] == 'e' || json[i] == 'E' || json[i] == '+' || json[i] == '-')) ++i;
            }
            if (large && integer) result += '"';
            result += json.mid(start, i - start);
            if (large && integer) result += '"';
        } else {
            result += json[i++];
        }
    }
    return result;
}

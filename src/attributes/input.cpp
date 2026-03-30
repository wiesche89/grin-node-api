#include "input.h"

/**
 * @brief Input::Input
 */
Input::Input() :
    m_features(OutputFeatures::Plain),
    m_commit()
{
}

/**
 * @brief Input::Input
 * @param features
 * @param commit
 */
Input::Input(OutputFeatures::Feature features, Commitment commit) :
    m_features(features),
    m_commit(commit)
{
}

/**
 * @brief Input::features
 * @return
 */
OutputFeatures::Feature Input::features() const
{
    return m_features;
}

/**
 * @brief Input::setFeatures
 * @param features
 */
void Input::setFeatures(OutputFeatures::Feature features)
{
    m_features = features;
}

/**
 * @brief Input::commit
 * @return
 */
Commitment Input::commit() const
{
    return m_commit;
}

/**
 * @brief Input::setCommit
 * @param commit
 */
void Input::setCommit(Commitment commit)
{
    m_commit = commit;
}

QJsonObject Input::toJson() const
{
    QJsonObject json;
    // CRITICAL: Features must be string like "Plain" or "Coinbase", not integer
    // Convert enum to string name to match Grin reference format
    QString featureName = (m_features == OutputFeatures::Coinbase) ? QStringLiteral("Coinbase") : QStringLiteral("Plain");
    json["features"] = featureName;
    json["commit"] = m_commit.toJson();

    return json;
}

/**
 * @brief Input::fromJson
 * @param json
 * @return
 */
Input Input::fromJson(const QJsonObject &json)
{
    // Handle features as either string ("Plain", "Coinbase") or integer (0, 1)
    OutputFeatures::Feature features = OutputFeatures::Plain;
    if (json.contains("features")) {
        if (json.value("features").isString()) {
            const QString featureStr = json.value("features").toString();
            if (featureStr == "Coinbase") {
                features = OutputFeatures::Coinbase;
            } else {
                features = OutputFeatures::Plain;  // Default to "Plain"
            }
        } else if (json.value("features").isDouble()) {
            features = static_cast<OutputFeatures::Feature>(json.value("features").toInt());
        }
    }
    
    Commitment commit;
    if (json.contains("commit")) {
        if (json.value("commit").isString()) {
            commit.setHex(json.value("commit").toString());
        } else if (json.value("commit").isObject()) {
            commit.fromJson(json.value("commit").toObject());
        }
    }
    return Input(features, commit);
}

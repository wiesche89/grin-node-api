#ifndef TRANSACTIONBODY_H
#define TRANSACTIONBODY_H

#include <QJsonObject>
#include <QVector>
#include <QJsonArray>
#include <QMetaType>

#include "input.h"
#include "output.h"
#include "txkernel.h"

class TransactionBody
{
    Q_GADGET
    // Expose properties to Qt's meta-object system
    Q_PROPERTY(QVector<Input> inputs READ inputs WRITE setInputs)
    Q_PROPERTY(QVector<Output> outputs READ outputs WRITE setOutputs)
    Q_PROPERTY(QVector<TxKernel> kernels READ kernels WRITE setKernels)

public:
    TransactionBody();

    // Getters
    QVector<Input> inputs() const;
    QVector<Output> outputs() const;
    QVector<TxKernel> kernels() const;

    // Setters
    void setInputs(const QVector<Input> &inputs);
    void setOutputs(const QVector<Output> &outputs);
    void setKernels(const QVector<TxKernel> &kernels);

    // JSON serialization / deserialization
    QJsonObject toJson() const;
    static TransactionBody fromJson(const QJsonObject &json);

private:
    QVector<Input> m_inputs;
    QVector<Output> m_outputs;
    QVector<TxKernel> m_kernels;
};

// Register this type with Qt's meta-object system
Q_DECLARE_METATYPE(TransactionBody)

#endif // TRANSACTIONBODY_H

#pragma once

#include <QByteArray>
#include <QString>

// Read a numeric root.data member without passing its wire representation
// through Qt 5's double storage. Strings and non-integral values are rejected.
qint64 jsonDataInteger(const QByteArray &json, const QString &member, qint64 fallback = -1);

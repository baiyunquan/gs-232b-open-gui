#include "ManagementJson.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

qint64 exactInteger(const QString &number, qint64 fallback)
{
    static const QRegularExpression pattern(
        QStringLiteral("^(-?)([0-9]+)(?:\\.([0-9]+))?(?:[eE]([+-]?[0-9]+))?$"));
    const auto match = pattern.match(number);
    if (!match.hasMatch())
        return fallback;
    QString digits = match.captured(2) + match.captured(3);
    int first = 0;
    while (first < digits.size() && digits.at(first) == QLatin1Char('0'))
        ++first;
    digits.remove(0, first);
    if (digits.isEmpty())
        return 0;

    bool ok = true;
    const int exponent = match.captured(4).isEmpty() ? 0 : match.captured(4).toInt(&ok);
    if (!ok)
        return fallback;
    const qint64 shift = qint64(exponent) - match.captured(3).size();
    if (shift < 0) {
        if (-shift >= digits.size())
            return fallback;
        for (qint64 i = digits.size() + shift; i < digits.size(); ++i) {
            if (digits.at(int(i)) != QLatin1Char('0'))
                return fallback;
        }
        digits.chop(int(-shift));
    } else {
        if (digits.size() + shift > 19)
            return fallback;
        digits.append(QString(int(shift), QLatin1Char('0')));
    }
    const qint64 value = (match.captured(1) + digits).toLongLong(&ok);
    return ok ? value : fallback;
}

QByteArray quoteNumbers(const QByteArray &json)
{
    QByteArray result;
    bool inString = false;
    bool escaped = false;
    for (int i = 0; i < json.size(); ++i) {
        const char ch = json.at(i);
        if (inString) {
            result.append(ch);
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                inString = false;
        } else if (ch == '"') {
            inString = true;
            result.append(ch);
        } else if (ch == '-' || (ch >= '0' && ch <= '9')) {
            result.append('"');
            do {
                result.append(json.at(i++));
            } while (i < json.size() && QByteArrayLiteral("0123456789.eE+-").contains(json.at(i)));
            result.append('"');
            --i;
        } else {
            result.append(ch);
        }
    }
    return result;
}

} // namespace

qint64 jsonDataInteger(const QByteArray &json, const QString &member, qint64 fallback)
{
    // Validate syntax and the original type first; quoted numeric strings are
    // not protocol integers. The second parse preserves numeric lexemes while
    // letting Qt handle nesting, escaped keys and duplicate members consistently.
    const auto original = QJsonDocument::fromJson(json).object();
    if (!original.value(QStringLiteral("data")).toObject().value(member).isDouble())
        return fallback;
    const auto exact = QJsonDocument::fromJson(quoteNumbers(json)).object();
    return exactInteger(exact.value(QStringLiteral("data")).toObject().value(member).toString(), fallback);
}

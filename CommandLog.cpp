#include "CommandLog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Communication {

namespace {

QJsonValue redactPasswords(const QJsonValue &value)
{
    if (value.isObject()) {
        QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            it.value() = it.key().compare(QStringLiteral("password"), Qt::CaseInsensitive) == 0
                ? QJsonValue(QStringLiteral("***")) : redactPasswords(it.value());
        }
        return object;
    }
    if (value.isArray()) {
        QJsonArray array;
        for (const auto &item : value.toArray())
            array.append(redactPasswords(item));
        return array;
    }
    return value;
}

} // namespace

QByteArray commandForLog(const QByteArray &bytes)
{
    const QByteArray command = bytes.trimmed();
    if (!command.startsWith('!'))
        return bytes;

    // NOTE: 管理协议以两个单词标识命令，其后的 JSON 参数可能包含凭据。
    qsizetype payloadStart = 0;
    for (int word = 0; word < 2; ++word) {
        while (payloadStart < command.size() &&
               static_cast<unsigned char>(command.at(payloadStart)) > ' ')
            ++payloadStart;
        while (payloadStart < command.size() &&
               static_cast<unsigned char>(command.at(payloadStart)) <= ' ')
            ++payloadStart;
    }
    if (payloadStart == command.size())
        return bytes;

    const QByteArray prefix = command.left(payloadStart);
    const QJsonDocument document = QJsonDocument::fromJson(command.mid(payloadStart));
    // NOTE: 格式错误时仍隐藏全部参数，避免手动输入或外部转发的凭据泄漏。
    if (!document.isObject())
        return prefix + "***\r";

    const QJsonObject redacted = redactPasswords(document.object()).toObject();
    if (redacted == document.object())
        return bytes;
    return prefix + QJsonDocument(redacted).toJson(QJsonDocument::Compact) + '\r';
}

} // namespace Communication

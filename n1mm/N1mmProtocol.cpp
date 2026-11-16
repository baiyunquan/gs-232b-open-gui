#include "N1mmProtocol.h"
#include "AzimuthScale.h"

#include <QSet>
#include <QXmlStreamReader>

#include <cmath>

namespace {

bool readFields(QXmlStreamReader &xml, N1mm::Command &command, bool firstFieldReady = false)
{
    QSet<QString> fields;
    while (firstFieldReady || xml.readNextStartElement()) {
        firstFieldReady = false;
        const QString field = xml.name().toString();
        if (fields.contains(field))
            return false;
        fields.insert(field);
        const QString value = xml.readElementText().trimmed();
        if (xml.hasError())
            return false;
        if (field == QStringLiteral("rotor")) {
            command.rotors = value.split(QLatin1Char(','));
            for (QString &rotor : command.rotors) {
                rotor = rotor.trimmed();
                if (!N1mm::isValidRotorName(rotor))
                    return false;
            }
        } else if (field == QStringLiteral("goazi") || field == QStringLiteral("offset")) {
            bool ok = false;
            const double angle = value.toDouble(&ok);
            if (!ok || !std::isfinite(angle))
                return false;
            if (field == QStringLiteral("goazi")) {
                if (angle < 0 || angle > AzimuthScale::Maximum)
                    return false;
                command.azimuth = angle;
            } else {
                command.offset = angle;
            }
        } else if (field == QStringLiteral("bidirectional")) {
            if (value != QStringLiteral("0") && value != QStringLiteral("1"))
                return false;
            command.bidirectional = value == QStringLiteral("1");
        } else if (field != QStringLiteral("freqband")) {
            return false;
        }
    }
    return fields.contains(QStringLiteral("rotor")) &&
           (command.kind == N1mm::Command::Kind::Stop || fields.contains(QStringLiteral("goazi")));
}

} // namespace

bool N1mm::isValidRotorName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > MaximumNameLength)
        return false;
    for (const QChar character : name) {
        if (character == QLatin1Char(',') || character == QLatin1Char('@') ||
            (character.isSpace() && character != QLatin1Char(' ')) ||
            character.category() == QChar::Other_Control)
            return false;
    }
    return true;
}

std::optional<N1mm::Command> N1mm::parseCommand(const QByteArray &datagram)
{
    if (datagram.isEmpty() || datagram.size() > 8192)
        return std::nullopt;
    QXmlStreamReader xml(datagram);
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::DTD)
            return std::nullopt;
        if (token == QXmlStreamReader::StartElement)
            break;
    }
    if (!xml.isStartElement() || xml.name() != QStringLiteral("N1MMRotor"))
        return std::nullopt;

    Command command;
    // NOTE: 官方停止报文把字段嵌套在 stop 中，不能把 stop 当作普通布尔字段解析。
    if (!xml.readNextStartElement())
        return std::nullopt;
    const bool stop = xml.name() == QStringLiteral("stop");
    if (stop) {
        command.kind = Command::Kind::Stop;
        if (!readFields(xml, command) || xml.readNextStartElement())
            return std::nullopt;
    } else if (!readFields(xml, command, true)) {
        return std::nullopt;
    }
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::DTD)
            return std::nullopt;
    }
    return xml.hasError() ? std::nullopt : std::optional<Command>(command);
}

int N1mm::mechanicalTarget(const Command &command, int current, int localOffset, bool shortestPath)
{
    const AzimuthScale scale(localOffset);
    // NOTE: 先约减外部偏移，防止极端有限浮点数在整数换算时溢出；保留 goazi 的连续行程。
    const double heading = command.azimuth - std::fmod(command.offset, 360.0);
    const auto targetFor = [&](double bearing) {
        const qint64 rounded = qRound64(bearing);
        if (!shortestPath) {
            if (const auto direct = scale.mechanical(rounded))
                return *direct;
        }
        return scale.mechanicalBearing(rounded, shortestPath ? current : 0);
    };
    const int target = targetFor(heading);
    if (!command.bidirectional)
        return target;
    const int reverse = targetFor(heading + 180);
    return qAbs(reverse - current) < qAbs(target - current) ? reverse : target;
}

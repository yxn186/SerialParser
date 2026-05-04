#include "HexUtil.h"

#include <QRegularExpression>
#include <QStringList>

bool HexUtil::parseHexString(const QString &text, QByteArray *outBytes, QString *errorMessage)
{
    if (outBytes) {
        outBytes->clear();
    }

    QString normalized = text.trimmed();
    if (normalized.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "HEX 字符串为空";
        }
        return false;
    }

    normalized.replace(',', ' ');
    normalized.replace(';', ' ');

    QStringList tokens = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QByteArray result;

    auto appendHexToken = [&](QString token) -> bool {
        token = token.trimmed();
        if (token.startsWith("0x", Qt::CaseInsensitive)) {
            token = token.mid(2);
        }
        if (token.isEmpty()) {
            if (errorMessage) {
                *errorMessage = "存在空的 HEX 字节";
            }
            return false;
        }
        if (!QRegularExpression("^[0-9a-fA-F]+$").match(token).hasMatch()) {
            if (errorMessage) {
                *errorMessage = QString("非法 HEX 字符：%1").arg(token);
            }
            return false;
        }

        // 支持 AA55 这种连续写法，也支持 A5 这种单字节写法。
        if (token.length() > 2) {
            if (token.length() % 2 != 0) {
                if (errorMessage) {
                    *errorMessage = QString("连续 HEX 字符长度必须为偶数：%1").arg(token);
                }
                return false;
            }
            for (int i = 0; i < token.length(); i += 2) {
                bool ok = false;
                int value = token.mid(i, 2).toInt(&ok, 16);
                if (!ok || value < 0 || value > 0xFF) {
                    if (errorMessage) {
                        *errorMessage = QString("非法 HEX 字节：%1").arg(token.mid(i, 2));
                    }
                    return false;
                }
                result.append(static_cast<char>(value));
            }
            return true;
        }

        bool ok = false;
        int value = token.toInt(&ok, 16);
        if (!ok || value < 0 || value > 0xFF) {
            if (errorMessage) {
                *errorMessage = QString("非法 HEX 字节：%1").arg(token);
            }
            return false;
        }
        result.append(static_cast<char>(value));
        return true;
    };

    for (const QString &token : tokens) {
        if (!appendHexToken(token)) {
            return false;
        }
    }

    if (result.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "HEX 字符串没有解析出任何字节";
        }
        return false;
    }

    if (outBytes) {
        *outBytes = result;
    }
    return true;
}

bool HexUtil::isValidHexString(const QString &text, QString *errorMessage)
{
    QByteArray ignored;
    return parseHexString(text, &ignored, errorMessage);
}

QString HexUtil::bytesToHexString(const QByteArray &data, int maxBytes)
{
    const int count = (maxBytes > 0) ? qMin(maxBytes, data.size()) : data.size();
    QStringList parts;
    parts.reserve(count);
    for (int i = 0; i < count; ++i) {
        parts << QString("%1").arg(static_cast<unsigned char>(data.at(i)), 2, 16, QLatin1Char('0')).toUpper();
    }
    if (maxBytes > 0 && data.size() > maxBytes) {
        parts << "...";
    }
    return parts.join(' ');
}


#pragma once

#include <QByteArray>
#include <QString>

class HexUtil
{
public:
    static bool parseHexString(const QString &text, QByteArray *outBytes, QString *errorMessage = nullptr);
    static bool isValidHexString(const QString &text, QString *errorMessage = nullptr);
    static QString bytesToHexString(const QByteArray &data, int maxBytes = -1);
};


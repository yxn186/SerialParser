#pragma once

#include <QByteArray>
#include <QString>

class EncodingUtil
{
public:
    static QString decode(const QByteArray &data, const QString &encoding, QString *warning = nullptr);
    static QByteArray encode(const QString &text, const QString &encoding, QString *warning = nullptr);
    static QString normalizeEncodingName(const QString &encoding);
};


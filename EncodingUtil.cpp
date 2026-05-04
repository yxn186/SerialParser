#include "EncodingUtil.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString EncodingUtil::normalizeEncodingName(const QString &encoding)
{
    const QString e = encoding.trimmed().toLower();
    if (e.contains("gb18030")) {
        return "GB18030";
    }
    if (e.contains("gbk") || e.contains("936")) {
        return "GBK";
    }
    if (e.contains("local") || e.contains("ansi") || e.contains("本地")) {
        return "Local8Bit";
    }
    if (e.contains("latin")) {
        return "Latin1";
    }
    return "UTF-8";
}

#ifdef Q_OS_WIN
static QString decodeByCodePage(const QByteArray &data, UINT codePage)
{
    if (data.isEmpty()) {
        return QString();
    }
    int wideLength = MultiByteToWideChar(codePage, 0, data.constData(), data.size(), nullptr, 0);
    if (wideLength <= 0) {
        return QString();
    }
    QString result;
    result.resize(wideLength);
    MultiByteToWideChar(codePage, 0, data.constData(), data.size(),
                        reinterpret_cast<wchar_t *>(result.data()), wideLength);
    return result;
}

static QByteArray encodeByCodePage(const QString &text, UINT codePage)
{
    if (text.isEmpty()) {
        return QByteArray();
    }
    int byteLength = WideCharToMultiByte(codePage, 0,
                                         reinterpret_cast<const wchar_t *>(text.utf16()), text.size(),
                                         nullptr, 0, nullptr, nullptr);
    if (byteLength <= 0) {
        return QByteArray();
    }
    QByteArray result(byteLength, Qt::Uninitialized);
    WideCharToMultiByte(codePage, 0,
                        reinterpret_cast<const wchar_t *>(text.utf16()), text.size(),
                        result.data(), byteLength, nullptr, nullptr);
    return result;
}
#endif

QString EncodingUtil::decode(const QByteArray &data, const QString &encoding, QString *warning)
{
    if (warning) {
        warning->clear();
    }

    const QString normalized = normalizeEncodingName(encoding);
    if (normalized == "UTF-8") {
        return QString::fromUtf8(data);
    }
    if (normalized == "Local8Bit") {
        return QString::fromLocal8Bit(data);
    }
    if (normalized == "Latin1") {
        return QString::fromLatin1(data);
    }

#ifdef Q_OS_WIN
    if (normalized == "GB18030") {
        QString decoded = decodeByCodePage(data, 54936);
        if (!decoded.isNull()) {
            return decoded;
        }
        if (warning) {
            *warning = "GB18030 解码失败，已回退到 GBK/CP936";
        }
        decoded = decodeByCodePage(data, 936);
        if (!decoded.isNull()) {
            return decoded;
        }
    }
    if (normalized == "GBK") {
        QString decoded = decodeByCodePage(data, 936);
        if (!decoded.isNull()) {
            return decoded;
        }
        if (warning) {
            *warning = "GBK/CP936 解码失败，已回退到 Local8Bit";
        }
    }
#endif

    return QString::fromLocal8Bit(data);
}

QByteArray EncodingUtil::encode(const QString &text, const QString &encoding, QString *warning)
{
    if (warning) {
        warning->clear();
    }

    const QString normalized = normalizeEncodingName(encoding);
    if (normalized == "UTF-8") {
        return text.toUtf8();
    }
    if (normalized == "Local8Bit") {
        return text.toLocal8Bit();
    }
    if (normalized == "Latin1") {
        return text.toLatin1();
    }

#ifdef Q_OS_WIN
    if (normalized == "GB18030") {
        QByteArray encoded = encodeByCodePage(text, 54936);
        if (!encoded.isEmpty() || text.isEmpty()) {
            return encoded;
        }
        if (warning) {
            *warning = "GB18030 编码失败，已回退到 GBK/CP936";
        }
        encoded = encodeByCodePage(text, 936);
        if (!encoded.isEmpty() || text.isEmpty()) {
            return encoded;
        }
    }
    if (normalized == "GBK") {
        QByteArray encoded = encodeByCodePage(text, 936);
        if (!encoded.isEmpty() || text.isEmpty()) {
            return encoded;
        }
        if (warning) {
            *warning = "GBK/CP936 编码失败，已回退到 Local8Bit";
        }
    }
#endif

    return text.toLocal8Bit();
}

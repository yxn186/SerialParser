#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct CrcConfig
{
    bool enabled = false;
    QString type = "none";
    int offset = 0;
    int length = 0;
    int rangeStart = 0;
    int rangeLength = 0;
};

struct FieldConfig
{
    QString name;
    QString type = "uint8";
    int offset = 0;
    int length = 1;
    double scale = 1.0;
    double bias = 0.0;
    QString unit;
    int decimals = 0;
    bool hasMin = false;
    bool hasMax = false;
    double minValue = 0.0;
    double maxValue = 0.0;
    QString display = "number";
    QMap<QString, QString> enumMap;
    bool visible = true;
    bool plot = false;
};

struct SerialDefaults
{
    int baudrate = 115200;
    int dataBits = 8;
    QString stopBits = "1";
    QString parity = "None";
};

struct RawDisplaySettings
{
    QString mode = "HEX";
    QString encoding = "UTF-8";
    bool paused = false;
    bool autoScroll = true;
    bool showTimestamp = true;
    int maxLines = 200;
};

struct CurveSettings
{
    int timeWindowSeconds = 60;
    int maxPoints = 2000;
    bool autoScroll = true;
    bool autoScaleY = true;
    double manualYMin = -10.0;
    double manualYMax = 10.0;
};

struct FieldValue
{
    QString name;
    QString type;
    QString displayValue;
    QString rawValue;
    QString unit;
    bool abnormal = false;
    QString statusMessage = "正常";
    bool hasNumericValue = false;
    double numericValue = 0.0;
};

struct ParseResult
{
    bool valid = false;
    QString errorMessage;
    QByteArray rawFrame;
    QVector<FieldValue> fieldValues;
    QDateTime timestamp;
};

struct ParserStats
{
    quint64 totalBytes = 0;
    quint64 candidateFrames = 0;
    quint64 validFrames = 0;
    quint64 headerErrorCount = 0;
    quint64 tailErrorCount = 0;
    quint64 crcErrorCount = 0;
    quint64 lengthErrorCount = 0;
    quint64 fieldErrorCount = 0;
    quint64 discardedBytes = 0;
    int rxBufferLength = 0;
    double frameRateHz = 0.0;
    QDateTime lastValidFrameTime;
};

class ProtocolConfig
{
public:
    QString profileName = "STM32_Remote_V1";
    int frameLength = 20;
    QString headerText = "A5";
    QString tailText = "5A";
    QString endian = "little";
    QString frameMode = "search_header";
    CrcConfig crc;
    SerialDefaults serial;
    RawDisplaySettings rawDisplay;
    CurveSettings curve;
    QVector<FieldConfig> fields;

    QByteArray headerBytes() const;
    QByteArray tailBytes() const;
    bool validate(QStringList *errors = nullptr) const;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &object, ProtocolConfig *config, QStringList *errors = nullptr);
    static ProtocolConfig defaultRemoteV1();

    static int typeDefaultLength(const QString &type);
    static QStringList supportedFieldTypes();
    static QString enumMapToString(const QMap<QString, QString> &map);
    static QMap<QString, QString> enumMapFromString(const QString &text);
};

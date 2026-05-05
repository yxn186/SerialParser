#include "ProtocolParser.h"

#include "CrcUtil.h"
#include "HexUtil.h"

#include <QDateTime>
#include <QtMath>

#include <cstring>

ProtocolParser::ProtocolParser(QObject *parent)
    : QObject(parent)
{
    m_config = ProtocolConfig::defaultRemoteV1();
}

bool ProtocolParser::setConfig(const ProtocolConfig &config, QStringList *errors)
{
    QStringList validationErrors;
    if (!config.validate(&validationErrors)) {
        if (errors) {
            *errors = validationErrors;
        }
        return false;
    }
    m_config = config;
    clearBuffer();
    emitStats();
    if (errors) {
        errors->clear();
    }
    return true;
}

ProtocolConfig ProtocolParser::config() const
{
    return m_config;
}

ParserStats ProtocolParser::stats() const
{
    return m_stats;
}

int ProtocolParser::rxBufferLength() const
{
    return m_rxBuffer.size();
}

void ProtocolParser::appendData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    m_stats.totalBytes += static_cast<quint64>(data.size());
    m_rxBuffer.append(data);

    if (m_config.frameMode == "strict_fixed") {
        parseStrictFixed();
    } else {
        parseSearchHeader();
    }

    emitStats();
}

void ProtocolParser::clearBuffer()
{
    m_rxBuffer.clear();
    m_stats.rxBufferLength = 0;
}

void ProtocolParser::resetStats()
{
    m_stats = ParserStats{};
    m_validFrameTimes.clear();
    emitStats();
}

void ProtocolParser::parseSearchHeader()
{
    const QByteArray header = m_config.headerBytes();
    const int frameLength = m_config.frameLength;

    while (true) {
        const int headerIndex = m_rxBuffer.indexOf(header);
        if (headerIndex < 0) {
            // 保留末尾可能构成下一次包头的少量字节，其他视为垃圾数据丢弃。
            const int keepBytes = qMax(0, header.size() - 1);
            if (m_rxBuffer.size() > keepBytes) {
                const int discard = m_rxBuffer.size() - keepBytes;
                m_rxBuffer.remove(0, discard);
                m_stats.discardedBytes += static_cast<quint64>(discard);
                emit errorOccurred(QString("未找到包头，丢弃 %1 字节").arg(discard));
            }
            return;
        }

        if (headerIndex > 0) {
            m_rxBuffer.remove(0, headerIndex);
            m_stats.discardedBytes += static_cast<quint64>(headerIndex);
            emit errorOccurred(QString("包头前存在错位数据，丢弃 %1 字节").arg(headerIndex));
        }

        if (m_rxBuffer.size() < frameLength) {
            return;
        }

        const QByteArray frame = m_rxBuffer.left(frameLength);
        m_stats.candidateFrames++;

        ParseResult result;
        if (!parseCandidateFrame(frame, &result)) {
            emit frameParsed(result);
            emit errorOccurred(result.errorMessage);
            if (result.errorMessage.contains("包尾")) {
                // 包尾错误时只丢弃当前包头第一个字节，继续搜索下一包。
                m_rxBuffer.remove(0, 1);
            } else {
                m_rxBuffer.remove(0, frameLength);
            }
            continue;
        }

        m_stats.validFrames++;
        m_stats.lastValidFrameTime = result.timestamp;
        m_validFrameTimes.enqueue(result.timestamp.toMSecsSinceEpoch());
        updateFrameRate();
        emit frameParsed(result);
        m_rxBuffer.remove(0, frameLength);
    }
}

void ProtocolParser::parseStrictFixed()
{
    const int frameLength = m_config.frameLength;
    if (m_rxBuffer.size() < frameLength) {
        return;
    }

    if (m_rxBuffer.size() > frameLength) {
        m_stats.lengthErrorCount++;
        const QString message = QString("strict_fixed 模式下接收缓存长度超过配置包长：RxBuffer=%1, frameLength=%2，已清空缓存")
                                    .arg(m_rxBuffer.size())
                                    .arg(frameLength);
        emit errorOccurred(message);

        ParseResult result;
        result.valid = false;
        result.timestamp = QDateTime::currentDateTime();
        result.errorMessage = message;
        result.rawFrame = m_rxBuffer;
        emit frameParsed(result);

        m_rxBuffer.clear();
        return;
    }

    const QByteArray frame = m_rxBuffer;
    m_stats.candidateFrames++;

    ParseResult result;
    if (!parseCandidateFrame(frame, &result)) {
        emit frameParsed(result);
        emit errorOccurred(result.errorMessage);
        m_rxBuffer.clear();
        return;
    }

    m_stats.validFrames++;
    m_stats.lastValidFrameTime = result.timestamp;
    m_validFrameTimes.enqueue(result.timestamp.toMSecsSinceEpoch());
    updateFrameRate();
    emit frameParsed(result);
    m_rxBuffer.clear();
}

bool ProtocolParser::parseCandidateFrame(const QByteArray &frame, ParseResult *result)
{
    result->timestamp = QDateTime::currentDateTime();
    result->rawFrame = frame;
    result->valid = false;

    const QByteArray header = m_config.headerBytes();
    if (!frame.startsWith(header)) {
        m_stats.headerErrorCount++;
        result->errorMessage = "包头错误：" + HexUtil::bytesToHexString(frame.left(header.size()));
        return false;
    }

    const QByteArray tail = m_config.tailBytes();
    if (!tail.isEmpty() && frame.mid(m_config.frameLength - tail.size(), tail.size()) != tail) {
        m_stats.tailErrorCount++;
        result->errorMessage = "包尾错误：" + HexUtil::bytesToHexString(frame.right(tail.size()));
        return false;
    }

    QString crcError;
    if (!checkCrc(frame, &crcError)) {
        m_stats.crcErrorCount++;
        result->errorMessage = crcError;
        return false;
    }

    QStringList fieldErrors;
    if (!parseFields(frame, &result->fieldValues, &fieldErrors)) {
        m_stats.fieldErrorCount += static_cast<quint64>(fieldErrors.size());
        result->errorMessage = fieldErrors.join("; ");
        return false;
    }

    result->valid = true;
    return true;
}

bool ProtocolParser::checkCrc(const QByteArray &frame, QString *errorMessage) const
{
    if (!m_config.crc.enabled) {
        return true;
    }

    const CrcConfig &crc = m_config.crc;
    if (crc.offset < 0 || crc.length <= 0 || crc.offset + crc.length > frame.size()) {
        if (errorMessage) {
            *errorMessage = "CRC 字段范围超出帧长度";
        }
        return false;
    }
    if (crc.rangeStart < 0 || crc.rangeLength <= 0 || crc.rangeStart + crc.rangeLength > frame.size()) {
        if (errorMessage) {
            *errorMessage = "CRC 计算范围超出帧长度";
        }
        return false;
    }

    const QByteArray range = frame.mid(crc.rangeStart, crc.rangeLength);
    if (crc.type == "sum8") {
        const quint8 expected = static_cast<unsigned char>(frame.at(crc.offset));
        const quint8 actual = CrcUtil::sum8(range);
        if (actual != expected) {
            if (errorMessage) {
                *errorMessage = QString("CRC sum8 错误：计算=%1，帧内=%2")
                                    .arg(actual, 2, 16, QLatin1Char('0'))
                                    .arg(expected, 2, 16, QLatin1Char('0')).toUpper();
            }
            return false;
        }
        return true;
    }

    if (crc.type == "xor8") {
        const quint8 expected = static_cast<unsigned char>(frame.at(crc.offset));
        const quint8 actual = CrcUtil::xor8(range);
        if (actual != expected) {
            if (errorMessage) {
                *errorMessage = QString("CRC xor8 错误：计算=%1，帧内=%2")
                                    .arg(actual, 2, 16, QLatin1Char('0'))
                                    .arg(expected, 2, 16, QLatin1Char('0')).toUpper();
            }
            return false;
        }
        return true;
    }

    if (crc.type == "crc8") {
        const quint8 expected = static_cast<unsigned char>(frame.at(crc.offset));
        const quint8 actual = CrcUtil::crc8(range);
        if (actual != expected) {
            if (errorMessage) {
                *errorMessage = QString("CRC8 错误：计算=%1，帧内=%2")
                                    .arg(actual, 2, 16, QLatin1Char('0'))
                                    .arg(expected, 2, 16, QLatin1Char('0')).toUpper();
            }
            return false;
        }
        return true;
    }

    if (crc.type == "crc16_modbus") {
        const quint16 expected = static_cast<quint16>(static_cast<unsigned char>(frame.at(crc.offset)))
                               | static_cast<quint16>(static_cast<unsigned char>(frame.at(crc.offset + 1)) << 8);
        const quint16 actual = CrcUtil::crc16Modbus(range);
        if (actual != expected) {
            if (errorMessage) {
                *errorMessage = QString("CRC16-Modbus 错误：计算=%1，帧内=%2")
                                    .arg(actual, 4, 16, QLatin1Char('0'))
                                    .arg(expected, 4, 16, QLatin1Char('0')).toUpper();
            }
            return false;
        }
        return true;
    }

    if (errorMessage) {
        *errorMessage = "不支持的 CRC 类型：" + crc.type;
    }
    return false;
}

bool ProtocolParser::parseFields(const QByteArray &frame, QVector<FieldValue> *values, QStringList *fieldErrors) const
{
    values->clear();
    QStringList errors;

    for (const FieldConfig &field : m_config.fields) {
        const int length = (field.type == "raw_hex") ? field.length : ProtocolConfig::typeDefaultLength(field.type);
        FieldValue value;
        value.name = field.name;
        value.type = field.type;
        value.unit = field.unit;

        if (field.offset < 0 || length <= 0 || field.offset + length > frame.size()) {
            value.abnormal = true;
            value.statusMessage = "字段越界";
            value.displayValue = "--";
            errors << QString("字段 %1 越界").arg(field.name);
            if (field.visible) {
                values->append(value);
            }
            continue;
        }

        const QByteArray bytes = frame.mid(field.offset, length);
        value.rawValue = HexUtil::bytesToHexString(bytes);

        bool hasNumeric = true;
        double numericValue = 0.0;
        qint64 integerValue = 0;

        if (field.type == "raw_hex") {
            hasNumeric = false;
            value.displayValue = HexUtil::bytesToHexString(bytes);
        } else if (field.type == "uint8" || field.type == "uint16" || field.type == "uint32") {
            integerValue = static_cast<qint64>(readUnsigned(bytes, m_config.endian));
            numericValue = static_cast<double>(integerValue);
        } else if (field.type == "int8" || field.type == "int16" || field.type == "int32") {
            integerValue = signExtend(readUnsigned(bytes, m_config.endian), length * 8);
            numericValue = static_cast<double>(integerValue);
        } else if (field.type == "bool_uint8") {
            integerValue = static_cast<unsigned char>(bytes.at(0));
            numericValue = static_cast<double>(integerValue);
        } else if (field.type == "float32") {
            QByteArray ordered(4, Qt::Uninitialized);
            if (m_config.endian == "big") {
                for (int i = 0; i < 4; ++i) {
                    ordered[i] = bytes.at(3 - i);
                }
            } else {
                ordered = bytes;
            }
            float raw = 0.0f;
            std::memcpy(&raw, ordered.constData(), sizeof(float));
            numericValue = static_cast<double>(raw);
        } else if (field.type == "float64") {
            QByteArray ordered(8, Qt::Uninitialized);
            if (m_config.endian == "big") {
                for (int i = 0; i < 8; ++i) {
                    ordered[i] = bytes.at(7 - i);
                }
            } else {
                ordered = bytes;
            }
            double raw = 0.0;
            std::memcpy(&raw, ordered.constData(), sizeof(double));
            numericValue = raw;
        }

        if (hasNumeric) {
            if (qIsNaN(numericValue) || qIsInf(numericValue)) {
                value.abnormal = true;
                value.statusMessage = "NaN/Inf";
                errors << QString("字段 %1 解析为 NaN 或 Inf").arg(field.name);
            }

            const double scaled = numericValue * field.scale + field.bias;
            value.hasNumericValue = true;
            value.numericValue = scaled;
            if (!value.abnormal && field.hasMin && scaled < field.minValue) {
                value.abnormal = true;
                value.statusMessage = "低于最小值";
                errors << QString("字段 %1 低于最小值").arg(field.name);
            }
            if (!value.abnormal && field.hasMax && scaled > field.maxValue) {
                value.abnormal = true;
                value.statusMessage = "高于最大值";
                errors << QString("字段 %1 高于最大值").arg(field.name);
            }

            if (field.display == "bool" || field.type == "bool_uint8") {
                value.displayValue = (integerValue != 0) ? "true" : "false";
            } else if (field.display == "enum") {
                const QString key = QString::number(static_cast<qint64>(numericValue));
                value.displayValue = field.enumMap.value(key, key);
            } else if (field.display == "hex") {
                value.displayValue = HexUtil::bytesToHexString(bytes);
            } else {
                value.displayValue = QString::number(scaled, 'f', qMax(0, field.decimals));
            }
        }

        if (!value.abnormal) {
            value.statusMessage = "正常";
        }
        if (field.visible) {
            values->append(value);
        }
    }

    if (fieldErrors) {
        *fieldErrors = errors;
    }
    return errors.isEmpty();
}

void ProtocolParser::updateFrameRate()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_validFrameTimes.isEmpty() && now - m_validFrameTimes.head() > 1000) {
        m_validFrameTimes.dequeue();
    }
    m_stats.frameRateHz = static_cast<double>(m_validFrameTimes.size());
}

void ProtocolParser::emitStats()
{
    updateFrameRate();
    m_stats.rxBufferLength = m_rxBuffer.size();
    emit statisticsChanged(m_stats);
}

quint64 ProtocolParser::readUnsigned(const QByteArray &bytes, const QString &endian)
{
    quint64 value = 0;
    if (endian == "big") {
        for (char ch : bytes) {
            value = (value << 8) | static_cast<unsigned char>(ch);
        }
    } else {
        for (int i = bytes.size() - 1; i >= 0; --i) {
            value = (value << 8) | static_cast<unsigned char>(bytes.at(i));
        }
    }
    return value;
}

qint64 ProtocolParser::signExtend(quint64 value, int bits)
{
    const quint64 signBit = 1ULL << (bits - 1);
    const quint64 mask = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);
    value &= mask;
    if (value & signBit) {
        return static_cast<qint64>(value | (~mask));
    }
    return static_cast<qint64>(value);
}

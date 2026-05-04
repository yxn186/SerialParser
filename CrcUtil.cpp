#include "CrcUtil.h"

quint8 CrcUtil::sum8(const QByteArray &data)
{
    quint32 sum = 0;
    for (char ch : data) {
        sum += static_cast<unsigned char>(ch);
    }
    return static_cast<quint8>(sum & 0xFF);
}

quint8 CrcUtil::xor8(const QByteArray &data)
{
    quint8 value = 0;
    for (char ch : data) {
        value ^= static_cast<unsigned char>(ch);
    }
    return value;
}

quint8 CrcUtil::crc8(const QByteArray &data)
{
    quint8 crc = 0x00;
    for (char ch : data) {
        crc ^= static_cast<unsigned char>(ch);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80) {
                crc = static_cast<quint8>((crc << 1) ^ 0x07);
            } else {
                crc = static_cast<quint8>(crc << 1);
            }
        }
    }
    return crc;
}

quint16 CrcUtil::crc16Modbus(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (char ch : data) {
        crc ^= static_cast<unsigned char>(ch);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<quint16>(crc >> 1);
            }
        }
    }
    return crc;
}


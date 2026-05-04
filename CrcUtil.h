#pragma once

#include <QByteArray>
#include <QtGlobal>

class CrcUtil
{
public:
    static quint8 sum8(const QByteArray &data);
    static quint8 xor8(const QByteArray &data);
    static quint8 crc8(const QByteArray &data);
    static quint16 crc16Modbus(const QByteArray &data);
};


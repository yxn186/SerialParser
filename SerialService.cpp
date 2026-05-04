#include "SerialService.h"

SerialService::SerialService(QObject *parent)
    : QObject(parent)
{
    connect(&m_serialPort, &QSerialPort::readyRead, this, &SerialService::handleReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred, this, &SerialService::handleSerialError);
}

QStringList SerialService::availablePorts() const
{
    QStringList ports;
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ports << info.portName();
    }
    ports.sort();
    return ports;
}

bool SerialService::openPort(const QString &portName, int baudrate, int dataBits, const QString &stopBits, const QString &parity, QString *errorMessage)
{
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }

    m_serialPort.setPortName(portName);
    m_serialPort.setBaudRate(baudrate);
    m_serialPort.setDataBits(toDataBits(dataBits));
    m_serialPort.setStopBits(toStopBits(stopBits));
    m_serialPort.setParity(toParity(parity));
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        if (errorMessage) {
            *errorMessage = m_serialPort.errorString();
        }
        emit errorOccurred("打开串口失败：" + m_serialPort.errorString());
        emit portStateChanged(false);
        return false;
    }

    emit portStateChanged(true);
    return true;
}

void SerialService::closePort()
{
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
    emit portStateChanged(false);
}

bool SerialService::isOpen() const
{
    return m_serialPort.isOpen();
}

bool SerialService::sendData(const QByteArray &data, QString *errorMessage)
{
    if (!m_serialPort.isOpen()) {
        if (errorMessage) {
            *errorMessage = "串口未打开";
        }
        return false;
    }

    const qint64 written = m_serialPort.write(data);
    if (written != data.size()) {
        if (errorMessage) {
            *errorMessage = "串口写入失败：" + m_serialPort.errorString();
        }
        return false;
    }
    if (!m_serialPort.flush()) {
        if (errorMessage) {
            *errorMessage = "串口刷新发送缓冲失败：" + m_serialPort.errorString();
        }
        return false;
    }
    return true;
}

QString SerialService::portName() const
{
    return m_serialPort.portName();
}

void SerialService::handleReadyRead()
{
    const QByteArray data = m_serialPort.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialService::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }
    emit errorOccurred("串口错误：" + m_serialPort.errorString());
}

QSerialPort::DataBits SerialService::toDataBits(int dataBits)
{
    switch (dataBits) {
    case 5: return QSerialPort::Data5;
    case 6: return QSerialPort::Data6;
    case 7: return QSerialPort::Data7;
    default: return QSerialPort::Data8;
    }
}

QSerialPort::StopBits SerialService::toStopBits(const QString &stopBits)
{
    if (stopBits == "1.5") {
        return QSerialPort::OneAndHalfStop;
    }
    if (stopBits == "2") {
        return QSerialPort::TwoStop;
    }
    return QSerialPort::OneStop;
}

QSerialPort::Parity SerialService::toParity(const QString &parity)
{
    if (parity == "Even") {
        return QSerialPort::EvenParity;
    }
    if (parity == "Odd") {
        return QSerialPort::OddParity;
    }
    if (parity == "Mark") {
        return QSerialPort::MarkParity;
    }
    if (parity == "Space") {
        return QSerialPort::SpaceParity;
    }
    return QSerialPort::NoParity;
}


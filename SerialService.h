#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QStringList>

class SerialService : public QObject
{
    Q_OBJECT

public:
    explicit SerialService(QObject *parent = nullptr);

    QStringList availablePorts() const;
    bool openPort(const QString &portName, int baudrate, int dataBits, const QString &stopBits, const QString &parity, QString *errorMessage = nullptr);
    void closePort();
    bool isOpen() const;
    bool sendData(const QByteArray &data, QString *errorMessage = nullptr);
    QString portName() const;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void portStateChanged(bool opened);

private slots:
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);

private:
    static QSerialPort::DataBits toDataBits(int dataBits);
    static QSerialPort::StopBits toStopBits(const QString &stopBits);
    static QSerialPort::Parity toParity(const QString &parity);

    QSerialPort m_serialPort;
};


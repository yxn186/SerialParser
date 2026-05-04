#pragma once

#include "ProtocolConfig.h"

#include <QObject>
#include <QQueue>

class ProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolParser(QObject *parent = nullptr);

    bool setConfig(const ProtocolConfig &config, QStringList *errors = nullptr);
    ProtocolConfig config() const;
    ParserStats stats() const;
    int rxBufferLength() const;
    void appendData(const QByteArray &data);
    void clearBuffer();
    void resetStats();

signals:
    void frameParsed(const ParseResult &result);
    void errorOccurred(const QString &message);
    void statisticsChanged(const ParserStats &stats);

private:
    void parseSearchHeader();
    void parseStrictFixed();
    bool parseCandidateFrame(const QByteArray &frame, ParseResult *result);
    bool checkCrc(const QByteArray &frame, QString *errorMessage) const;
    bool parseFields(const QByteArray &frame, QVector<FieldValue> *values, QStringList *fieldErrors) const;
    void updateFrameRate();
    void emitStats();
    static quint64 readUnsigned(const QByteArray &bytes, const QString &endian);
    static qint64 signExtend(quint64 value, int bits);

    ProtocolConfig m_config;
    QByteArray m_rxBuffer;
    ParserStats m_stats;
    QQueue<qint64> m_validFrameTimes;
};


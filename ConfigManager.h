#pragma once

#include "ProtocolConfig.h"

#include <QObject>
#include <QString>
#include <QVector>

struct ConfigInfo
{
    QString profileName;
    QString filePath;
};

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    QString configDirPath() const;
    QString defaultConfigPath() const;
    QVector<ConfigInfo> scanConfigs(QStringList *warnings = nullptr);
    bool loadConfig(const QString &filePath, ProtocolConfig *config, QStringList *errors = nullptr) const;
    bool saveConfig(const QString &filePath, const ProtocolConfig &config, QString *errorMessage = nullptr) const;
    bool ensureDefaultConfig(QString *errorMessage = nullptr) const;

private:
    QString m_configDirPath;
};


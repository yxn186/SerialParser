#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    const QString appConfigDir = QDir(QCoreApplication::applicationDirPath()).filePath("configs");
    const QString currentConfigDir = QDir(QDir::currentPath()).filePath("configs");
    if (QDir(appConfigDir).exists() || !QDir(currentConfigDir).exists()) {
        m_configDirPath = appConfigDir;
    } else {
        m_configDirPath = currentConfigDir;
    }
}

QString ConfigManager::configDirPath() const
{
    return m_configDirPath;
}

QString ConfigManager::defaultConfigPath() const
{
    return QDir(m_configDirPath).filePath("remote_v1.json");
}

bool ConfigManager::ensureDefaultConfig(QString *errorMessage) const
{
    QDir dir(m_configDirPath);
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = "无法创建 configs 目录：" + m_configDirPath;
        }
        return false;
    }

    if (QFile::exists(defaultConfigPath())) {
        return true;
    }

    return saveConfig(defaultConfigPath(), ProtocolConfig::defaultRemoteV1(), errorMessage);
}

QVector<ConfigInfo> ConfigManager::scanConfigs(QStringList *warnings)
{
    if (warnings) {
        warnings->clear();
    }

    QString ensureError;
    if (!ensureDefaultConfig(&ensureError) && warnings) {
        *warnings << ensureError;
    }

    QVector<ConfigInfo> result;
    QDir dir(m_configDirPath);
    const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        ProtocolConfig config;
        QStringList errors;
        if (!loadConfig(fileInfo.absoluteFilePath(), &config, &errors)) {
            if (warnings) {
                *warnings << QString("配置文件加载失败，已跳过：%1\n%2")
                                 .arg(fileInfo.fileName(), errors.join("\n"));
            }
            continue;
        }
        result.append({config.profileName, fileInfo.absoluteFilePath()});
    }

    bool hasDefault = false;
    for (const ConfigInfo &info : result) {
        if (QFileInfo(info.filePath).fileName() == "remote_v1.json") {
            hasDefault = true;
            break;
        }
    }

    // 如果默认配置损坏，覆盖写入内置默认配置，保证程序至少有一个可用配置。
    if (!hasDefault) {
        QString saveError;
        if (saveConfig(defaultConfigPath(), ProtocolConfig::defaultRemoteV1(), &saveError)) {
            result.prepend({"STM32_Remote_V1", defaultConfigPath()});
            if (warnings) {
                *warnings << "默认 remote_v1.json 不可用，已使用内置默认配置重新生成。";
            }
        } else if (warnings) {
            *warnings << "默认 remote_v1.json 重新生成失败：" + saveError;
        }
    }

    return result;
}

bool ConfigManager::loadConfig(const QString &filePath, ProtocolConfig *config, QStringList *errors) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errors) {
            *errors << "无法读取文件：" + file.errorString();
        }
        return false;
    }

    const QByteArray data = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errors) {
            *errors << QString("JSON 解析失败：%1").arg(parseError.errorString());
        }
        return false;
    }

    return ProtocolConfig::fromJson(document.object(), config, errors);
}

bool ConfigManager::saveConfig(const QString &filePath, const ProtocolConfig &config, QString *errorMessage) const
{
    QStringList validationErrors;
    if (!config.validate(&validationErrors)) {
        if (errorMessage) {
            *errorMessage = validationErrors.join("\n");
        }
        return false;
    }

    QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = "无法创建配置目录：" + dir.absolutePath();
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = "无法写入配置文件：" + file.errorString();
        }
        return false;
    }

    const QJsonDocument document(config.toJson());
    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

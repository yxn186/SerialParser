#include "ProtocolConfig.h"

#include "HexUtil.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

static QJsonObject serialToJson(const SerialDefaults &serial)
{
    QJsonObject object;
    object["baudrate"] = serial.baudrate;
    object["dataBits"] = serial.dataBits;
    object["stopBits"] = serial.stopBits;
    object["parity"] = serial.parity;
    return object;
}

static SerialDefaults serialFromJson(const QJsonObject &object)
{
    SerialDefaults serial;
    serial.baudrate = object.value("baudrate").toInt(115200);
    serial.dataBits = object.value("dataBits").toInt(8);
    serial.stopBits = object.value("stopBits").toString("1");
    serial.parity = object.value("parity").toString("None");
    return serial;
}

static QJsonObject rawDisplayToJson(const RawDisplaySettings &settings)
{
    QJsonObject object;
    object["mode"] = settings.mode;
    object["encoding"] = settings.encoding;
    object["paused"] = settings.paused;
    object["autoScroll"] = settings.autoScroll;
    object["showTimestamp"] = settings.showTimestamp;
    object["maxLines"] = settings.maxLines;
    return object;
}

static RawDisplaySettings rawDisplayFromJson(const QJsonObject &object)
{
    RawDisplaySettings settings;
    settings.mode = object.value("mode").toString("HEX");
    settings.encoding = object.value("encoding").toString("UTF-8");
    settings.paused = object.value("paused").toBool(false);
    settings.autoScroll = object.value("autoScroll").toBool(true);
    settings.showTimestamp = object.value("showTimestamp").toBool(true);
    settings.maxLines = object.value("maxLines").toInt(200);
    return settings;
}

QByteArray ProtocolConfig::headerBytes() const
{
    QByteArray bytes;
    HexUtil::parseHexString(headerText, &bytes);
    return bytes;
}

QByteArray ProtocolConfig::tailBytes() const
{
    QByteArray bytes;
    HexUtil::parseHexString(tailText, &bytes);
    return bytes;
}

int ProtocolConfig::typeDefaultLength(const QString &type)
{
    const QString t = type.trimmed().toLower();
    if (t == "uint8" || t == "int8" || t == "bool_uint8") {
        return 1;
    }
    if (t == "uint16" || t == "int16") {
        return 2;
    }
    if (t == "uint32" || t == "int32" || t == "float32") {
        return 4;
    }
    if (t == "float64") {
        return 8;
    }
    if (t == "raw_hex") {
        return 0;
    }
    return -1;
}

QStringList ProtocolConfig::supportedFieldTypes()
{
    return {"uint8", "int8", "uint16", "int16", "uint32", "int32", "float32", "float64", "bool_uint8", "raw_hex"};
}

QString ProtocolConfig::enumMapToString(const QMap<QString, QString> &map)
{
    QStringList parts;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        parts << QString("%1=%2").arg(it.key(), it.value());
    }
    return parts.join(';');
}

QMap<QString, QString> ProtocolConfig::enumMapFromString(const QString &text)
{
    QMap<QString, QString> result;
    const QStringList parts = text.split(';', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const int index = part.indexOf('=');
        if (index <= 0) {
            continue;
        }
        const QString key = part.left(index).trimmed();
        const QString value = part.mid(index + 1).trimmed();
        if (!key.isEmpty()) {
            result.insert(key, value);
        }
    }
    return result;
}

bool ProtocolConfig::validate(QStringList *errors) const
{
    QStringList localErrors;

    if (profileName.trimmed().isEmpty()) {
        localErrors << "profileName 不能为空";
    }
    if (frameLength <= 0) {
        localErrors << "frameLength 必须大于 0";
    }

    QByteArray header;
    QString hexError;
    if (!HexUtil::parseHexString(headerText, &header, &hexError) || header.isEmpty()) {
        localErrors << QString("header 非法：%1").arg(hexError);
    }

    QByteArray tail;
    if (!HexUtil::parseHexString(tailText, &tail, &hexError) || tail.isEmpty()) {
        localErrors << QString("tail 非法：%1").arg(hexError);
    }

    if (!header.isEmpty() && header.size() > frameLength) {
        localErrors << "header 长度不能超过 frameLength";
    }
    if (!tail.isEmpty() && tail.size() > frameLength) {
        localErrors << "tail 长度不能超过 frameLength";
    }

    const QString endianLower = endian.trimmed().toLower();
    if (endianLower != "little" && endianLower != "big") {
        localErrors << "endian 只支持 little 或 big";
    }

    if (frameMode != "search_header" && frameMode != "strict_fixed") {
        localErrors << "frameMode 只支持 search_header 或 strict_fixed";
    }

    if (crc.enabled) {
        const QSet<QString> crcTypes = {"sum8", "xor8", "crc8", "crc16_modbus"};
        if (!crcTypes.contains(crc.type)) {
            localErrors << "CRC type 只支持 sum8、xor8、crc8、crc16_modbus";
        }
        if (crc.length <= 0 || crc.offset < 0 || crc.offset + crc.length > frameLength) {
            localErrors << "CRC 字段 offset/length 超出 frameLength";
        }
        if (crc.rangeStart < 0 || crc.rangeLength <= 0 || crc.rangeStart + crc.rangeLength > frameLength) {
            localErrors << "CRC 计算范围 rangeStart/rangeLength 超出 frameLength";
        }
        if ((crc.type == "sum8" || crc.type == "xor8" || crc.type == "crc8") && crc.length != 1) {
            localErrors << "sum8/xor8/crc8 的 CRC length 必须为 1";
        }
        if (crc.type == "crc16_modbus" && crc.length != 2) {
            localErrors << "crc16_modbus 的 CRC length 必须为 2";
        }
    }

    for (const FieldConfig &field : fields) {
        const QString prefix = QString("字段 [%1]：").arg(field.name.isEmpty() ? "(未命名)" : field.name);
        if (field.name.trimmed().isEmpty()) {
            localErrors << "字段名不能为空";
        }
        const int defaultLength = typeDefaultLength(field.type);
        if (defaultLength < 0) {
            localErrors << prefix + "不支持的字段类型 " + field.type;
            continue;
        }
        const int actualLength = (field.type == "raw_hex") ? field.length : defaultLength;
        if (field.type == "raw_hex" && actualLength <= 0) {
            localErrors << prefix + "raw_hex 必须填写合法 length";
        }
        if (field.type != "raw_hex" && field.length != defaultLength) {
            localErrors << prefix + QString("length 应为 %1").arg(defaultLength);
        }
        if (field.offset < 0 || actualLength <= 0 || field.offset + actualLength > frameLength) {
            localErrors << prefix + "offset + length 超出 frameLength";
        }
        if (field.hasMin && field.hasMax && field.minValue > field.maxValue) {
            localErrors << prefix + "min 不能大于 max";
        }
    }

    if (rawDisplay.maxLines <= 0) {
        localErrors << "原始数据显示最大行数必须大于 0";
    }

    if (errors) {
        *errors = localErrors;
    }
    return localErrors.isEmpty();
}

QJsonObject ProtocolConfig::toJson() const
{
    QJsonObject object;
    object["profileName"] = profileName;
    object["frameLength"] = frameLength;
    object["header"] = headerText;
    object["tail"] = tailText;
    object["endian"] = endian;
    object["frameMode"] = frameMode;
    object["serial"] = serialToJson(serial);
    object["rawDisplay"] = rawDisplayToJson(rawDisplay);

    QJsonObject crcObject;
    crcObject["enabled"] = crc.enabled;
    crcObject["type"] = crc.type;
    crcObject["offset"] = crc.offset;
    crcObject["length"] = crc.length;
    crcObject["rangeStart"] = crc.rangeStart;
    crcObject["rangeLength"] = crc.rangeLength;
    object["crc"] = crcObject;

    QJsonArray fieldArray;
    for (const FieldConfig &field : fields) {
        QJsonObject item;
        item["name"] = field.name;
        item["type"] = field.type;
        item["offset"] = field.offset;
        item["length"] = field.length;
        item["scale"] = field.scale;
        item["bias"] = field.bias;
        item["unit"] = field.unit;
        item["decimals"] = field.decimals;
        if (field.hasMin) {
            item["min"] = field.minValue;
        }
        if (field.hasMax) {
            item["max"] = field.maxValue;
        }
        item["display"] = field.display;
        if (!field.enumMap.isEmpty()) {
            QJsonObject enumObject;
            for (auto it = field.enumMap.constBegin(); it != field.enumMap.constEnd(); ++it) {
                enumObject[it.key()] = it.value();
            }
            item["enumMap"] = enumObject;
        }
        item["visible"] = field.visible;
        item["plot"] = field.plot;
        fieldArray.append(item);
    }
    object["fields"] = fieldArray;

    return object;
}

bool ProtocolConfig::fromJson(const QJsonObject &object, ProtocolConfig *config, QStringList *errors)
{
    if (!config) {
        return false;
    }

    ProtocolConfig parsed;
    parsed.profileName = object.value("profileName").toString();
    parsed.frameLength = object.value("frameLength").toInt(0);
    parsed.headerText = object.value("header").toString();
    parsed.tailText = object.value("tail").toString();
    parsed.endian = object.value("endian").toString("little");
    parsed.frameMode = object.value("frameMode").toString("search_header");
    parsed.serial = serialFromJson(object.value("serial").toObject());
    parsed.rawDisplay = rawDisplayFromJson(object.value("rawDisplay").toObject());

    const QJsonObject crcObject = object.value("crc").toObject();
    parsed.crc.enabled = crcObject.value("enabled").toBool(false);
    parsed.crc.type = crcObject.value("type").toString("none");
    parsed.crc.offset = crcObject.value("offset").toInt(0);
    parsed.crc.length = crcObject.value("length").toInt(0);
    parsed.crc.rangeStart = crcObject.value("rangeStart").toInt(0);
    parsed.crc.rangeLength = crcObject.value("rangeLength").toInt(0);

    const QJsonArray fieldArray = object.value("fields").toArray();
    for (const QJsonValue &value : fieldArray) {
        const QJsonObject item = value.toObject();
        FieldConfig field;
        field.name = item.value("name").toString();
        field.type = item.value("type").toString("uint8");
        field.offset = item.value("offset").toInt(0);
        field.length = item.contains("length") ? item.value("length").toInt(0) : typeDefaultLength(field.type);
        if (field.type != "raw_hex") {
            field.length = typeDefaultLength(field.type);
        }
        field.scale = item.value("scale").toDouble(1.0);
        field.bias = item.value("bias").toDouble(0.0);
        field.unit = item.value("unit").toString();
        field.decimals = item.value("decimals").toInt(0);
        field.hasMin = item.contains("min");
        field.hasMax = item.contains("max");
        field.minValue = item.value("min").toDouble(0.0);
        field.maxValue = item.value("max").toDouble(0.0);
        field.display = item.value("display").toString(field.type == "bool_uint8" ? "bool" : "number");
        const QJsonObject enumObject = item.value("enumMap").toObject();
        for (auto it = enumObject.constBegin(); it != enumObject.constEnd(); ++it) {
            field.enumMap.insert(it.key(), it.value().toString());
        }
        field.visible = item.value("visible").toBool(true);
        field.plot = item.value("plot").toBool(false);
        parsed.fields.append(field);
    }

    QStringList validationErrors;
    if (!parsed.validate(&validationErrors)) {
        if (errors) {
            *errors = validationErrors;
        }
        return false;
    }

    *config = parsed;
    if (errors) {
        errors->clear();
    }
    return true;
}

ProtocolConfig ProtocolConfig::defaultRemoteV1()
{
    ProtocolConfig config;
    config.profileName = "STM32_Remote_V1";
    config.frameLength = 20;
    config.headerText = "A5";
    config.tailText = "5A";
    config.endian = "little";
    config.frameMode = "search_header";
    config.serial = SerialDefaults{};
    config.rawDisplay = RawDisplaySettings{};

    auto makeBool = [](const QString &name, int offset) {
        FieldConfig field;
        field.name = name;
        field.type = "bool_uint8";
        field.offset = offset;
        field.length = 1;
        field.display = "bool";
        field.visible = true;
        return field;
    };

    config.fields = {
        makeBool("K1", 1),
        makeBool("K2", 2),
        makeBool("K3", 3),
        makeBool("LB", 4),
        makeBool("RB", 5)
    };

    auto makeFloat = [](const QString &name, int offset) {
        FieldConfig field;
        field.name = name;
        field.type = "float32";
        field.offset = offset;
        field.length = 4;
        field.decimals = 3;
        field.display = "number";
        field.plot = true;
        return field;
    };

    config.fields.append(makeFloat("Vx", 6));
    config.fields.append(makeFloat("Vy", 10));
    config.fields.append(makeFloat("Wz", 14));

    FieldConfig mode;
    mode.name = "Mode";
    mode.type = "uint8";
    mode.offset = 18;
    mode.length = 1;
    mode.display = "enum";
    mode.enumMap.insert("0", "自动模式");
    mode.enumMap.insert("1", "遥控模式");
    mode.enumMap.insert("2", "调试模式");
    config.fields.append(mode);

    return config;
}


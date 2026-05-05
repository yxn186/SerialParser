#include "MainWindow.h"

#include "EncodingUtil.h"
#include "HexUtil.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCompleter>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int FieldNameColumn = 0;
constexpr int FieldTypeColumn = 1;
constexpr int FieldOffsetColumn = 2;
constexpr int FieldLengthColumn = 3;
constexpr int FieldScaleColumn = 4;
constexpr int FieldBiasColumn = 5;
constexpr int FieldUnitColumn = 6;
constexpr int FieldDecimalsColumn = 7;
constexpr int FieldMinColumn = 8;
constexpr int FieldMaxColumn = 9;
constexpr int FieldDisplayColumn = 10;
constexpr int FieldEnumMapColumn = 11;
constexpr int FieldVisibleColumn = 12;
constexpr int FieldPlotColumn = 13;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    loadStyleSheet();
    setupConnections();

    m_logEdit->document()->setMaximumBlockCount(300);
    refreshPorts();
    refreshConfigList();
    if (m_profileCombo->count() > 0) {
        loadSelectedConfig();
    } else {
        applyConfigToUi(ProtocolConfig::defaultRemoteV1());
        m_parser.setConfig(m_config);
    }

    m_statusTimer.setInterval(500);
    connect(&m_statusTimer, &QTimer::timeout, this, &MainWindow::updateOnlineState);
    m_statusTimer.start();
}

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *root = new QVBoxLayout(central);
    root->setContentsMargins(14, 12, 14, 10);
    root->setSpacing(10);

    root->addWidget(setupHeader());

    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, central);

    QWidget *leftPanel = new QWidget(mainSplitter);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(setupSerialPanel());
    leftLayout->addWidget(setupConfigPanel());
    leftLayout->addStretch();

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(setupValuePanel());
    mainSplitter->addWidget(setupProtocolTabs());
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 2);
    mainSplitter->setStretchFactor(2, 2);
    mainSplitter->setSizes({280, 560, 620});

    root->addWidget(mainSplitter, 5);
    root->addWidget(setupSendPanel(), 1);
    root->addWidget(setupRawAndLogPanel(), 3);

    setCentralWidget(central);
    statusBar()->showMessage("Ready");
}

QWidget *MainWindow::setupHeader()
{
    QWidget *header = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(4, 0, 4, 0);

    QLabel *title = new QLabel("SerialParser", header);
    title->setObjectName("TitleLabel");
    QLabel *subtitle = new QLabel("STM32 Serial Protocol Parser", header);
    subtitle->setObjectName("SubtitleLabel");
    QLabel *appIcon = new QLabel(header);
    appIcon->setObjectName("HeaderAppIcon");
    appIcon->setFixedSize(66, 66);
    appIcon->setAlignment(Qt::AlignCenter);
    QPixmap iconPixmap(":/icons/app_icon.png");
    if (iconPixmap.isNull()) {
        iconPixmap.load(QDir(QCoreApplication::applicationDirPath()).filePath("resources/app_icon.png"));
    }
    if (iconPixmap.isNull()) {
        iconPixmap.load(QDir(QDir::currentPath()).filePath("resources/app_icon.png"));
    }
    if (!iconPixmap.isNull()) {
        QPixmap badge(62, 62);
        badge.fill(Qt::transparent);
        QPainter painter(&badge);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor("#26485a"), 1));
        painter.setBrush(QColor("#101c25"));
        painter.drawRoundedRect(QRectF(1, 1, 60, 60), 12, 12);
        painter.drawPixmap(7, 7, iconPixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        appIcon->setPixmap(badge);
    }

    QVBoxLayout *titleLayout = new QVBoxLayout;
    titleLayout->setSpacing(0);
    titleLayout->addWidget(title);
    titleLayout->addWidget(subtitle);

    m_profileLabel = new QLabel("配置：--", header);
    m_headerRateLabel = new QLabel("有效帧率：0 Hz", header);
    m_onlineBadge = new QLabel("离线", header);
    m_onlineBadge->setObjectName("OnlineBadge");
    m_onlineBadge->setProperty("online", false);
    QPushButton *guideButton = new QPushButton("使用说明", header);

    layout->addWidget(appIcon);
    layout->addLayout(titleLayout);
    layout->addStretch();
    layout->addWidget(guideButton);
    layout->addWidget(m_profileLabel);
    layout->addWidget(m_headerRateLabel);
    layout->addWidget(m_onlineBadge);
    connect(guideButton, &QPushButton::clicked, this, &MainWindow::showUserGuide);
    return header;
}

QGroupBox *MainWindow::setupSerialPanel()
{
    QGroupBox *group = new QGroupBox("串口设置", this);
    QGridLayout *layout = new QGridLayout(group);

    m_portCombo = new QComboBox(group);
    QPushButton *refreshButton = new QPushButton("刷新", group);
    m_baudCombo = new QComboBox(group);
    m_baudCombo->setEditable(true);
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");

    m_dataBitsCombo = new QComboBox(group);
    m_dataBitsCombo->addItems({"5", "6", "7", "8"});
    m_dataBitsCombo->setCurrentText("8");

    m_stopBitsCombo = new QComboBox(group);
    m_stopBitsCombo->addItems({"1", "1.5", "2"});

    m_parityCombo = new QComboBox(group);
    m_parityCombo->addItems({"None", "Even", "Odd", "Mark", "Space"});

    m_openCloseButton = new QPushButton("打开串口", group);
    m_openCloseButton->setObjectName("PrimaryButton");
    m_portStateLabel = new QLabel("状态：未打开", group);

    layout->addWidget(new QLabel("串口号"), 0, 0);
    layout->addWidget(m_portCombo, 0, 1);
    layout->addWidget(refreshButton, 0, 2);
    layout->addWidget(new QLabel("波特率"), 1, 0);
    layout->addWidget(m_baudCombo, 1, 1, 1, 2);
    layout->addWidget(new QLabel("数据位"), 2, 0);
    layout->addWidget(m_dataBitsCombo, 2, 1, 1, 2);
    layout->addWidget(new QLabel("停止位"), 3, 0);
    layout->addWidget(m_stopBitsCombo, 3, 1, 1, 2);
    layout->addWidget(new QLabel("校验位"), 4, 0);
    layout->addWidget(m_parityCombo, 4, 1, 1, 2);
    layout->addWidget(m_openCloseButton, 5, 0, 1, 3);
    layout->addWidget(m_portStateLabel, 6, 0, 1, 3);

    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    return group;
}

QGroupBox *MainWindow::setupConfigPanel()
{
    QGroupBox *group = new QGroupBox("配置管理", this);
    QVBoxLayout *layout = new QVBoxLayout(group);

    m_profileCombo = new QComboBox(group);
    QPushButton *loadButton = new QPushButton("加载配置", group);
    QPushButton *saveButton = new QPushButton("保存配置", group);
    QPushButton *saveAsButton = new QPushButton("另存为", group);
    QPushButton *openFolderButton = new QPushButton("打开 configs 文件夹", group);

    layout->addWidget(new QLabel("当前配置"));
    layout->addWidget(m_profileCombo);
    layout->addWidget(loadButton);
    layout->addWidget(saveButton);
    layout->addWidget(saveAsButton);
    layout->addWidget(openFolderButton);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadSelectedConfig);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentConfig);
    connect(saveAsButton, &QPushButton::clicked, this, &MainWindow::saveConfigAs);
    connect(openFolderButton, &QPushButton::clicked, this, &MainWindow::openConfigFolder);
    return group;
}

QWidget *MainWindow::setupValuePanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QTabWidget *tabs = new QTabWidget(panel);
    QGroupBox *tableGroup = new QGroupBox("实时字段值", panel);
    QVBoxLayout *tableLayout = new QVBoxLayout(tableGroup);
    m_valueTable = new QTableWidget(tableGroup);
    m_valueTable->setColumnCount(6);
    m_valueTable->setHorizontalHeaderLabels({"字段名", "类型", "Offset", "当前值", "单位", "状态"});
    m_valueTable->horizontalHeader()->setStretchLastSection(true);
    m_valueTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_valueTable->verticalHeader()->setVisible(false);
    m_valueTable->setAlternatingRowColors(true);
    m_valueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_valueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_valueTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_valueTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_valueTable->verticalScrollBar()->setSingleStep(18);
    m_valueTable->horizontalScrollBar()->setSingleStep(24);
    tableLayout->addWidget(m_valueTable);

    m_curvePanel = new CurvePanel(tabs);
    tabs->addTab(tableGroup, "实时字段值");
    tabs->addTab(m_curvePanel, "实时曲线");

    layout->addWidget(tabs, 4);
    layout->addWidget(setupStatsPanel(), 1);
    return panel;
}

QWidget *MainWindow::setupStatsPanel()
{
    QGroupBox *group = new QGroupBox("统计信息", this);
    QGridLayout *layout = new QGridLayout(group);

    auto addStat = [&](int row, int col, const QString &name, QLabel **label) {
        layout->addWidget(new QLabel(name), row, col * 2);
        *label = new QLabel("0", group);
        layout->addWidget(*label, row, col * 2 + 1);
    };

    addStat(0, 0, "总接收字节", &m_totalBytesLabel);
    addStat(0, 1, "候选帧", &m_candidateFramesLabel);
    addStat(0, 2, "有效帧", &m_validFramesLabel);
    addStat(1, 0, "有效帧率", &m_frameRateLabel);
    addStat(1, 1, "包头错误", &m_headerErrorsLabel);
    addStat(1, 2, "包尾错误", &m_tailErrorsLabel);
    addStat(2, 0, "CRC 错误", &m_crcErrorsLabel);
    addStat(2, 1, "长度错误", &m_lengthErrorsLabel);
    addStat(2, 2, "字段异常", &m_fieldErrorsLabel);
    addStat(3, 0, "丢弃字节", &m_discardedBytesLabel);
    addStat(3, 1, "RxBuffer 长度", &m_rxBufferLengthLabel);

    return group;
}

QWidget *MainWindow::setupProtocolTabs()
{
    QTabWidget *tabs = new QTabWidget(this);

    QWidget *basicTab = new QWidget(tabs);
    QFormLayout *basicLayout = new QFormLayout(basicTab);
    m_profileNameEdit = new QLineEdit(basicTab);
    m_frameLengthEdit = new QLineEdit(basicTab);
    m_headerEdit = new QLineEdit(basicTab);
    m_tailEdit = new QLineEdit(basicTab);
    m_endianCombo = new QComboBox(basicTab);
    m_endianCombo->addItem("小端 little", "little");
    m_endianCombo->addItem("大端 big", "big");
    m_frameModeCombo = new QComboBox(basicTab);
    m_frameModeCombo->addItem("搜索包头重同步模式 search_header", "search_header");
    m_frameModeCombo->addItem("严格定长缓存模式 strict_fixed", "strict_fixed");
    QPushButton *applyBasicButton = new QPushButton("应用配置", basicTab);
    applyBasicButton->setObjectName("PrimaryButton");
    basicLayout->addRow("配置名称 profileName", m_profileNameEdit);
    basicLayout->addRow("帧长度 frameLength", m_frameLengthEdit);
    basicLayout->addRow("包头 header", m_headerEdit);
    basicLayout->addRow("包尾 tail", m_tailEdit);
    basicLayout->addRow("字节序 endian", m_endianCombo);
    basicLayout->addRow("帧模式 frameMode", m_frameModeCombo);
    basicLayout->addRow(applyBasicButton);
    tabs->addTab(basicTab, "基础配置");

    QWidget *crcTab = new QWidget(tabs);
    QFormLayout *crcLayout = new QFormLayout(crcTab);
    m_crcEnabledCheck = new QCheckBox("启用 CRC / 校验", crcTab);
    m_crcTypeCombo = new QComboBox(crcTab);
    m_crcTypeCombo->addItems({"none", "sum8", "xor8", "crc8", "crc16_modbus"});
    m_crcOffsetEdit = new QLineEdit(crcTab);
    m_crcLengthEdit = new QLineEdit(crcTab);
    m_crcRangeStartEdit = new QLineEdit(crcTab);
    m_crcRangeLengthEdit = new QLineEdit(crcTab);
    QPushButton *applyCrcButton = new QPushButton("应用配置", crcTab);
    applyCrcButton->setObjectName("PrimaryButton");
    crcLayout->addRow(m_crcEnabledCheck);
    crcLayout->addRow("校验类型 type", m_crcTypeCombo);
    crcLayout->addRow("校验偏移 offset", m_crcOffsetEdit);
    crcLayout->addRow("校验长度 length", m_crcLengthEdit);
    crcLayout->addRow("计算起点 rangeStart", m_crcRangeStartEdit);
    crcLayout->addRow("计算长度 rangeLength", m_crcRangeLengthEdit);
    crcLayout->addRow(applyCrcButton);
    tabs->addTab(crcTab, "CRC 配置");

    QWidget *fieldsTab = new QWidget(tabs);
    QVBoxLayout *fieldsLayout = new QVBoxLayout(fieldsTab);
    m_fieldConfigTable = new QTableWidget(fieldsTab);
    m_fieldConfigTable->setColumnCount(14);
    m_fieldConfigTable->setHorizontalHeaderLabels({
        "字段名 name", "类型 type", "偏移 offset", "长度 length", "缩放 scale", "偏置 bias",
        "单位 unit", "小数位 decimals", "最小值 min", "最大值 max", "显示 display",
        "枚举映射 enumMap", "可见 visible", "曲线 plot"
    });
    m_fieldConfigTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldConfigTable->horizontalHeader()->setSectionResizeMode(FieldTypeColumn, QHeaderView::Interactive);
    m_fieldConfigTable->horizontalHeader()->setSectionResizeMode(FieldEnumMapColumn, QHeaderView::Interactive);
    m_fieldConfigTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldConfigTable->horizontalHeader()->setMinimumSectionSize(76);
    m_fieldConfigTable->setColumnWidth(FieldTypeColumn, 132);
    m_fieldConfigTable->setColumnWidth(FieldEnumMapColumn, 240);
    m_fieldConfigTable->verticalHeader()->setVisible(false);
    m_fieldConfigTable->setAlternatingRowColors(true);
    m_fieldConfigTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fieldConfigTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fieldConfigTable->verticalScrollBar()->setSingleStep(18);
    m_fieldConfigTable->horizontalScrollBar()->setSingleStep(24);
    fieldsLayout->addWidget(m_fieldConfigTable);

    QHBoxLayout *fieldButtons = new QHBoxLayout;
    QPushButton *autoOffsetButton = new QPushButton("自动计算布局", fieldsTab);
    autoOffsetButton->setToolTip("按字段顺序自动计算 length、offset，并更新基础配置里的 frameLength");
    QPushButton *helpButton = new QPushButton("字段配置说明", fieldsTab);
    QPushButton *addFieldButton = new QPushButton("新增字段", fieldsTab);
    QPushButton *removeFieldButton = new QPushButton("删除选中字段", fieldsTab);
    QPushButton *applyFieldsButton = new QPushButton("应用配置", fieldsTab);
    applyFieldsButton->setObjectName("PrimaryButton");
    fieldButtons->addWidget(autoOffsetButton);
    fieldButtons->addWidget(helpButton);
    fieldButtons->addWidget(addFieldButton);
    fieldButtons->addWidget(removeFieldButton);
    fieldButtons->addStretch();
    fieldButtons->addWidget(applyFieldsButton);
    fieldsLayout->addLayout(fieldButtons);
    tabs->addTab(fieldsTab, "字段配置");

    QWidget *rawTab = new QWidget(tabs);
    QFormLayout *rawLayout = new QFormLayout(rawTab);
    m_rawModeCombo = new QComboBox(rawTab);
    m_rawModeCombo->addItem("HEX 显示", "HEX");
    m_rawModeCombo->addItem("文本显示", "TEXT");
    m_rawModeCombo->addItem("HEX + 文本双显示", "BOTH");
    m_rawEncodingCombo = new QComboBox(rawTab);
    m_rawEncodingCombo->addItems({"UTF-8", "GBK / GB18030", "Local8Bit / 本地编码", "Latin1"});
    m_rawPauseCheck = new QCheckBox("暂停显示（后台继续接收和解析）", rawTab);
    m_rawAutoScrollCheck = new QCheckBox("自动滚动到底部", rawTab);
    m_rawTimestampCheck = new QCheckBox("显示时间戳 [HH:mm:ss.zzz]", rawTab);
    m_rawMaxLinesSpin = new QSpinBox(rawTab);
    m_rawMaxLinesSpin->setRange(20, 5000);
    m_rawMaxLinesSpin->setValue(200);
    QPushButton *applyRawButton = new QPushButton("应用配置", rawTab);
    applyRawButton->setObjectName("PrimaryButton");
    rawLayout->addRow("显示模式", m_rawModeCombo);
    rawLayout->addRow("文本编码", m_rawEncodingCombo);
    rawLayout->addRow(m_rawPauseCheck);
    rawLayout->addRow(m_rawAutoScrollCheck);
    rawLayout->addRow(m_rawTimestampCheck);
    rawLayout->addRow("最大显示行数", m_rawMaxLinesSpin);
    rawLayout->addRow(applyRawButton);
    tabs->addTab(rawTab, "原始数据设置");

    connect(applyBasicButton, &QPushButton::clicked, this, &MainWindow::applyUiConfig);
    connect(applyCrcButton, &QPushButton::clicked, this, &MainWindow::applyUiConfig);
    connect(applyFieldsButton, &QPushButton::clicked, this, &MainWindow::applyUiConfig);
    connect(applyRawButton, &QPushButton::clicked, this, &MainWindow::applyUiConfig);
    connect(autoOffsetButton, &QPushButton::clicked, this, &MainWindow::autoCalculateFieldLayout);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::showFieldConfigHelp);
    connect(addFieldButton, &QPushButton::clicked, this, [this]() {
        const int row = m_fieldConfigTable->rowCount();
        m_fieldConfigTable->insertRow(row);
        setTableText(m_fieldConfigTable, row, FieldNameColumn, "NewField");
        setFieldTypeAtRow(row, "uint8");
        setTableText(m_fieldConfigTable, row, FieldOffsetColumn, "0");
        setTableText(m_fieldConfigTable, row, FieldLengthColumn, "1");
        setTableText(m_fieldConfigTable, row, FieldScaleColumn, "1");
        setTableText(m_fieldConfigTable, row, FieldBiasColumn, "0");
        setTableText(m_fieldConfigTable, row, FieldDecimalsColumn, "0");
        setTableText(m_fieldConfigTable, row, FieldDisplayColumn, "number");
        setTableText(m_fieldConfigTable, row, FieldVisibleColumn, "true");
        setTableText(m_fieldConfigTable, row, FieldPlotColumn, "false");
    });
    connect(removeFieldButton, &QPushButton::clicked, this, [this]() {
        const QList<QTableWidgetSelectionRange> ranges = m_fieldConfigTable->selectedRanges();
        if (ranges.isEmpty()) {
            return;
        }
        for (int row = ranges.first().bottomRow(); row >= ranges.first().topRow(); --row) {
            m_fieldConfigTable->removeRow(row);
        }
    });
    connect(m_rawMaxLinesSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        m_rawDataEdit->setMaximumBlockCount(value);
    });
    connect(m_rawEncodingCombo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        m_sendEncodingCombo->setCurrentText(text);
    });

    return tabs;
}

QGroupBox *MainWindow::setupSendPanel()
{
    QGroupBox *group = new QGroupBox("发送区", this);
    QHBoxLayout *layout = new QHBoxLayout(group);

    m_sendModeCombo = new QComboBox(group);
    m_sendModeCombo->addItem("文本发送", "TEXT");
    m_sendModeCombo->addItem("HEX 发送", "HEX");
    m_sendEncodingCombo = new QComboBox(group);
    m_sendEncodingCombo->addItems({"UTF-8", "GBK / GB18030", "Local8Bit / 本地编码", "Latin1"});
    m_sendNewlineCombo = new QComboBox(group);
    m_sendNewlineCombo->addItem("无", "");
    m_sendNewlineCombo->addItem("\\r\\n", "\r\n");
    m_sendNewlineCombo->addItem("\\n", "\n");
    m_sendNewlineCombo->addItem("\\r", "\r");
    m_sendEdit = new QLineEdit(group);
    m_sendEdit->setPlaceholderText("文本发送：输入要发送的文本；HEX 发送：A5 01 00 5A 或 0xA5 0x01 0x00 0x5A");
    m_sendEdit->setMinimumHeight(34);
    QPushButton *clearSendButton = new QPushButton("清空发送区", group);
    QPushButton *sendButton = new QPushButton("发送", group);
    sendButton->setObjectName("PrimaryButton");

    layout->addWidget(new QLabel("模式"));
    layout->addWidget(m_sendModeCombo);
    layout->addWidget(new QLabel("编码"));
    layout->addWidget(m_sendEncodingCombo);
    layout->addWidget(new QLabel("追加换行"));
    layout->addWidget(m_sendNewlineCombo);
    layout->addWidget(m_sendEdit, 1);
    layout->addWidget(clearSendButton);
    layout->addWidget(sendButton);

    connect(sendButton, &QPushButton::clicked, this, &MainWindow::sendData);
    connect(clearSendButton, &QPushButton::clicked, m_sendEdit, &QLineEdit::clear);
    connect(m_sendEdit, &QLineEdit::returnPressed, this, &MainWindow::sendData);
    connect(m_sendModeCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const bool textMode = m_sendModeCombo->currentData().toString() == "TEXT";
        m_sendEncodingCombo->setEnabled(textMode);
        m_sendNewlineCombo->setEnabled(textMode);
    });
    return group;
}

QWidget *MainWindow::setupRawAndLogPanel()
{
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    QGroupBox *rawGroup = new QGroupBox("原始数据", splitter);
    QVBoxLayout *rawLayout = new QVBoxLayout(rawGroup);
    QHBoxLayout *rawButtons = new QHBoxLayout;
    QPushButton *clearRawButton = new QPushButton("清空显示", rawGroup);
    QPushButton *copyRawButton = new QPushButton("复制显示内容", rawGroup);
    rawButtons->addStretch();
    rawButtons->addWidget(clearRawButton);
    rawButtons->addWidget(copyRawButton);
    m_rawDataEdit = new QPlainTextEdit(rawGroup);
    m_rawDataEdit->setReadOnly(true);
    m_rawDataEdit->setMaximumBlockCount(200);
    rawLayout->addLayout(rawButtons);
    rawLayout->addWidget(m_rawDataEdit);

    QGroupBox *logGroup = new QGroupBox("日志", splitter);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QTextEdit(logGroup);
    m_logEdit->setReadOnly(true);
    logLayout->addWidget(m_logEdit);

    splitter->addWidget(rawGroup);
    splitter->addWidget(logGroup);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    connect(clearRawButton, &QPushButton::clicked, m_rawDataEdit, &QPlainTextEdit::clear);
    connect(copyRawButton, &QPushButton::clicked, m_rawDataEdit, &QPlainTextEdit::selectAll);
    connect(copyRawButton, &QPushButton::clicked, m_rawDataEdit, &QPlainTextEdit::copy);
    return splitter;
}

void MainWindow::setupConnections()
{
    connect(&m_serialService, &SerialService::dataReceived, this, &MainWindow::handleSerialData);
    connect(&m_serialService, &SerialService::errorOccurred, this, [this](const QString &message) {
        appendLog(message, true);
    });
    connect(&m_serialService, &SerialService::portStateChanged, this, &MainWindow::handleSerialStateChanged);
    connect(&m_parser, &ProtocolParser::frameParsed, this, &MainWindow::handleFrameParsed);
    connect(&m_parser, &ProtocolParser::statisticsChanged, this, &MainWindow::handleParserStats);
    connect(&m_parser, &ProtocolParser::errorOccurred, this, [this](const QString &message) {
        appendLog(message, true);
    });
    connect(m_curvePanel, &CurvePanel::plotFieldChanged, this, &MainWindow::syncPlotFieldFromCurve);
    connect(m_curvePanel, &CurvePanel::detachRequested, this, &MainWindow::openDetachedCurveWindow);
    connect(m_openCloseButton, &QPushButton::clicked, this, &MainWindow::toggleSerialPort);
}

void MainWindow::loadStyleSheet()
{
    const QString stylePath = QDir(QCoreApplication::applicationDirPath()).filePath("styles/dark.qss");
    QFile file(stylePath);
    if (!file.exists()) {
        const QString sourcePath = QDir(QDir::currentPath()).filePath("styles/dark.qss");
        QFile sourceFile(sourcePath);
        if (sourceFile.open(QIODevice::ReadOnly)) {
            QString qss = QString::fromUtf8(sourceFile.readAll());
            qss.replace("__STYLE_DIR__", QFileInfo(sourcePath).absolutePath());
            qApp->setStyleSheet(qss);
        }
        return;
    }
    if (file.open(QIODevice::ReadOnly)) {
        QString qss = QString::fromUtf8(file.readAll());
        qss.replace("__STYLE_DIR__", QFileInfo(stylePath).absolutePath());
        qApp->setStyleSheet(qss);
    }
}

void MainWindow::refreshPorts()
{
    const QString current = m_portCombo->currentText();
    m_portCombo->clear();
    m_portCombo->addItems(m_serialService.availablePorts());
    const int index = m_portCombo->findText(current);
    if (index >= 0) {
        m_portCombo->setCurrentIndex(index);
    }
}

void MainWindow::refreshConfigList()
{
    QStringList warnings;
    const QVector<ConfigInfo> configs = m_configManager.scanConfigs(&warnings);
    for (const QString &warning : warnings) {
        appendLog(warning);
    }

    m_profileCombo->clear();
    m_profilePaths.clear();
    for (const ConfigInfo &info : configs) {
        m_profileCombo->addItem(info.profileName);
        m_profilePaths.insert(info.profileName, info.filePath);
    }
}

void MainWindow::loadSelectedConfig()
{
    const QString profile = m_profileCombo->currentText();
    const QString path = m_profilePaths.value(profile);
    if (path.isEmpty()) {
        appendLog("没有可加载的配置文件");
        return;
    }

    ProtocolConfig config;
    QStringList errors;
    if (!m_configManager.loadConfig(path, &config, &errors)) {
        appendLog("加载配置失败：" + errors.join("\n"), true);
        QMessageBox::warning(this, "配置错误", errors.join("\n"));
        return;
    }

    applyConfigToUi(config);
    QStringList parserErrors;
    if (!m_parser.setConfig(config, &parserErrors)) {
        appendLog("应用配置失败：" + parserErrors.join("\n"), true);
        QMessageBox::warning(this, "配置错误", parserErrors.join("\n"));
        return;
    }

    m_currentConfigPath = path;
    appendLog("已加载配置：" + path);
}

void MainWindow::saveCurrentConfig()
{
    ProtocolConfig config;
    QStringList errors;
    if (!readConfigFromUi(&config, &errors)) {
        QMessageBox::warning(this, "配置错误", errors.join("\n"));
        return;
    }

    QString path = m_currentConfigPath;
    if (path.isEmpty()) {
        path = QDir(m_configManager.configDirPath()).filePath(profileFileName(config.profileName));
    }

    QString error;
    if (!m_configManager.saveConfig(path, config, &error)) {
        appendLog("保存配置失败：" + error, true);
        QMessageBox::warning(this, "保存失败", error);
        return;
    }

    m_currentConfigPath = path;
    m_config = config;
    appendLog("已保存配置：" + path);
    refreshConfigList();
    const int index = m_profileCombo->findText(config.profileName);
    if (index >= 0) {
        m_profileCombo->setCurrentIndex(index);
    }
}

void MainWindow::saveConfigAs()
{
    ProtocolConfig config;
    QStringList errors;
    if (!readConfigFromUi(&config, &errors)) {
        QMessageBox::warning(this, "配置错误", errors.join("\n"));
        return;
    }

    const QString defaultPath = QDir(m_configManager.configDirPath()).filePath(profileFileName(config.profileName));
    const QString path = QFileDialog::getSaveFileName(this, "另存为配置", defaultPath, "JSON (*.json)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_configManager.saveConfig(path, config, &error)) {
        appendLog("另存为失败：" + error, true);
        QMessageBox::warning(this, "保存失败", error);
        return;
    }
    m_currentConfigPath = path;
    m_config = config;
    appendLog("已另存为配置：" + path);
    refreshConfigList();
}

void MainWindow::openConfigFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_configManager.configDirPath()));
}

void MainWindow::applyUiConfig()
{
    ProtocolConfig config;
    QStringList errors;
    if (!readConfigFromUi(&config, &errors)) {
        appendLog("配置检查失败：" + errors.join("; "), true);
        QMessageBox::warning(this, "配置错误", errors.join("\n"));
        return;
    }

    QStringList parserErrors;
    if (!m_parser.setConfig(config, &parserErrors)) {
        appendLog("应用配置失败：" + parserErrors.join("; "), true);
        QMessageBox::warning(this, "配置错误", parserErrors.join("\n"));
        return;
    }

    m_config = config;
    m_profileLabel->setText("配置：" + config.profileName);
    m_rawDataEdit->setMaximumBlockCount(config.rawDisplay.maxLines);
    populateValueTable(config.fields);
    m_curvePanel->setConfig(config);
    for (CurvePanel *panel : m_detachedCurvePanels) {
        if (panel) {
            panel->setConfig(config);
        }
    }
    appendLog("配置已应用到解析器");
}

void MainWindow::autoCalculateFieldLayout()
{
    QStringList errors;

    QByteArray header;
    QString hexError;
    if (!HexUtil::parseHexString(m_headerEdit->text(), &header, &hexError)) {
        QMessageBox::warning(this, "无法自动计算布局", "包头 header 非法：" + hexError);
        return;
    }

    QByteArray tail;
    if (!HexUtil::parseHexString(m_tailEdit->text(), &tail, &hexError)) {
        QMessageBox::warning(this, "无法自动计算布局", "包尾 tail 非法：" + hexError);
        return;
    }

    int offset = header.size();
    for (int row = 0; row < m_fieldConfigTable->rowCount(); ++row) {
        const QString name = tableText(m_fieldConfigTable, row, FieldNameColumn).trimmed();
        const QString type = fieldTypeAtRow(row).trimmed();
        if (name.isEmpty() && type.isEmpty()) {
            continue;
        }

        int length = 0;
        const int defaultLength = ProtocolConfig::typeDefaultLength(type);
        if (defaultLength < 0) {
            errors << QString("第 %1 行字段类型非法：%2").arg(row + 1).arg(type);
            continue;
        }

        if (type == "raw_hex") {
            bool ok = false;
            length = tableText(m_fieldConfigTable, row, FieldLengthColumn).trimmed().toInt(&ok);
            if (!ok || length <= 0) {
                errors << QString("第 %1 行 raw_hex 必须先填写合法 length").arg(row + 1);
                continue;
            }
        } else {
            length = defaultLength;
            setTableText(m_fieldConfigTable, row, FieldLengthColumn, QString::number(length));
        }

        setTableText(m_fieldConfigTable, row, FieldOffsetColumn, QString::number(offset));
        offset += length;
    }

    if (!errors.isEmpty()) {
        appendLog("自动计算布局发现问题：" + errors.join("; "), true);
        QMessageBox::warning(this, "自动计算布局", errors.join("\n"));
        return;
    }

    const int computedFrameLength = offset + tail.size();
    m_frameLengthEdit->setText(QString::number(computedFrameLength));
    appendLog(QString("已自动计算字段布局：起始 offset=%1，字段结束 offset=%2，frameLength=%3")
                  .arg(header.size())
                  .arg(offset)
                  .arg(computedFrameLength));
    if (m_crcEnabledCheck->isChecked()) {
        appendLog("提示：如果 CRC 字节没有作为字段写入字段表，请手动确认 frameLength 和 CRC offset。");
    }
}

void MainWindow::showFieldConfigHelp()
{
    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowTitle("字段配置说明");
    dialog->resize(860, 640);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QLabel *pageLabel = new QLabel(dialog);
    pageLabel->setStyleSheet("font-weight: 700; color: #72f7d0; padding: 4px;");
    QTabWidget *tabs = new QTabWidget(dialog);
    tabs->setTabPosition(QTabWidget::North);

    auto addPage = [&](const QString &title, const QString &html) {
        QTextBrowser *browser = new QTextBrowser(tabs);
        browser->setOpenExternalLinks(false);
        browser->setHtml(html);
        tabs->addTab(browser, title);
    };

    addPage("总览", R"(
        <h2>字段配置总览</h2>
        <p>如果 STM32 使用 <b>连续 packed 结构体</b> 发送数据，通常只需要按顺序填写字段名和类型，必要时填写枚举映射。</p>
        <ol>
            <li>先在基础配置里填写包头 header 和包尾 tail。</li>
            <li>到字段配置页，按 STM32 结构体顺序填写字段。</li>
            <li>主要填写 <b>字段名 name</b> 和 <b>类型 type</b>。</li>
            <li>如果字段是枚举，填写 enumMap，例如 <code>0=自动模式;1=遥控模式</code>。</li>
            <li>点击 <b>自动计算布局</b>，软件会自动计算 offset、length 和 frameLength。</li>
            <li>点击 <b>应用配置</b> 后，解析器才会使用新配置。</li>
        </ol>
        <p><b>建议：</b>常规协议不要手动填 offset，除非你的协议中有保留字节、CRC 字段、跳过字段或非连续布局。</p>
    )");

    addPage("字段名 name", R"(
        <h2>字段名 name</h2>
        <p>字段名用于显示在实时字段值表格中，也会保存到 JSON 配置。</p>
        <p>建议使用简短、稳定、有意义的名称。</p>
        <h3>示例</h3>
        <pre>K1
Vx
BatteryVoltage
Mode</pre>
    )");

    addPage("类型 type", R"(
        <h2>类型 type</h2>
        <p>类型决定软件从帧中取几个字节，以及如何解释这些字节。</p>
        <p>类型列是可编辑下拉框，可以输入部分字符进行提示，例如输入 <code>float</code> 会看到 <code>float32</code> 和 <code>float64</code>。</p>
        <table border="1" cellspacing="0" cellpadding="6">
            <tr><th>类型</th><th>长度</th><th>说明</th></tr>
            <tr><td>uint8 / int8</td><td>1</td><td>8 位整数</td></tr>
            <tr><td>uint16 / int16</td><td>2</td><td>16 位整数</td></tr>
            <tr><td>uint32 / int32</td><td>4</td><td>32 位整数</td></tr>
            <tr><td>float32</td><td>4</td><td>STM32 C 语言 float</td></tr>
            <tr><td>float64</td><td>8</td><td>double</td></tr>
            <tr><td>bool_uint8</td><td>1</td><td>uint8_t 按 bool 显示</td></tr>
            <tr><td>raw_hex</td><td>用户填写</td><td>按 HEX 原样显示</td></tr>
        </table>
    )");

    addPage("偏移 offset", R"(
        <h2>偏移 offset</h2>
        <p>offset 表示字段在整帧数据中的起始字节位置，从 0 开始。</p>
        <p>包头通常是 offset 0。第一个业务字段通常从包头后面开始。</p>
        <h3>默认协议示例</h3>
        <pre>offset 0  : A5 包头
offset 1  : K1
offset 2  : K2
offset 6  : Vx 的第 1 个字节
offset 18 : Mode
offset 19 : 5A 包尾</pre>
        <p>普通连续结构体协议建议点击 <b>自动计算布局</b>，让软件自动填写 offset。</p>
    )");

    addPage("长度 length", R"(
        <h2>长度 length</h2>
        <p>length 表示字段占用多少字节。</p>
        <p>普通类型会根据 type 自动计算，不需要手动填。只有 <code>raw_hex</code> 必须手动填写。</p>
        <h3>规则</h3>
        <pre>uint8 / int8 / bool_uint8 = 1
uint16 / int16            = 2
uint32 / int32 / float32  = 4
float64                   = 8
raw_hex                   = 手动填写</pre>
    )");

    addPage("缩放 bias", R"(
        <h2>scale 和 bias</h2>
        <p>scale 和 bias 用于把原始值转换成显示值。</p>
        <pre>显示值 = 原始值 * scale + bias</pre>
        <h3>示例</h3>
        <p>STM32 发送电压原始值 1234，单位是 0.001V，则配置：</p>
        <pre>scale = 0.001
bias  = 0
显示值 = 1.234 V</pre>
    )");

    addPage("单位/小数", R"(
        <h2>unit 和 decimals</h2>
        <p><b>unit</b> 是字段单位，显示在实时字段值表中。</p>
        <p><b>decimals</b> 是小数位数，只影响 number 显示。</p>
        <h3>示例</h3>
        <pre>Vx: unit = m/s, decimals = 3
Temp: unit = degC, decimals = 1
Voltage: unit = V, decimals = 2</pre>
    )");

    addPage("范围 min/max", R"(
        <h2>min 和 max</h2>
        <p>min/max 是可选正常范围。字段解析后的显示值超出范围时，状态会显示异常。</p>
        <h3>示例</h3>
        <pre>BatteryVoltage:
min = 10.5
max = 12.6</pre>
        <p>如果不需要范围检查，留空即可。</p>
    )");

    addPage("显示/枚举", R"(
        <h2>display 和 enumMap</h2>
        <p>display 控制字段显示方式。</p>
        <ul>
            <li><code>number</code>：普通数字</li>
            <li><code>bool</code>：0 显示 false，非 0 显示 true</li>
            <li><code>enum</code>：按 enumMap 显示中文含义</li>
            <li><code>hex</code>：按原始 HEX 显示</li>
        </ul>
        <h3>enumMap 示例</h3>
        <pre>0=自动模式;1=遥控模式;2=调试模式</pre>
        <p>保存 JSON 时会自动转换成对象。</p>
    )");

    addPage("可见/曲线", R"(
        <h2>visible 和 plot</h2>
        <p><b>visible</b> 控制字段是否显示在实时字段值表中。</p>
        <p><b>plot</b> 控制字段是否加入实时曲线。只有数值字段会绘制，<code>raw_hex</code> 不绘制。</p>
        <h3>填写方式</h3>
        <pre>true
false</pre>
    )");

    addPage("完整案例", R"(
        <h2>完整配置案例：STM32 遥控器帧</h2>
        <p>STM32 结构体：</p>
        <pre>#pragma pack(push, 1)
typedef struct
{
    uint8_t Frame_Header; // 0xA5
    uint8_t K1;
    uint8_t K2;
    uint8_t K3;
    uint8_t LB;
    uint8_t RB;
    float Vx;
    float Vy;
    float Wz;
    uint8_t Mode;
    uint8_t Frame_Tail;   // 0x5A
} Remote_TX_Frame_t;
#pragma pack(pop)</pre>
        <p>基础配置：</p>
        <pre>header = A5
tail = 5A
endian = little
frameMode = search_header</pre>
        <p>字段配置只需要按顺序填写：</p>
        <pre>K1   bool_uint8
K2   bool_uint8
K3   bool_uint8
LB   bool_uint8
RB   bool_uint8
Vx   float32
Vy   float32
Wz   float32
Mode uint8, display=enum, enumMap=0=自动模式;1=遥控模式;2=调试模式</pre>
        <p>点击 <b>自动计算布局</b> 后会得到：</p>
        <pre>K1 offset 1
K2 offset 2
K3 offset 3
LB offset 4
RB offset 5
Vx offset 6
Vy offset 10
Wz offset 14
Mode offset 18
frameLength = 20</pre>
    )");

    auto updatePageLabel = [pageLabel, tabs]() {
        pageLabel->setText(QString("当前页：%1 / %2 - %3")
                               .arg(tabs->currentIndex() + 1)
                               .arg(tabs->count())
                               .arg(tabs->tabText(tabs->currentIndex())));
    };
    connect(tabs, &QTabWidget::currentChanged, dialog, [=](int) {
        updatePageLabel();
    });
    updatePageLabel();

    QPushButton *closeButton = new QPushButton("关闭", dialog);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);

    layout->addWidget(pageLabel);
    layout->addWidget(tabs, 1);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::showUserGuide()
{
    QString readmePath = QDir(QCoreApplication::applicationDirPath()).filePath("README.md");
    if (!QFile::exists(readmePath)) {
        readmePath = QDir(QDir::currentPath()).filePath("README.md");
    }
    if (!QFile::exists(readmePath)) {
        readmePath = QDir(QCoreApplication::applicationDirPath()).filePath("../README.md");
    }

    QString markdown;
    QFile file(readmePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        markdown = QString::fromUtf8(file.readAll());
    }
    if (markdown.trimmed().isEmpty()) {
        markdown = "# SerialParser 使用说明\n\nREADME.md 未找到，请确认发布目录中包含 README.md。";
    }

    struct GuidePage {
        QString title;
        QString markdown;
    };

    QVector<GuidePage> pages;
    const QRegularExpression sectionRe("(?m)^##\\s+(.+)$");
    QRegularExpressionMatchIterator it = sectionRe.globalMatch(markdown);
    QVector<QRegularExpressionMatch> matches;
    while (it.hasNext()) {
        matches.append(it.next());
    }

    if (matches.isEmpty()) {
        pages.append({"使用说明", markdown});
    } else {
        const int firstStart = matches.first().capturedStart();
        const QString intro = markdown.left(firstStart).trimmed();
        if (!intro.isEmpty()) {
            pages.append({"简介", intro});
        }
        for (int i = 0; i < matches.size(); ++i) {
            const int start = matches.at(i).capturedStart();
            const int end = (i + 1 < matches.size()) ? matches.at(i + 1).capturedStart() : markdown.size();
            QString title = matches.at(i).captured(1).trimmed();
            title.remove(QRegularExpression("^([一二三四五六七八九十]+|\\d+)[、.．]\\s*"));
            pages.append({title, markdown.mid(start, end - start).trimmed()});
        }
    }

    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowTitle("使用说明");
    dialog->resize(920, 700);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QLabel *pageLabel = new QLabel(dialog);
    pageLabel->setStyleSheet("font-weight: 700; color: #72f7d0; padding: 4px;");

    QTabWidget *tabs = new QTabWidget(dialog);
    tabs->setTabPosition(QTabWidget::North);
    tabs->setUsesScrollButtons(true);

    for (const GuidePage &page : pages) {
        QTextBrowser *browser = new QTextBrowser(tabs);
        browser->setOpenExternalLinks(false);
        // README 使用相对路径引用图片；搜索路径和 baseUrl 都指向 README 所在目录，发布版可离线显示。
        const QString readmeDir = QFileInfo(readmePath).absolutePath();
        browser->setSearchPaths({readmeDir});
        browser->document()->setBaseUrl(QUrl::fromLocalFile(readmeDir + QDir::separator()));
        QString pageMarkdown = page.markdown;
        // GitHub README 继续使用原图；应用内简介页使用较小预览图，避免撑满说明窗口。
        if (pageMarkdown.contains("resources/readme_hero.png")) {
            pageMarkdown.replace("resources/readme_hero.png", "resources/readme_hero_preview.png");
        }
        browser->document()->setMarkdown(pageMarkdown, QTextDocument::MarkdownDialectGitHub);
        tabs->addTab(browser, page.title.left(16));
    }

    auto updatePageLabel = [pageLabel, tabs]() {
        pageLabel->setText(QString("当前页：%1 / %2 - %3")
                               .arg(tabs->currentIndex() + 1)
                               .arg(tabs->count())
                               .arg(tabs->tabText(tabs->currentIndex())));
    };
    connect(tabs, &QTabWidget::currentChanged, dialog, [=](int) {
        updatePageLabel();
    });
    updatePageLabel();

    QPushButton *closeButton = new QPushButton("关闭", dialog);
    connect(closeButton, &QPushButton::clicked, dialog, &QDialog::close);

    layout->addWidget(pageLabel);
    layout->addWidget(tabs, 1);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::toggleSerialPort()
{
    if (m_serialService.isOpen()) {
        m_serialService.closePort();
        return;
    }

    if (m_portCombo->currentText().isEmpty()) {
        QMessageBox::warning(this, "串口错误", "没有可用串口，请检查设备连接后刷新。");
        return;
    }

    bool ok = false;
    const int baudrate = m_baudCombo->currentText().toInt(&ok);
    if (!ok || baudrate <= 0) {
        QMessageBox::warning(this, "串口错误", "波特率必须是正整数。");
        return;
    }

    QString error;
    if (!m_serialService.openPort(m_portCombo->currentText(),
                                  baudrate,
                                  m_dataBitsCombo->currentText().toInt(),
                                  m_stopBitsCombo->currentText(),
                                  m_parityCombo->currentText(),
                                  &error)) {
        appendLog("打开串口失败：" + error, true);
        QMessageBox::warning(this, "打开串口失败", error);
    }
}

void MainWindow::handleSerialData(const QByteArray &data)
{
    appendRawDataLine("RX", data);
    m_parser.appendData(data);
}

void MainWindow::handleFrameParsed(const ParseResult &result)
{
    if (result.valid) {
        updateValueTable(result.fieldValues);
        m_curvePanel->appendFrame(result);
        for (CurvePanel *panel : m_detachedCurvePanels) {
            if (panel) {
                panel->appendFrame(result);
            }
        }
    }
}

void MainWindow::handleParserStats(const ParserStats &stats)
{
    m_lastStats = stats;
    m_totalBytesLabel->setText(QString::number(stats.totalBytes));
    m_candidateFramesLabel->setText(QString::number(stats.candidateFrames));
    m_validFramesLabel->setText(QString::number(stats.validFrames));
    m_frameRateLabel->setText(QString("%1 Hz").arg(stats.frameRateHz, 0, 'f', 1));
    m_headerRateLabel->setText(QString("有效帧率：%1 Hz").arg(stats.frameRateHz, 0, 'f', 1));
    m_headerErrorsLabel->setText(QString::number(stats.headerErrorCount));
    m_tailErrorsLabel->setText(QString::number(stats.tailErrorCount));
    m_crcErrorsLabel->setText(QString::number(stats.crcErrorCount));
    m_lengthErrorsLabel->setText(QString::number(stats.lengthErrorCount));
    m_fieldErrorsLabel->setText(QString::number(stats.fieldErrorCount));
    m_discardedBytesLabel->setText(QString::number(stats.discardedBytes));
    m_rxBufferLengthLabel->setText(QString::number(stats.rxBufferLength));

    const quint64 totalErrors = stats.headerErrorCount + stats.tailErrorCount + stats.crcErrorCount
                              + stats.lengthErrorCount + stats.fieldErrorCount;
    statusBar()->showMessage(QString("有效帧：%1 | 错误：%2 | RxBuffer：%3 字节")
                                 .arg(stats.validFrames)
                                 .arg(totalErrors)
                                 .arg(stats.rxBufferLength));
}

void MainWindow::handleSerialStateChanged(bool opened)
{
    m_openCloseButton->setText(opened ? "关闭串口" : "打开串口");
    m_portStateLabel->setText(opened ? "状态：已打开 " + m_serialService.portName() : "状态：未打开");
    if (!opened) {
        setOnlineBadge(false);
    }
    appendLog(opened ? "串口已打开" : "串口已关闭");
}

void MainWindow::sendData()
{
    QByteArray data;
    const QString mode = m_sendModeCombo->currentData().toString();
    if (mode == "HEX") {
        QString error;
        if (!HexUtil::parseHexString(m_sendEdit->text(), &data, &error)) {
            appendLog("HEX 发送解析失败：" + error, true);
            QMessageBox::warning(this, "HEX 格式错误", error);
            return;
        }
    } else {
        QString text = m_sendEdit->text();
        text += m_sendNewlineCombo->currentData().toString();
        QString warning;
        data = EncodingUtil::encode(text, m_sendEncodingCombo->currentText(), &warning);
        if (!warning.isEmpty()) {
            appendLog(warning);
        }
    }

    QString error;
    if (!m_serialService.sendData(data, &error)) {
        appendLog("发送失败：" + error, true);
        return;
    }

    appendRawDataLine("TX", data);
    appendLog(QString("发送成功：%1 字节").arg(data.size()));
}

void MainWindow::updateOnlineState()
{
    bool online = false;
    if (m_serialService.isOpen() && m_lastStats.lastValidFrameTime.isValid()) {
        online = m_lastStats.lastValidFrameTime.msecsTo(QDateTime::currentDateTime()) <= 2000;
    }
    setOnlineBadge(online);
}

void MainWindow::syncPlotFieldFromCurve(const QString &fieldName, bool enabled)
{
    for (int row = 0; row < m_fieldConfigTable->rowCount(); ++row) {
        if (tableText(m_fieldConfigTable, row, FieldNameColumn) == fieldName) {
            setTableText(m_fieldConfigTable, row, FieldPlotColumn, boolToText(enabled));
            break;
        }
    }
    m_curvePanel->setPlotFieldEnabled(fieldName, enabled);
    for (CurvePanel *panel : m_detachedCurvePanels) {
        if (panel) {
            panel->setPlotFieldEnabled(fieldName, enabled);
        }
    }
}

void MainWindow::openDetachedCurveWindow()
{
    QDialog *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->setWindowTitle("SerialParser 实时曲线");
    dialog->resize(980, 720);

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QHBoxLayout *toolbar = new QHBoxLayout;
    QLabel *title = new QLabel("实时曲线独立窗口", dialog);
    title->setStyleSheet("font-weight: 700; color: #72f7d0;");
    QCheckBox *topMostCheck = new QCheckBox("置顶", dialog);
    toolbar->addWidget(title);
    toolbar->addStretch();
    toolbar->addWidget(topMostCheck);

    CurvePanel *panel = new CurvePanel(dialog);
    panel->setConfig(m_config);
    const QMap<QString, bool> plotStates = m_curvePanel->plotFieldStates();
    for (auto it = plotStates.constBegin(); it != plotStates.constEnd(); ++it) {
        panel->setPlotFieldEnabled(it.key(), it.value());
    }

    layout->addLayout(toolbar);
    layout->addWidget(panel, 1);

    m_detachedCurvePanels.append(panel);
    connect(dialog, &QObject::destroyed, this, [this, panel]() {
        m_detachedCurvePanels.removeAll(panel);
    });
    connect(panel, &CurvePanel::plotFieldChanged, this, &MainWindow::syncPlotFieldFromCurve);
    connect(panel, &CurvePanel::detachRequested, this, &MainWindow::openDetachedCurveWindow);
    connect(topMostCheck, &QCheckBox::toggled, dialog, [dialog](bool checked) {
        Qt::WindowFlags flags = dialog->windowFlags();
        if (checked) {
            flags |= Qt::WindowStaysOnTopHint;
        } else {
            flags &= ~Qt::WindowStaysOnTopHint;
        }
        dialog->setWindowFlags(flags);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void MainWindow::applyConfigToUi(const ProtocolConfig &config)
{
    m_config = config;
    m_profileNameEdit->setText(config.profileName);
    m_frameLengthEdit->setText(QString::number(config.frameLength));
    m_headerEdit->setText(config.headerText);
    m_tailEdit->setText(config.tailText);

    int index = m_endianCombo->findData(config.endian);
    if (index >= 0) {
        m_endianCombo->setCurrentIndex(index);
    }
    index = m_frameModeCombo->findData(config.frameMode);
    if (index >= 0) {
        m_frameModeCombo->setCurrentIndex(index);
    }

    m_crcEnabledCheck->setChecked(config.crc.enabled);
    m_crcTypeCombo->setCurrentText(config.crc.type);
    m_crcOffsetEdit->setText(QString::number(config.crc.offset));
    m_crcLengthEdit->setText(QString::number(config.crc.length));
    m_crcRangeStartEdit->setText(QString::number(config.crc.rangeStart));
    m_crcRangeLengthEdit->setText(QString::number(config.crc.rangeLength));

    applySerialDefaultsToUi(config.serial);
    populateFieldConfigTable(config.fields);
    populateValueTable(config.fields);
    m_curvePanel->setConfig(config);
    for (CurvePanel *panel : m_detachedCurvePanels) {
        if (panel) {
            panel->setConfig(config);
        }
    }

    index = m_rawModeCombo->findData(config.rawDisplay.mode);
    if (index >= 0) {
        m_rawModeCombo->setCurrentIndex(index);
    }
    m_rawEncodingCombo->setCurrentText(config.rawDisplay.encoding);
    m_sendEncodingCombo->setCurrentText(config.rawDisplay.encoding);
    m_rawPauseCheck->setChecked(config.rawDisplay.paused);
    m_rawAutoScrollCheck->setChecked(config.rawDisplay.autoScroll);
    m_rawTimestampCheck->setChecked(config.rawDisplay.showTimestamp);
    m_rawMaxLinesSpin->setValue(config.rawDisplay.maxLines);
    m_rawDataEdit->setMaximumBlockCount(config.rawDisplay.maxLines);

    m_profileLabel->setText("配置：" + config.profileName);
}

bool MainWindow::readConfigFromUi(ProtocolConfig *config, QStringList *errors) const
{
    QStringList localErrors;
    ProtocolConfig parsed;

    auto readInt = [&](QLineEdit *edit, const QString &name, int defaultValue = 0) {
        bool ok = false;
        const int value = edit->text().trimmed().toInt(&ok);
        if (!ok) {
            localErrors << QString("%1 必须是整数").arg(name);
            return defaultValue;
        }
        return value;
    };

    parsed.profileName = m_profileNameEdit->text().trimmed();
    parsed.frameLength = readInt(m_frameLengthEdit, "frameLength");
    parsed.headerText = m_headerEdit->text().trimmed();
    parsed.tailText = m_tailEdit->text().trimmed();
    parsed.endian = m_endianCombo->currentData().toString();
    parsed.frameMode = m_frameModeCombo->currentData().toString();
    parsed.serial = readSerialDefaultsFromUi();
    parsed.rawDisplay = readRawDisplaySettingsFromUi();
    parsed.curve = m_curvePanel->settings();

    parsed.crc.enabled = m_crcEnabledCheck->isChecked();
    parsed.crc.type = m_crcTypeCombo->currentText();
    parsed.crc.offset = readInt(m_crcOffsetEdit, "crc.offset");
    parsed.crc.length = readInt(m_crcLengthEdit, "crc.length");
    parsed.crc.rangeStart = readInt(m_crcRangeStartEdit, "crc.rangeStart");
    parsed.crc.rangeLength = readInt(m_crcRangeLengthEdit, "crc.rangeLength");

    parsed.fields = readFieldConfigTable(&localErrors);

    QStringList validationErrors;
    if (!parsed.validate(&validationErrors)) {
        localErrors << validationErrors;
    }

    if (errors) {
        *errors = localErrors;
    }
    if (!localErrors.isEmpty()) {
        return false;
    }
    *config = parsed;
    return true;
}

void MainWindow::populateFieldConfigTable(const QVector<FieldConfig> &fields)
{
    m_fieldConfigTable->setRowCount(fields.size());
    for (int row = 0; row < fields.size(); ++row) {
        const FieldConfig &field = fields.at(row);
        setTableText(m_fieldConfigTable, row, FieldNameColumn, field.name);
        setFieldTypeAtRow(row, field.type);
        setTableText(m_fieldConfigTable, row, FieldOffsetColumn, QString::number(field.offset));
        setTableText(m_fieldConfigTable, row, FieldLengthColumn, QString::number(field.length));
        setTableText(m_fieldConfigTable, row, FieldScaleColumn, QString::number(field.scale));
        setTableText(m_fieldConfigTable, row, FieldBiasColumn, QString::number(field.bias));
        setTableText(m_fieldConfigTable, row, FieldUnitColumn, field.unit);
        setTableText(m_fieldConfigTable, row, FieldDecimalsColumn, QString::number(field.decimals));
        setTableText(m_fieldConfigTable, row, FieldMinColumn, field.hasMin ? QString::number(field.minValue) : "");
        setTableText(m_fieldConfigTable, row, FieldMaxColumn, field.hasMax ? QString::number(field.maxValue) : "");
        setTableText(m_fieldConfigTable, row, FieldDisplayColumn, field.display);
        setTableText(m_fieldConfigTable, row, FieldEnumMapColumn, ProtocolConfig::enumMapToString(field.enumMap));
        setTableText(m_fieldConfigTable, row, FieldVisibleColumn, boolToText(field.visible));
        setTableText(m_fieldConfigTable, row, FieldPlotColumn, boolToText(field.plot));
    }
}

QVector<FieldConfig> MainWindow::readFieldConfigTable(QStringList *errors) const
{
    QVector<FieldConfig> fields;
    QStringList localErrors;

    for (int row = 0; row < m_fieldConfigTable->rowCount(); ++row) {
        const QString name = tableText(m_fieldConfigTable, row, FieldNameColumn).trimmed();
        const QString type = fieldTypeAtRow(row).trimmed();
        if (name.isEmpty() && type.isEmpty()) {
            continue;
        }

        FieldConfig field;
        field.name = name;
        field.type = type.isEmpty() ? "uint8" : type;

        bool ok = false;
        field.offset = tableText(m_fieldConfigTable, row, FieldOffsetColumn).toInt(&ok);
        if (!ok) {
            localErrors << QString("第 %1 行 offset 必须是整数").arg(row + 1);
        }

        field.length = tableText(m_fieldConfigTable, row, FieldLengthColumn).toInt(&ok);
        if (!ok) {
            localErrors << QString("第 %1 行 length 必须是整数").arg(row + 1);
            field.length = 0;
        }
        const int defaultLength = ProtocolConfig::typeDefaultLength(field.type);
        if (field.type != "raw_hex" && defaultLength > 0) {
            field.length = defaultLength;
        }

        field.scale = tableText(m_fieldConfigTable, row, FieldScaleColumn).isEmpty()
                          ? 1.0
                          : tableText(m_fieldConfigTable, row, FieldScaleColumn).toDouble(&ok);
        if (!ok && !tableText(m_fieldConfigTable, row, FieldScaleColumn).isEmpty()) {
            localErrors << QString("第 %1 行 scale 必须是数字").arg(row + 1);
        }

        field.bias = tableText(m_fieldConfigTable, row, FieldBiasColumn).isEmpty()
                         ? 0.0
                         : tableText(m_fieldConfigTable, row, FieldBiasColumn).toDouble(&ok);
        if (!ok && !tableText(m_fieldConfigTable, row, FieldBiasColumn).isEmpty()) {
            localErrors << QString("第 %1 行 bias 必须是数字").arg(row + 1);
        }

        field.unit = tableText(m_fieldConfigTable, row, FieldUnitColumn);
        field.decimals = tableText(m_fieldConfigTable, row, FieldDecimalsColumn).isEmpty()
                             ? 0
                             : tableText(m_fieldConfigTable, row, FieldDecimalsColumn).toInt(&ok);
        if (!ok && !tableText(m_fieldConfigTable, row, FieldDecimalsColumn).isEmpty()) {
            localErrors << QString("第 %1 行 decimals 必须是整数").arg(row + 1);
        }

        const QString minText = tableText(m_fieldConfigTable, row, FieldMinColumn).trimmed();
        if (!minText.isEmpty()) {
            field.hasMin = true;
            field.minValue = minText.toDouble(&ok);
            if (!ok) {
                localErrors << QString("第 %1 行 min 必须是数字").arg(row + 1);
            }
        }

        const QString maxText = tableText(m_fieldConfigTable, row, FieldMaxColumn).trimmed();
        if (!maxText.isEmpty()) {
            field.hasMax = true;
            field.maxValue = maxText.toDouble(&ok);
            if (!ok) {
                localErrors << QString("第 %1 行 max 必须是数字").arg(row + 1);
            }
        }

        field.display = tableText(m_fieldConfigTable, row, FieldDisplayColumn).trimmed();
        if (field.display.isEmpty()) {
            field.display = (field.type == "bool_uint8") ? "bool" : "number";
        }
        field.enumMap = ProtocolConfig::enumMapFromString(tableText(m_fieldConfigTable, row, FieldEnumMapColumn));
        field.visible = textToBool(tableText(m_fieldConfigTable, row, FieldVisibleColumn));
        field.plot = textToBool(tableText(m_fieldConfigTable, row, FieldPlotColumn));
        fields.append(field);
    }

    if (errors) {
        errors->append(localErrors);
    }
    return fields;
}

QComboBox *MainWindow::createFieldTypeCombo(const QString &type)
{
    QComboBox *combo = new QComboBox(m_fieldConfigTable);
    combo->setEditable(true);
    combo->setMinimumWidth(118);
    combo->addItems(ProtocolConfig::supportedFieldTypes());
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setMaxVisibleItems(10);

    QCompleter *completer = new QCompleter(ProtocolConfig::supportedFieldTypes(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);

    const int index = combo->findText(type, Qt::MatchFixedString);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->setCurrentText(type.isEmpty() ? "uint8" : type);
    }

    connect(combo, &QComboBox::currentTextChanged, this, [this, combo](const QString &text) {
        const int row = m_fieldConfigTable->indexAt(combo->pos()).row();
        if (row < 0) {
            return;
        }
        const int defaultLength = ProtocolConfig::typeDefaultLength(text.trimmed());
        if (defaultLength > 0) {
            setTableText(m_fieldConfigTable, row, FieldLengthColumn, QString::number(defaultLength));
        }
        if (text.trimmed() == "bool_uint8") {
            setTableText(m_fieldConfigTable, row, FieldDisplayColumn, "bool");
        } else if (text.trimmed() == "raw_hex") {
            setTableText(m_fieldConfigTable, row, FieldDisplayColumn, "hex");
        }
    });

    return combo;
}

QString MainWindow::fieldTypeAtRow(int row) const
{
    QWidget *widget = m_fieldConfigTable->cellWidget(row, FieldTypeColumn);
    if (QComboBox *combo = qobject_cast<QComboBox *>(widget)) {
        return combo->currentText();
    }
    return tableText(m_fieldConfigTable, row, FieldTypeColumn);
}

void MainWindow::setFieldTypeAtRow(int row, const QString &type)
{
    m_fieldConfigTable->removeCellWidget(row, FieldTypeColumn);
    m_fieldConfigTable->setCellWidget(row, FieldTypeColumn, createFieldTypeCombo(type));
}

void MainWindow::populateValueTable(const QVector<FieldConfig> &fields)
{
    int visibleCount = 0;
    for (const FieldConfig &field : fields) {
        if (field.visible) {
            visibleCount++;
        }
    }
    m_valueTable->setRowCount(visibleCount);

    int row = 0;
    for (const FieldConfig &field : fields) {
        if (!field.visible) {
            continue;
        }
        setTableText(m_valueTable, row, 0, field.name);
        setTableText(m_valueTable, row, 1, field.type);
        setTableText(m_valueTable, row, 2, QString::number(field.offset));
        setTableText(m_valueTable, row, 3, "--");
        setTableText(m_valueTable, row, 4, field.unit);
        setTableText(m_valueTable, row, 5, "等待数据");
        row++;
    }
}

void MainWindow::updateValueTable(const QVector<FieldValue> &values)
{
    for (const FieldValue &value : values) {
        for (int row = 0; row < m_valueTable->rowCount(); ++row) {
            if (tableText(m_valueTable, row, 0) != value.name) {
                continue;
            }
            setTableText(m_valueTable, row, 3, value.displayValue);
            setTableText(m_valueTable, row, 5, value.statusMessage);
            const QColor color = value.abnormal ? QColor("#ff8a65") : QColor("#79f2b6");
            for (int col = 0; col < m_valueTable->columnCount(); ++col) {
                QTableWidgetItem *item = m_valueTable->item(row, col);
                if (item) {
                    item->setForeground(color);
                }
            }
            break;
        }
    }
}

void MainWindow::applySerialDefaultsToUi(const SerialDefaults &serial)
{
    m_baudCombo->setCurrentText(QString::number(serial.baudrate));
    m_dataBitsCombo->setCurrentText(QString::number(serial.dataBits));
    m_stopBitsCombo->setCurrentText(serial.stopBits);
    m_parityCombo->setCurrentText(serial.parity);
}

SerialDefaults MainWindow::readSerialDefaultsFromUi() const
{
    SerialDefaults serial;
    serial.baudrate = m_baudCombo->currentText().toInt();
    serial.dataBits = m_dataBitsCombo->currentText().toInt();
    serial.stopBits = m_stopBitsCombo->currentText();
    serial.parity = m_parityCombo->currentText();
    return serial;
}

RawDisplaySettings MainWindow::readRawDisplaySettingsFromUi() const
{
    RawDisplaySettings settings;
    settings.mode = m_rawModeCombo->currentData().toString();
    settings.encoding = m_rawEncodingCombo->currentText();
    settings.paused = m_rawPauseCheck->isChecked();
    settings.autoScroll = m_rawAutoScrollCheck->isChecked();
    settings.showTimestamp = m_rawTimestampCheck->isChecked();
    settings.maxLines = m_rawMaxLinesSpin->value();
    return settings;
}

void MainWindow::appendRawDataLine(const QString &direction, const QByteArray &data)
{
    const RawDisplaySettings settings = readRawDisplaySettingsFromUi();
    if (settings.paused) {
        return;
    }

    QString prefix;
    if (settings.showTimestamp) {
        prefix = "[" + QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "] ";
    }
    prefix += direction + " ";

    QString line;
    if (settings.mode == "TEXT") {
        QString warning;
        const QString text = sanitizeText(EncodingUtil::decode(data, settings.encoding, &warning));
        if (!warning.isEmpty()) {
            appendLog(warning);
        }
        line = prefix + "TEXT: " + text;
    } else if (settings.mode == "BOTH") {
        QString warning;
        const QString text = sanitizeText(EncodingUtil::decode(data, settings.encoding, &warning));
        if (!warning.isEmpty()) {
            appendLog(warning);
        }
        line = prefix + "HEX: " + HexUtil::bytesToHexString(data) + " | TEXT: " + text;
    } else {
        line = prefix + "HEX: " + HexUtil::bytesToHexString(data);
    }

    m_rawDataEdit->appendPlainText(line);
    if (settings.autoScroll) {
        m_rawDataEdit->verticalScrollBar()->setValue(m_rawDataEdit->verticalScrollBar()->maximum());
    }
}

void MainWindow::appendLog(const QString &message, bool isError)
{
    if (!m_logEdit) {
        return;
    }
    const QString color = isError ? "#ff6b6b" : "#e7eef6";
    const QString prefix = isError ? "ERROR" : "INFO";
    const QString line = QString("[%1] %2 %3")
                             .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), prefix, message);
    m_logEdit->append(QString("<span style=\"color:%1;\">%2</span>")
                          .arg(color, line.toHtmlEscaped().replace("\n", "<br>")));
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
}

void MainWindow::setOnlineBadge(bool online)
{
    m_onlineBadge->setText(online ? "在线" : "离线");
    m_onlineBadge->setProperty("online", online);
    m_onlineBadge->style()->unpolish(m_onlineBadge);
    m_onlineBadge->style()->polish(m_onlineBadge);
}

QString MainWindow::currentRawEncoding() const
{
    return m_rawEncodingCombo->currentText();
}

QString MainWindow::sanitizeText(const QString &text) const
{
    QString result;
    result.reserve(text.size());
    for (const QChar &ch : text) {
        if (ch.isPrint() || ch == '\t') {
            result.append(ch);
        } else {
            result.append('.');
        }
    }
    return result;
}

QString MainWindow::profileFileName(const QString &profileName) const
{
    QString fileName = profileName.trimmed();
    if (fileName.isEmpty()) {
        fileName = "profile";
    }
    fileName.replace(QRegularExpression("[^A-Za-z0-9_\\-]+"), "_");
    return fileName + ".json";
}

QString MainWindow::boolToText(bool value)
{
    return value ? "true" : "false";
}

bool MainWindow::textToBool(const QString &text)
{
    const QString t = text.trimmed().toLower();
    return t == "true" || t == "1" || t == "yes" || t == "是";
}

void MainWindow::setTableText(QTableWidget *table, int row, int column, const QString &text)
{
    QTableWidgetItem *item = table->item(row, column);
    if (!item) {
        item = new QTableWidgetItem;
        table->setItem(row, column, item);
    }
    item->setText(text);
}

QString MainWindow::tableText(const QTableWidget *table, int row, int column)
{
    const QTableWidgetItem *item = table->item(row, column);
    return item ? item->text() : QString();
}

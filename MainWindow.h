#pragma once

#include "ConfigManager.h"
#include "CurvePanel.h"
#include "ProtocolConfig.h"
#include "ProtocolParser.h"
#include "SerialService.h"

#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QMap>
#include <QTimer>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDialog;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QWidget;
class QVBoxLayout;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void refreshPorts();
    void refreshConfigList();
    void loadSelectedConfig();
    void saveCurrentConfig();
    void saveConfigAs();
    void importConfig();
    void openConfigFolder();
    void applyUiConfig();
    void autoCalculateFieldLayout();
    void showFieldConfigHelp();
    void toggleSerialPort();
    void handleSerialData(const QByteArray &data);
    void handleFrameParsed(const ParseResult &result);
    void handleParserStats(const ParserStats &stats);
    void handleSerialStateChanged(bool opened);
    void sendData();
    void sendRemoteFrame();
    void toggleRemoteSending();
    void updateRemoteSendInterval();
    void updateOnlineState();
    void syncPlotFieldFromCurve(const QString &fieldName, bool enabled);
    void openDetachedCurveWindow();

private:
    struct RemoteFieldEditor
    {
        FieldConfig field;
        QLineEdit *valueEdit = nullptr;
        QSlider *slider = nullptr;
        QCheckBox *switchBox = nullptr;
        QComboBox *enumCombo = nullptr;
        QDoubleSpinBox *minSpin = nullptr;
        QDoubleSpinBox *maxSpin = nullptr;
    };

    void setupUi();
    QWidget *setupHeader();
    QGroupBox *setupSerialPanel();
    QGroupBox *setupConfigPanel();
    QWidget *setupValuePanel();
    QWidget *setupProtocolTabs();
    QWidget *setupReceivePage();
    QWidget *setupRemoteControlPage();
    QGroupBox *setupRemoteControlPanel();
    QWidget *setupRawAndLogPanel();
    QGroupBox *setupSendPanel();
    QWidget *setupStatsPanel();
    void setupConnections();
    void loadStyleSheet();

    void applyConfigToUi(const ProtocolConfig &config);
    bool readConfigFromUi(ProtocolConfig *config, QStringList *errors) const;
    void populateFieldConfigTable(const QVector<FieldConfig> &fields);
    QVector<FieldConfig> readFieldConfigTable(QStringList *errors) const;
    QComboBox *createFieldTypeCombo(const QString &type);
    QString fieldTypeAtRow(int row) const;
    void setFieldTypeAtRow(int row, const QString &type);
    void populateValueTable(const QVector<FieldConfig> &fields);
    void updateValueTable(const QVector<FieldValue> &values);
    void populateRemoteControlPanel(const QVector<FieldConfig> &fields);
    QWidget *createRemoteFieldRow(const FieldConfig &field, RemoteFieldEditor *editor);
    void updateRemoteFramePreview();
    bool buildRemoteFrame(QByteArray *frame, QStringList *errors) const;
    bool transmitRemoteFrame(bool showDialog);
    void stopRemoteSending();
    void applySerialDefaultsToUi(const SerialDefaults &serial);
    SerialDefaults readSerialDefaultsFromUi() const;
    RawDisplaySettings readRawDisplaySettingsFromUi() const;

    void appendRawDataLine(const QString &direction, const QByteArray &data);
    void appendLog(const QString &message, bool isError = false);
    void showUserGuide();
    void setOnlineBadge(bool online);
    QString currentRawEncoding() const;
    QString sanitizeText(const QString &text) const;
    QString configDisplayName(const ConfigInfo &info) const;
    void updateConfigTitle();
    void updateConfigWatcher();
    QString profileFileName(const QString &profileName) const;
    static QString boolToText(bool value);
    static bool textToBool(const QString &text);
    static void setTableText(QTableWidget *table, int row, int column, const QString &text);
    static QString tableText(const QTableWidget *table, int row, int column);

    SerialService m_serialService;
    ProtocolParser m_parser;
    ConfigManager m_configManager;
    ProtocolConfig m_config;
    QString m_currentConfigPath;
    QFileSystemWatcher m_configWatcher;
    ParserStats m_lastStats;
    QTimer m_statusTimer;
    QTimer m_remoteSendTimer;

    QLabel *m_profileLabel = nullptr;
    QLabel *m_onlineBadge = nullptr;
    QLabel *m_headerRateLabel = nullptr;

    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QPushButton *m_openCloseButton = nullptr;
    QLabel *m_portStateLabel = nullptr;

    QComboBox *m_profileCombo = nullptr;

    QTableWidget *m_valueTable = nullptr;
    QTableWidget *m_fieldConfigTable = nullptr;
    CurvePanel *m_curvePanel = nullptr;
    QVector<CurvePanel *> m_detachedCurvePanels;

    QLineEdit *m_profileNameEdit = nullptr;
    QLineEdit *m_frameLengthEdit = nullptr;
    QLineEdit *m_headerEdit = nullptr;
    QLineEdit *m_tailEdit = nullptr;
    QComboBox *m_endianCombo = nullptr;
    QComboBox *m_frameModeCombo = nullptr;

    QCheckBox *m_crcEnabledCheck = nullptr;
    QComboBox *m_crcTypeCombo = nullptr;
    QLineEdit *m_crcOffsetEdit = nullptr;
    QLineEdit *m_crcLengthEdit = nullptr;
    QLineEdit *m_crcRangeStartEdit = nullptr;
    QLineEdit *m_crcRangeLengthEdit = nullptr;

    QComboBox *m_rawModeCombo = nullptr;
    QComboBox *m_rawEncodingCombo = nullptr;
    QCheckBox *m_rawPauseCheck = nullptr;
    QCheckBox *m_rawAutoScrollCheck = nullptr;
    QCheckBox *m_rawTimestampCheck = nullptr;
    QSpinBox *m_rawMaxLinesSpin = nullptr;
    QPlainTextEdit *m_rawDataEdit = nullptr;
    QTextEdit *m_logEdit = nullptr;

    QComboBox *m_sendModeCombo = nullptr;
    QComboBox *m_sendEncodingCombo = nullptr;
    QComboBox *m_sendNewlineCombo = nullptr;
    QLineEdit *m_sendEdit = nullptr;

    QWidget *m_remoteFieldContainer = nullptr;
    QVBoxLayout *m_remoteFieldLayout = nullptr;
    QVector<RemoteFieldEditor> m_remoteEditors;
    QSpinBox *m_remoteFrequencySpin = nullptr;
    QPushButton *m_remoteStartStopButton = nullptr;
    QPlainTextEdit *m_remotePreviewEdit = nullptr;
    QLabel *m_remoteFrameInfoLabel = nullptr;

    QLabel *m_totalBytesLabel = nullptr;
    QLabel *m_candidateFramesLabel = nullptr;
    QLabel *m_validFramesLabel = nullptr;
    QLabel *m_frameRateLabel = nullptr;
    QLabel *m_headerErrorsLabel = nullptr;
    QLabel *m_tailErrorsLabel = nullptr;
    QLabel *m_crcErrorsLabel = nullptr;
    QLabel *m_lengthErrorsLabel = nullptr;
    QLabel *m_fieldErrorsLabel = nullptr;
    QLabel *m_discardedBytesLabel = nullptr;
    QLabel *m_rxBufferLengthLabel = nullptr;
};

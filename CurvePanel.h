#pragma once

#include "ProtocolConfig.h"

#include <QColor>
#include <QHash>
#include <QMap>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

QT_BEGIN_NAMESPACE
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
QT_END_NAMESPACE

class CurvePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CurvePanel(QWidget *parent = nullptr);

    void setConfig(const ProtocolConfig &config);
    CurveSettings settings() const;
    QMap<QString, bool> plotFieldStates() const;
    void setPlotFieldEnabled(const QString &fieldName, bool enabled);

public slots:
    void appendFrame(const ParseResult &result);
    void clearCurves();

signals:
    void plotFieldChanged(const QString &fieldName, bool enabled);
    void detachRequested();

private:
    struct SeriesState
    {
        QLineSeries *series = nullptr;
        QVector<QPointF> points;
        QColor color;
        QString type;
        QString unit;
        int tableRow = -1;
        bool enabled = false;
    };

    void setupUi();
    void rebuildFieldList(const QVector<FieldConfig> &fields);
    void applySettingsToUi(const CurveSettings &settings);
    void trimSeries(SeriesState *state, double latestX);
    void refreshAxes(bool forceX = false);
    void refreshManualYControls();
    void updateCurrentValue(const QString &fieldName, const FieldValue &value);
    void setCurveSpinBoxSize(QWidget *spinBox);
    QColor colorForIndex(int index) const;
    static bool isPlottableType(const QString &type);

    QChart *m_chart = nullptr;
    QChartView *m_chartView = nullptr;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    QTableWidget *m_fieldTable = nullptr;
    QCheckBox *m_pauseCheck = nullptr;
    QCheckBox *m_autoScrollCheck = nullptr;
    QCheckBox *m_autoScaleYCheck = nullptr;
    QSpinBox *m_timeWindowSpin = nullptr;
    QSpinBox *m_maxPointsSpin = nullptr;
    QDoubleSpinBox *m_yMinSpin = nullptr;
    QDoubleSpinBox *m_yMaxSpin = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_detachButton = nullptr;

    QHash<QString, SeriesState> m_series;
    QStringList m_fieldOrder;
    bool m_updatingTable = false;
    qint64 m_firstTimestampMs = 0;
    double m_latestX = 0.0;
};

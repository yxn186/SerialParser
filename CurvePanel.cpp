#include "CurvePanel.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtMath>

namespace {
constexpr int CurveShowColumn = 0;
constexpr int CurveNameColumn = 1;
constexpr int CurveTypeColumn = 2;
constexpr int CurveUnitColumn = 3;
constexpr int CurveColorColumn = 4;
constexpr int CurveValueColumn = 5;
}

CurvePanel::CurvePanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void CurvePanel::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    QGroupBox *controlGroup = new QGroupBox("曲线控制", this);
    QGridLayout *controlLayout = new QGridLayout(controlGroup);

    m_pauseCheck = new QCheckBox("暂停曲线", controlGroup);
    m_autoScrollCheck = new QCheckBox("自动滚动", controlGroup);
    m_autoScaleYCheck = new QCheckBox("Y 轴自适应", controlGroup);
    m_timeWindowSpin = new QSpinBox(controlGroup);
    m_timeWindowSpin->setRange(1, 3600);
    m_timeWindowSpin->setSuffix(" s");
    m_maxPointsSpin = new QSpinBox(controlGroup);
    m_maxPointsSpin->setRange(100, 200000);
    m_maxPointsSpin->setSingleStep(100);
    m_yMinSpin = new QDoubleSpinBox(controlGroup);
    m_yMinSpin->setRange(-1.0e9, 1.0e9);
    m_yMinSpin->setDecimals(3);
    m_yMaxSpin = new QDoubleSpinBox(controlGroup);
    m_yMaxSpin->setRange(-1.0e9, 1.0e9);
    m_yMaxSpin->setDecimals(3);
    setCurveSpinBoxSize(m_timeWindowSpin);
    setCurveSpinBoxSize(m_maxPointsSpin);
    setCurveSpinBoxSize(m_yMinSpin);
    setCurveSpinBoxSize(m_yMaxSpin);
    m_clearButton = new QPushButton("清空曲线", controlGroup);
    m_detachButton = new QPushButton("独立窗口", controlGroup);

    controlLayout->addWidget(m_pauseCheck, 0, 0);
    controlLayout->addWidget(m_autoScrollCheck, 0, 1);
    controlLayout->addWidget(m_autoScaleYCheck, 0, 2);
    controlLayout->addWidget(new QLabel("时间窗口", controlGroup), 0, 3);
    controlLayout->addWidget(m_timeWindowSpin, 0, 4);
    controlLayout->addWidget(new QLabel("最大点数", controlGroup), 1, 0);
    controlLayout->addWidget(m_maxPointsSpin, 1, 1);
    controlLayout->addWidget(new QLabel("Y 最小", controlGroup), 1, 2);
    controlLayout->addWidget(m_yMinSpin, 1, 3);
    controlLayout->addWidget(new QLabel("Y 最大", controlGroup), 1, 4);
    controlLayout->addWidget(m_yMaxSpin, 1, 5);
    controlLayout->addWidget(m_clearButton, 0, 5);
    controlLayout->addWidget(m_detachButton, 0, 6);

    m_chart = new QChart;
    m_chart->setBackgroundBrush(QColor("#0e141c"));
    m_chart->setPlotAreaBackgroundBrush(QColor("#101923"));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setLabelColor(QColor("#d7e2ee"));
    m_chart->setTitle("实时曲线");
    m_chart->setTitleBrush(QColor("#72f7d0"));

    m_axisX = new QValueAxis;
    m_axisX->setTitleText("Time (s)");
    m_axisX->setLabelFormat("%.1f");
    m_axisX->setRange(0.0, 60.0);
    m_axisX->setLabelsColor(QColor("#9fb2c2"));
    m_axisX->setTitleBrush(QColor("#9fb2c2"));
    m_axisX->setGridLineColor(QColor("#243442"));

    m_axisY = new QValueAxis;
    m_axisY->setTitleText("Value");
    m_axisY->setLabelFormat("%.3f");
    m_axisY->setRange(-10.0, 10.0);
    m_axisY->setLabelsColor(QColor("#9fb2c2"));
    m_axisY->setTitleBrush(QColor("#9fb2c2"));
    m_axisY->setGridLineColor(QColor("#243442"));

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing, true);
    m_chartView->setMinimumHeight(260);

    m_fieldTable = new QTableWidget(this);
    m_fieldTable->setColumnCount(6);
    m_fieldTable->setHorizontalHeaderLabels({"显示", "字段名", "类型", "单位", "颜色", "当前值"});
    m_fieldTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldTable->horizontalHeader()->setStretchLastSection(true);
    m_fieldTable->verticalHeader()->setVisible(false);
    m_fieldTable->setAlternatingRowColors(true);
    m_fieldTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fieldTable->setMaximumHeight(190);

    root->addWidget(controlGroup);
    root->addWidget(m_chartView, 1);
    root->addWidget(m_fieldTable);

    connect(m_clearButton, &QPushButton::clicked, this, &CurvePanel::clearCurves);
    connect(m_detachButton, &QPushButton::clicked, this, &CurvePanel::detachRequested);
    connect(m_fieldTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (m_updatingTable || !item || item->column() != CurveShowColumn) {
            return;
        }
        const QString fieldName = item->data(Qt::UserRole).toString();
        if (!m_series.contains(fieldName)) {
            return;
        }
        const bool enabled = item->checkState() == Qt::Checked;
        SeriesState &state = m_series[fieldName];
        state.enabled = enabled;
        if (state.series) {
            state.series->setVisible(enabled);
        }
        emit plotFieldChanged(fieldName, enabled);
        refreshAxes();
    });

    auto refreshOnChange = [this]() {
        refreshManualYControls();
        refreshAxes(true);
    };
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, refreshOnChange);
    connect(m_autoScaleYCheck, &QCheckBox::toggled, this, refreshOnChange);
    connect(m_timeWindowSpin, qOverload<int>(&QSpinBox::valueChanged), this, refreshOnChange);
    connect(m_maxPointsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
        for (const QString &fieldName : m_fieldOrder) {
            SeriesState &state = m_series[fieldName];
            trimSeries(&state, m_latestX);
            if (state.series) {
                state.series->replace(state.points);
            }
        }
        refreshAxes(true);
    });
    auto manualYChanged = [this]() {
        if (m_autoScaleYCheck->isChecked()) {
            m_autoScaleYCheck->setChecked(false);
        }
        refreshAxes(true);
    };
    connect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, manualYChanged);
    connect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, manualYChanged);

    applySettingsToUi(CurveSettings{});
}

void CurvePanel::setConfig(const ProtocolConfig &config)
{
    applySettingsToUi(config.curve);
    rebuildFieldList(config.fields);
    clearCurves();
}

CurveSettings CurvePanel::settings() const
{
    CurveSettings result;
    result.timeWindowSeconds = m_timeWindowSpin->value();
    result.maxPoints = m_maxPointsSpin->value();
    result.autoScroll = m_autoScrollCheck->isChecked();
    result.autoScaleY = m_autoScaleYCheck->isChecked();
    result.manualYMin = m_yMinSpin->value();
    result.manualYMax = m_yMaxSpin->value();
    return result;
}

QMap<QString, bool> CurvePanel::plotFieldStates() const
{
    QMap<QString, bool> result;
    for (const QString &fieldName : m_fieldOrder) {
        result.insert(fieldName, m_series.value(fieldName).enabled);
    }
    return result;
}

void CurvePanel::setPlotFieldEnabled(const QString &fieldName, bool enabled)
{
    if (!m_series.contains(fieldName)) {
        return;
    }

    SeriesState &state = m_series[fieldName];
    state.enabled = enabled;
    if (state.series) {
        state.series->setVisible(enabled);
    }
    if (state.tableRow >= 0) {
        QSignalBlocker blocker(m_fieldTable);
        QTableWidgetItem *item = m_fieldTable->item(state.tableRow, CurveShowColumn);
        if (item) {
            item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
        }
    }
    refreshAxes();
}

void CurvePanel::appendFrame(const ParseResult &result)
{
    if (m_pauseCheck->isChecked()) {
        return;
    }

    const qint64 timestampMs = result.timestamp.isValid()
                                   ? result.timestamp.toMSecsSinceEpoch()
                                   : QDateTime::currentMSecsSinceEpoch();
    if (m_firstTimestampMs == 0) {
        m_firstTimestampMs = timestampMs;
    }
    m_latestX = static_cast<double>(timestampMs - m_firstTimestampMs) / 1000.0;

    for (const FieldValue &value : result.fieldValues) {
        if (!value.hasNumericValue || qIsNaN(value.numericValue) || qIsInf(value.numericValue)) {
            continue;
        }
        if (!m_series.contains(value.name)) {
            continue;
        }

        SeriesState &state = m_series[value.name];
        state.points.append(QPointF(m_latestX, value.numericValue));
        trimSeries(&state, m_latestX);
        if (state.series) {
            state.series->replace(state.points);
        }
        updateCurrentValue(value.name, value);
    }

    refreshAxes();
}

void CurvePanel::clearCurves()
{
    m_firstTimestampMs = 0;
    m_latestX = 0.0;
    for (const QString &fieldName : m_fieldOrder) {
        SeriesState &state = m_series[fieldName];
        state.points.clear();
        if (state.series) {
            state.series->clear();
        }
        if (state.tableRow >= 0) {
            QTableWidgetItem *item = m_fieldTable->item(state.tableRow, CurveValueColumn);
            if (item) {
                item->setText("--");
            }
        }
    }
    refreshAxes(true);
}

void CurvePanel::rebuildFieldList(const QVector<FieldConfig> &fields)
{
    QSet<QLineSeries *> removedSeries;
    for (const QString &fieldName : m_fieldOrder) {
        auto it = m_series.find(fieldName);
        if (it == m_series.end()) {
            continue;
        }
        QLineSeries *series = it->series;
        if (series && !removedSeries.contains(series)) {
            removedSeries.insert(series);
            m_chart->removeSeries(series);
            delete series;
        }
    }
    m_series.clear();
    m_fieldOrder.clear();

    m_updatingTable = true;
    m_fieldTable->clearContents();
    m_fieldTable->setRowCount(0);

    int row = 0;
    int colorIndex = 0;
    for (const FieldConfig &field : fields) {
        if (!isPlottableType(field.type)) {
            continue;
        }

        const QColor color = colorForIndex(colorIndex++);
        QLineSeries *series = new QLineSeries;
        series->setName(field.name);
        series->setColor(color);
        series->setPen(QPen(color, 2.0));
        series->setVisible(field.plot);
        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);

        SeriesState state;
        state.series = series;
        state.color = color;
        state.type = field.type;
        state.unit = field.unit;
        state.tableRow = row;
        state.enabled = field.plot;
        m_series.insert(field.name, state);
        m_fieldOrder.append(field.name);

        m_fieldTable->insertRow(row);
        QTableWidgetItem *showItem = new QTableWidgetItem;
        showItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        showItem->setCheckState(field.plot ? Qt::Checked : Qt::Unchecked);
        showItem->setData(Qt::UserRole, field.name);
        m_fieldTable->setItem(row, CurveShowColumn, showItem);

        QTableWidgetItem *nameItem = new QTableWidgetItem(field.name);
        QTableWidgetItem *typeItem = new QTableWidgetItem(field.type);
        QTableWidgetItem *unitItem = new QTableWidgetItem(field.unit);
        QTableWidgetItem *colorItem = new QTableWidgetItem(color.name().toUpper());
        colorItem->setForeground(color);
        QTableWidgetItem *valueItem = new QTableWidgetItem("--");

        m_fieldTable->setItem(row, CurveNameColumn, nameItem);
        m_fieldTable->setItem(row, CurveTypeColumn, typeItem);
        m_fieldTable->setItem(row, CurveUnitColumn, unitItem);
        m_fieldTable->setItem(row, CurveColorColumn, colorItem);
        m_fieldTable->setItem(row, CurveValueColumn, valueItem);
        row++;
    }
    m_updatingTable = false;
    refreshAxes(true);
}

void CurvePanel::applySettingsToUi(const CurveSettings &settings)
{
    const QSignalBlocker blockPause(m_pauseCheck);
    const QSignalBlocker blockScroll(m_autoScrollCheck);
    const QSignalBlocker blockScale(m_autoScaleYCheck);
    const QSignalBlocker blockWindow(m_timeWindowSpin);
    const QSignalBlocker blockPoints(m_maxPointsSpin);
    const QSignalBlocker blockMin(m_yMinSpin);
    const QSignalBlocker blockMax(m_yMaxSpin);

    m_pauseCheck->setChecked(false);
    m_autoScrollCheck->setChecked(settings.autoScroll);
    m_autoScaleYCheck->setChecked(settings.autoScaleY);
    m_timeWindowSpin->setValue(qBound(1, settings.timeWindowSeconds, 3600));
    m_maxPointsSpin->setValue(qBound(100, settings.maxPoints, 200000));
    m_yMinSpin->setValue(settings.manualYMin);
    m_yMaxSpin->setValue(settings.manualYMax);
    refreshManualYControls();
}

void CurvePanel::trimSeries(SeriesState *state, double latestX)
{
    if (!state) {
        return;
    }

    const CurveSettings current = settings();
    const double minAllowedX = qMax(0.0, latestX - static_cast<double>(current.timeWindowSeconds));
    while (!state->points.isEmpty() && state->points.first().x() < minAllowedX) {
        state->points.removeFirst();
    }
    if (state->points.size() > current.maxPoints) {
        state->points.remove(0, state->points.size() - current.maxPoints);
    }
}

void CurvePanel::refreshAxes(bool forceX)
{
    const CurveSettings current = settings();

    if (current.autoScroll || forceX || m_axisX->max() <= m_axisX->min()) {
        double xMin = 0.0;
        double xMax = static_cast<double>(current.timeWindowSeconds);
        if (m_latestX > 0.0) {
            xMax = qMax(static_cast<double>(current.timeWindowSeconds), m_latestX);
            xMin = qMax(0.0, xMax - static_cast<double>(current.timeWindowSeconds));
        }
        m_axisX->setRange(xMin, xMax);
    }

    if (!current.autoScaleY) {
        double yMin = current.manualYMin;
        double yMax = current.manualYMax;
        if (yMin >= yMax) {
            yMax = yMin + 1.0;
        }
        m_axisY->setRange(yMin, yMax);
        return;
    }

    const double xMin = m_axisX->min();
    const double xMax = m_axisX->max();
    bool found = false;
    double yMin = 0.0;
    double yMax = 0.0;
    for (const QString &fieldName : m_fieldOrder) {
        const SeriesState &state = m_series[fieldName];
        if (!state.enabled) {
            continue;
        }
        for (const QPointF &point : state.points) {
            if (point.x() < xMin || point.x() > xMax) {
                continue;
            }
            if (!found) {
                yMin = yMax = point.y();
                found = true;
            } else {
                yMin = qMin(yMin, point.y());
                yMax = qMax(yMax, point.y());
            }
        }
    }

    if (!found) {
        m_axisY->setRange(current.manualYMin, current.manualYMax);
        return;
    }

    double margin = (yMax - yMin) * 0.1;
    if (qFuzzyIsNull(margin)) {
        margin = 1.0;
    }
    m_axisY->setRange(yMin - margin, yMax + margin);
}

void CurvePanel::refreshManualYControls()
{
    m_yMinSpin->setEnabled(true);
    m_yMaxSpin->setEnabled(true);
    const QString tip = m_autoScaleYCheck->isChecked()
                            ? "当前为 Y 轴自适应；手动修改此值会自动关闭 Y 轴自适应。"
                            : "手动 Y 轴范围已启用。";
    m_yMinSpin->setToolTip(tip);
    m_yMaxSpin->setToolTip(tip);
}

void CurvePanel::updateCurrentValue(const QString &fieldName, const FieldValue &value)
{
    if (!m_series.contains(fieldName)) {
        return;
    }
    const SeriesState &state = m_series[fieldName];
    if (state.tableRow < 0) {
        return;
    }
    QTableWidgetItem *item = m_fieldTable->item(state.tableRow, CurveValueColumn);
    if (!item) {
        item = new QTableWidgetItem;
        m_fieldTable->setItem(state.tableRow, CurveValueColumn, item);
    }
    item->setText(QString::number(value.numericValue, 'f', 3));
    item->setForeground(value.abnormal ? QColor("#ffb45c") : QColor("#d7e2ee"));
}

void CurvePanel::setCurveSpinBoxSize(QWidget *spinBox)
{
    if (!spinBox) {
        return;
    }
    spinBox->setObjectName("CurveSpinBox");
    spinBox->setMinimumHeight(40);
    spinBox->setMinimumWidth(116);
}

QColor CurvePanel::colorForIndex(int index) const
{
    static const QVector<QColor> colors = {
        QColor("#72f7d0"),
        QColor("#42cfe8"),
        QColor("#ffcf5c"),
        QColor("#ff7a90"),
        QColor("#9f8cff"),
        QColor("#79f2b6"),
        QColor("#f18cff"),
        QColor("#8cc8ff")
    };
    return colors.at(index % colors.size());
}

bool CurvePanel::isPlottableType(const QString &type)
{
    const QString t = type.trimmed().toLower();
    return t != "raw_hex" && ProtocolConfig::typeDefaultLength(t) > 0;
}

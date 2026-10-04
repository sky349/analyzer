#include "trackerevidencepanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace {
QString number(double value)
{
    return std::isfinite(value) ? QString::number(value, 'g', 12) : QObject::tr("unavailable");
}

void addFields(QTreeWidgetItem *parent, const QCborMap &fields)
{
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        const QCborValue value = it.value();
        auto *item = new QTreeWidgetItem(parent, { it.key().toString(), value.isNull() ? QObject::tr("unavailable")
                                                      : value.isDouble() ? number(value.toDouble())
                                                      : value.isMap() ? QString() : value.toDiagnosticNotation() });
        if (value.isMap())
            addFields(item, value.toMap());
    }
}
}

class TrackerEvidenceChart : public QWidget
{
public:
    explicit TrackerEvidenceChart(QWidget *parent) : QWidget(parent) { setMinimumHeight(120); }
    void setHistory(QSharedPointer<const Prp3TrackerLife> life, QString metric, double selectedTime)
    {
        m_life = std::move(life);
        m_metric = std::move(metric);
        m_selectedTime = selectedTime;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), palette().base());
        if (!m_life)
            return;
        QVector<QPointF> values;
        for (const auto &sample : std::as_const(m_life->samples))
            if (sample->event == QStringLiteral("evidence") && std::isfinite(sample->metric(m_metric, true)))
                values.append({ sample->time, sample->metric(m_metric, true) });
        if (values.isEmpty()) {
            painter.drawText(rect(), Qt::AlignCenter, tr("No finite recorded values for this metric"));
            return;
        }
        const auto bounds = std::accumulate(values.cbegin(), values.cend(), QRectF(values.first(), QSizeF()),
                [](const QRectF &b, const QPointF &p) {
                    return QRectF(QPointF(std::min(b.left(), p.x()), std::min(b.top(), p.y())),
                                  QPointF(std::max(b.right(), p.x()), std::max(b.bottom(), p.y())));
                });
        const auto lowLabel = number(bounds.top()), highLabel = number(bounds.bottom());
        const auto labelWidth = std::max(painter.fontMetrics().horizontalAdvance(lowLabel),
                                        painter.fontMetrics().horizontalAdvance(highLabel));
        const auto area = QRectF(rect()).adjusted(labelWidth + 12, 20, -20, -28);
        const auto point = [&area, &bounds](const QPointF &p) {
            return QPointF(area.left() + (p.x() - bounds.left()) / std::max(0.001, bounds.width()) * area.width(),
                           area.bottom() - (p.y() - bounds.top()) / std::max(0.001, bounds.height()) * area.height());
        };
        painter.setPen(palette().text().color());
        painter.drawText(QRectF(0, 0, labelWidth + 4, 25), Qt::AlignRight, highLabel);
        painter.drawText(QRectF(0, area.bottom() - 12, labelWidth + 4, 25), Qt::AlignRight, lowLabel);
        painter.drawText(QRectF(area.left(), area.bottom(), area.width(), 25), Qt::AlignCenter,
                         tr("%1 — %2 s (recorded observation time)").arg(number(bounds.left()), number(bounds.right())));
        painter.setPen(QPen(QColor(0, 145, 190), 2));
        // Separate dots preserve unavailable gaps and regressing observation times without inventing interpolation.
        for (const auto &value : std::as_const(values))
            painter.drawEllipse(point(value), 3, 3);
        painter.setPen(QPen(QColor(210, 120, 0), 1, Qt::DashLine));
        const auto selectedX = point({ m_selectedTime, bounds.top() }).x();
        if (selectedX >= area.left() && selectedX <= area.right())
            painter.drawLine(QPointF(selectedX, area.top()), QPointF(selectedX, area.bottom()));
    }

private:
    QSharedPointer<const Prp3TrackerLife> m_life;
    QString m_metric;
    double m_selectedTime = 0.0;
};

TrackerEvidencePanel::TrackerEvidencePanel(QWidget *parent)
    : QDockWidget(tr("Recorded tracker evidence"), parent),
      m_metric(new QComboBox(this)), m_scope(new QComboBox(this)), m_table(new QComboBox(this)),
      m_availability(new QComboBox(this)), m_context(new QComboBox(this)), m_colours(new QCheckBox(tr("Colour by metric"), this)),
      m_range(new QCheckBox(tr("Filter by range"), this)), m_minimum(new QDoubleSpinBox(this)),
      m_maximum(new QDoubleSpinBox(this)), m_summary(new QLabel(this)), m_details(new QTreeWidget(this)),
      m_history(new QTableWidget(this)), m_chart(new TrackerEvidenceChart(this))
{
    setObjectName(QStringLiteral("trackerEvidencePanel"));
    m_metric->setObjectName(QStringLiteral("trackerMetric"));
    m_scope->setObjectName(QStringLiteral("trackerScope"));
    m_table->setObjectName(QStringLiteral("trackerTable"));
    m_availability->setObjectName(QStringLiteral("trackerAvailability"));
    m_colours->setObjectName(QStringLiteral("trackerColours"));
    m_range->setObjectName(QStringLiteral("trackerRange"));
    m_minimum->setObjectName(QStringLiteral("trackerMinimum"));
    m_maximum->setObjectName(QStringLiteral("trackerMaximum"));
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    auto *metrics = new QHBoxLayout;
    auto *filters = new QHBoxLayout;
    for (const auto &[label, key] : { std::pair{ "H_T / H_C log evidence", "L_target_clutter" },
                                     { "Trajectory Jcv / dof", "Jcv_dof" }, { "Trajectory Jcv", "Jcv" },
                                     { "NIS", "NIS" }, { "H_T / Poisson", "L_target_poisson" },
                                     { "Persistent / Poisson", "L_persistent_poisson" },
                                     { "H_C / Poisson", "L_clutter_poisson" } })
        m_metric->addItem(tr(label), QLatin1String(key));
    m_scope->addItems({ tr("At each observation (segments)"), tr("At confirmation (whole lifetime)"),
                       tr("Last new H_T/H_C score (whole lifetime, retrospective)") });
    m_table->addItem(tr("All tables — ambiguous plots stay grey"), -1);
    m_availability->addItems({ tr("Any availability"), tr("Finite values only"), tr("Unavailable only"),
                              tr("New H_T/H_C score only") });
    for (auto *spin : { m_minimum, m_maximum }) {
        spin->setRange(-1e12, 1e12);
        spin->setDecimals(6);
    }
    m_minimum->setValue(-20.0);
    m_maximum->setValue(20.0);
    metrics->addWidget(m_metric);
    metrics->addWidget(m_scope);
    metrics->addWidget(m_table);
    filters->addWidget(m_colours);
    filters->addWidget(m_availability);
    filters->addWidget(m_range);
    filters->addWidget(m_minimum);
    filters->addWidget(m_maximum);
    layout->addLayout(metrics);
    layout->addLayout(filters);
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);
    layout->addWidget(m_context);
    auto *splitter = new QSplitter(content);
    m_details->setColumnCount(2);
    m_details->setHeaderLabels({ tr("Recorded field"), tr("Value") });
    m_details->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_history->setColumnCount(7);
    m_history->setHorizontalHeaderLabels({ tr("Time (s)"), tr("Event / phase"), tr("Input"), tr("H_T/H_C log"),
                                          tr("Jcv/dof"), tr("H status"), tr("Validity") });
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    splitter->addWidget(m_details);
    splitter->addWidget(m_history);
    layout->addWidget(splitter);
    layout->addWidget(m_chart);
    setWidget(content);
    const auto changed = [this] {
        updateInspection();
        emit displayChanged();
    };
    for (auto *combo : { m_metric, m_scope, m_table, m_availability })
        connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, changed);
    for (auto *check : { m_colours, m_range })
        connect(check, &QCheckBox::toggled, this, changed);
    for (auto *spin : { m_minimum, m_maximum })
        connect(spin, &QDoubleSpinBox::editingFinished, this, changed);
    connect(m_context, qOverload<int>(&QComboBox::currentIndexChanged), this, &TrackerEvidencePanel::updateInspection);
    connect(m_history, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (m_context->currentIndex() < 0 || m_context->currentIndex() >= m_uses.size())
            return;
        const auto &life = m_uses.at(m_context->currentIndex()).life;
        if (row >= 0 && row < life->samples.size()) {
            const auto sample = life->samples.at(row);
            showSample(sample);
            m_chart->setHistory(life, m_metric->currentData().toString(), sample->time);
        }
    });
    setRecording({});
}

void TrackerEvidencePanel::setRecording(QSharedPointer<const Prp3TrackerRecording> recording)
{
    m_recording = std::move(recording);
    const QSignalBlocker blocker(m_table);
    m_table->clear();
    m_table->addItem(tr("All tables — ambiguous plots stay grey"), -1);
    QSet<int> tables;
    if (m_recording)
        for (const auto &life : std::as_const(m_recording->lives))
            tables.insert(life->table);
    auto sorted = tables.values();
    std::sort(sorted.begin(), sorted.end());
    for (const auto table : std::as_const(sorted))
        m_table->addItem(tr("Table %1").arg(table), table);
    inspect({});
}

Prp3EvidenceScope TrackerEvidencePanel::scope() const
{
    return static_cast<Prp3EvidenceScope>(m_scope->currentIndex());
}

bool TrackerEvidencePanel::coloursEnabled() const
{
    return m_colours->isChecked();
}

QSharedPointer<const Prp3TrackerSample> TrackerEvidencePanel::selected(const QVector<Prp3TrackerUse> &uses) const
{
    QSharedPointer<const Prp3TrackerSample> result;
    int matches = 0;
    for (const auto &use : uses) {
        if (m_table->currentData().toInt() >= 0 && use.life->table != m_table->currentData().toInt())
            continue;
        const auto sample = prp3EvidenceSample(use, scope());
        if (++matches > 1)
            return {}; // One physical input can be committed independently by several tables/lifetimes.
        result = sample;
    }
    return result;
}

bool TrackerEvidencePanel::accepts(const QVector<Prp3TrackerUse> &uses) const
{
    if (!m_range->isChecked() && !m_availability->currentIndex() && m_table->currentData().toInt() < 0)
        return true;
    const auto sample = selected(uses);
    const auto value = sample
            ? sample->metric(m_metric->currentData().toString(), scope() == Prp3EvidenceScope::OBSERVATION)
            : std::numeric_limits<double>::quiet_NaN();
    const auto finite = std::isfinite(value);
    if (m_table->currentData().toInt() >= 0 && std::none_of(uses.cbegin(), uses.cend(), [this](const auto &use) {
            return use.life->table == m_table->currentData().toInt();
        }))
        return false;
    if ((m_availability->currentIndex() == 1 && !finite) || (m_availability->currentIndex() == 2 && finite)
        || (m_availability->currentIndex() == 3 && (!sample || !sample->updated() || !sample->warnings.isEmpty())))
        return false;
    return !m_range->isChecked() || (finite && value >= m_minimum->value() && value <= m_maximum->value());
}

QColor TrackerEvidencePanel::colour(const QVector<Prp3TrackerUse> &uses) const
{
    if (!coloursEnabled())
        return {};
    const auto sample = selected(uses);
    const auto value = sample
            ? sample->metric(m_metric->currentData().toString(), scope() == Prp3EvidenceScope::OBSERVATION)
            : std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(value))
        return QColor(125, 125, 125);
    const auto span = m_maximum->value() - m_minimum->value();
    const auto level = span > 0.0 ? std::clamp((value - m_minimum->value()) / span, 0.0, 1.0) : 0.5;
    return QColor::fromHsvF((1.0 - level) * 0.66, 0.75, 0.9);
}

QString TrackerEvidencePanel::summary(const QVector<Prp3TrackerUse> &uses) const
{
    const auto sample = selected(uses);
    return sample ? tr("Table %1 · lifetime %2 · %3 · %4 = %5")
                            .arg(sample->table).arg(sample->life).arg(m_scope->currentText(),
                                  m_metric->currentText(), number(sample->metric(
                                          m_metric->currentData().toString(), scope() == Prp3EvidenceScope::OBSERVATION)))
                  : uses.isEmpty() ? tr("No recorded tracker evidence")
                                   : tr("Several tracker uses or unavailable scope — select a table/context");
}

void TrackerEvidencePanel::inspect(QVector<Prp3TrackerUse> uses)
{
    m_uses = std::move(uses);
    const QSignalBlocker blocker(m_context);
    m_context->clear();
    for (const auto &use : std::as_const(m_uses))
        m_context->addItem(tr("Run %1 · table %2 · lifetime %3 · slot %4 · input %5")
                                  .arg(use.sample->run).arg(use.sample->table).arg(use.sample->life)
                                  .arg(use.sample->slot).arg(use.sample->input));
    updateInspection();
}

void TrackerEvidencePanel::showSample(const QSharedPointer<const Prp3TrackerSample> &sample)
{
    m_details->clear();
    if (!sample)
        return;
    auto fields = sample->fields;
    fields.remove(QStringLiteral("evidence"));
    addFields(m_details->invisibleRootItem(), fields);
    if (m_recording) {
        auto *run = new QTreeWidgetItem(m_details, { tr("Run configuration and clock assumptions") });
        addFields(run, m_recording->runs.value(sample->run));
    }
    auto *evidence = new QTreeWidgetItem(m_details, { tr("Evidence at this observation") });
    addFields(evidence, sample->evidence);
    new QTreeWidgetItem(evidence, { tr("Last new score age (s)"), number(sample->time - sample->lastScoreTime) });
    new QTreeWidgetItem(evidence, { tr("Validity"), sample->warnings.isEmpty() ? tr("recorded")
                                                                            : sample->warnings.join(QStringLiteral("; ")) });
    if (scope() == Prp3EvidenceScope::OBSERVATION && std::isfinite(sample->carriedTargetClutter)) {
        auto *carried = new QTreeWidgetItem(m_details, { tr("Display H_T/H_C carried forward") });
        new QTreeWidgetItem(carried, { tr("Value"), number(sample->carriedTargetClutter) });
        new QTreeWidgetItem(carried, { tr("Source observation time (s)"), number(sample->carriedScoreTime) });
        new QTreeWidgetItem(carried, { tr("Age (s)"), number(sample->time - sample->carriedScoreTime) });
        carried->setExpanded(true);
    }
    evidence->setExpanded(true);
}

void TrackerEvidencePanel::updateInspection()
{
    m_summary->setText(tr("Recorded scenario evidence, not aircraft probability. Blue = lower, red = higher; "
                          "range boxes also set colour limits. At each observation carries the last valid H_T/H_C "
                          "forward when no new score is recorded; details/history retain recorded fields. "
                          "Grey = unavailable, invalid or ambiguous. ")
                       + summary(m_uses));
    m_history->setRowCount(0);
    m_chart->setHistory({}, {}, 0.0);
    m_details->clear();
    if (m_context->currentIndex() < 0 || m_context->currentIndex() >= m_uses.size())
        return;
    const auto &use = m_uses.at(m_context->currentIndex());
    const auto sample = prp3EvidenceSample(use, scope());
    showSample(sample);
    if (!sample)
        m_summary->setText(m_summary->text() + tr(" This lifetime has no value at the selected scope."));
    m_chart->setHistory(use.life, m_metric->currentData().toString(), sample ? sample->time : use.sample->time);
    m_history->setRowCount(use.life->samples.size());
    for (int row = 0; row < use.life->samples.size(); ++row) {
        const auto &entry = use.life->samples.at(row);
        const QStringList fields { number(entry->time), entry->event + QLatin1Char(' ') + entry->phase,
                                   QString::number(entry->input), number(entry->metric(QStringLiteral("L_target_clutter"))),
                                   number(entry->metric(QStringLiteral("Jcv_dof"))),
                                   entry->evidence.value(QStringLiteral("hypothesis_reason")).toString(),
                                   entry->warnings.join(QStringLiteral("; ")) };
        for (int column = 0; column < fields.size(); ++column)
            m_history->setItem(row, column, new QTableWidgetItem(fields.at(column)));
    }
    m_history->resizeColumnsToContents();
}

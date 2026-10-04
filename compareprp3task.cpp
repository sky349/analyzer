#include "compareprp3task.h"
#include "prp3signalwindow.h"

#include <QComboBox>
#include <QCborArray>
#include <QCborMap>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTimer>
#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>

namespace {
using namespace Prp3PhaseDiagnostics;
constexpr double UNAVAILABLE = std::numeric_limits<double>::quiet_NaN();

QString number(double value)
{
    return std::isfinite(value) ? QString::number(value, 'f', 3) : QStringLiteral("—");
}

double median(std::vector<double> values)
{
    if (values.empty())
        return UNAVAILABLE;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2.0;
}

int competitiveCount(const Prp3ComparisonRow &row)
{
    return row.fits.empty() ? 0 : int(std::count_if(row.fits.cbegin(), row.fits.cend(), [&row](const auto &fit) {
        return fit.rmsDeg <= row.fits.front().rmsDeg + COMPETITIVE_RMS_DEG;
    }));
}

QString htmlRow(const QStringList &cells, bool heading = false)
{
    const auto tag = heading ? QStringLiteral("th") : QStringLiteral("td");
    return std::accumulate(cells.cbegin(), cells.cend(), QStringLiteral("<tr>"),
                           [&tag](const auto &text, const auto &cell) {
        return text + QStringLiteral("<%1>%2</%1>").arg(tag, cell.toHtmlEscaped());
    }) + QStringLiteral("</tr>");
}

const PsrDoppler::BranchSnapshot *diagnosticBranch(const Prp3PlotRecord &record, int channel)
{
    const auto &snapshot = record.snapshot.evidence.snapshot;
    if (!record.snapshot.decoded || (snapshot.signal == PsrDoppler::Signal::MH && channel)
        || (record.restoration.valid && !(record.restoration.branchMask & (1U << channel))))
        return nullptr;
    const auto end = snapshot.branches.cbegin() + snapshot.branchCount;
    const auto found = std::find_if(snapshot.branches.cbegin(), end, [channel](const auto &branch) {
        return int(branch.branch) == channel;
    });
    return found == end ? nullptr : &*found;
}

QStringList groupKey(const Prp3PlotRecord &record, QString &timing)
{
    auto payload = QCborValue::fromCbor(record.cborPayload);
    if (payload.isTag())
        payload = payload.taggedValue();
    const auto restoration = payload.toMap().value(QStringLiteral("restoration")).toMap();
    timing = restoration.value(QStringLiteral("pri_association")).toString(QStringLiteral("unrecorded"));
    if (record.compact)
        timing = record.runInfo.value(QStringLiteral("pri_association")).toString(QStringLiteral("unrecorded"));
    const auto &snapshot = record.snapshot.evidence.snapshot;
    QStringList key { record.filePath, QString::number(record.inputChannel), record.producerBuild,
                      record.snapshot.decoded ? QString::number(int(snapshot.mode)) : QStringLiteral("undecoded"),
                      QString::number(int(snapshot.signal)), QString::number(int(snapshot.bin)),
                      QString::number(record.restoration.branchMask), timing };
    if (record.compact)
        key.append(QString::fromLatin1(record.runInfo.value(QStringLiteral("doppler_settings")).toCbor().toHex()));
    // Preserve logged profile distinctions without assuming a missing profile means identity calibration.
    if (record.snapshot.decoded) {
        for (int index = 0; index < snapshot.branchCount; ++index)
            key.append(QStringLiteral("%1:%2").arg(int(snapshot.branches[index].branch))
                           .arg(snapshot.branches[index].sourceSic));
    }
    const auto branches = restoration.value(QStringLiteral("branches")).toArray();
    for (const auto &branch : branches) {
        const auto map = branch.toMap();
        key.append(QString::number(map.value(QStringLiteral("slot")).toInteger(-1)));
        key.append(QString::fromLatin1(map.value(QStringLiteral("calibration")).toCbor().toHex()));
    }
    return key;
}

Prp3RadialDiagnostics::Analysis radialAnalysis(const Prp3Records &records)
{
    QMap<QStringList, int> sources;
    std::vector<Prp3RadialDiagnostics::Point> points;
    points.reserve(size_t(records.size()));
    for (int index = 0; index < records.size(); ++index) {
        const auto &record = *records[index];
        if (!record.legacy.valid || !record.snapshot.decoded)
            continue;
        const auto &snapshot = record.snapshot.evidence.snapshot;
        const QStringList key { record.filePath, QString::number(record.inputChannel), record.producerBuild,
                                QString::number(int(snapshot.bin)) };
        const auto found = sources.find(key);
        const auto source = found == sources.end() ? sources.insert(key, sources.size()).value() : found.value();
        const auto angle = (90.0 - record.legacy.azimuthRaw * 360.0 / 16384.0) * PI / 180.0;
        points.push_back({ index, source, record.envelopeOffset, snapshot.scanNumber,
            record.arrivalUtcMs / 1000.0, record.legacy.rangeRaw * 75.0, std::cos(angle), std::sin(angle) });
    }
    return { std::move(points), records.size() };
}

void configure(QwtPlot *plot, const QString &title, const QString &xTitle, const QString &yTitle)
{
    plot->setTitle(title);
    plot->setCanvasBackground(Qt::white);
    plot->setAxisTitle(QwtPlot::xBottom, xTitle);
    plot->setAxisTitle(QwtPlot::yLeft, yTitle);
    plot->insertLegend(new QwtLegend(plot), QwtPlot::BottomLegend);
    auto *grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(Qt::lightGray, 0, Qt::DashLine));
    grid->attach(plot);
}

void curve(QwtPlot *plot, const QString &title, const QColor &color, const QVector<QPointF> &points)
{
    auto *item = new QwtPlotCurve(title);
    item->setStyle(QwtPlotCurve::NoCurve);
    item->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, QBrush(color), QPen(color), QSize(7, 7)));
    item->setSamples(points);
    item->attach(plot);
}

class NumericItem : public QTableWidgetItem
{
public:
    explicit NumericItem(double value, int decimals = 3)
        : QTableWidgetItem(std::isfinite(value) ? QString::number(value, 'f', decimals) : QStringLiteral("—"))
        , m_value(value)
    {
    }
    bool operator<(const QTableWidgetItem &other) const override
    {
        // Each numeric column is populated exclusively with NumericItem, including missing values.
        const auto &item = static_cast<const NumericItem &>(other);
        return std::isfinite(m_value) && (!std::isfinite(item.m_value) || m_value < item.m_value);
    }
private:
    double m_value = UNAVAILABLE;
};
}

Prp3ComparisonWindow::Prp3ComparisonWindow(Prp3Records records, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_records([&records] {
        std::stable_sort(records.begin(), records.end(), [](const auto &a, const auto &b) {
            return std::tie(a->arrivalUtcMs, a->filePath, a->sequence, a->envelopeOffset)
                < std::tie(b->arrivalUtcMs, b->filePath, b->sequence, b->envelopeOffset);
        });
        return std::move(records);
    }())
    , m_radialAnalysis(radialAnalysis(m_records))
    , m_channel(new QComboBox(this))
    , m_mapping(new QComboBox(this))
    , m_status(new QLabel(this))
    , m_radialStatus(new QLabel(this))
    , m_table(new QTableWidget(this))
    , m_summary(new QTextBrowser(this))
    , m_detail(new QTextBrowser(this))
    , m_frequencyPlot(new QwtPlot(this))
    , m_qualityPlot(new QwtPlot(this))
    , m_residualPlot(new QwtPlot(this))
    , m_radialPlot(new QwtPlot(this))
    , m_radialHistory(new QwtPlot(this))
    , m_timer(new QTimer(this))
{
    setWindowTitle(tr("Compare PRP3 plots"));
    setAttribute(Qt::WA_DeleteOnClose);
    m_channel->addItems({ tr("Channel A"), tr("Channel B (NFM only)") });
    m_mapping->addItem(tr("n→n+1: period[n]"), 0);
    m_mapping->addItem(tr("n→n+1: period[n+1]"), 1);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_table->setObjectName(QStringLiteral("prp3ComparisonTable"));
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->hide();
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    auto *reference = new QPushButton(tr("Use selected plot as raw reference"), this);
    auto *open = new QPushButton(tr("Open selected plot diagnostics"), this);
    auto *layout = new QGridLayout(this);
    layout->addWidget(m_status, 0, 0, 1, 4);
    layout->addWidget(m_channel, 1, 0);
    layout->addWidget(m_mapping, 1, 1);
    layout->addWidget(reference, 1, 2);
    layout->addWidget(open, 1, 3);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(m_table);
    auto *tabs = new QTabWidget(splitter);
    auto *overview = new QWidget(tabs);
    auto *charts = new QGridLayout(overview);
    charts->addWidget(m_frequencyPlot, 0, 0);
    charts->addWidget(m_qualityPlot, 0, 1);
    tabs->addTab(overview, tr("Frequency and phase fit"));
    auto *radial = new QWidget(tabs);
    auto *radialLayout = new QGridLayout(radial);
    m_radialStatus->setWordWrap(true);
    m_radialStatus->setTextFormat(Qt::PlainText);
    radialLayout->addWidget(m_radialStatus, 0, 0, 1, 2);
    radialLayout->addWidget(m_radialPlot, 1, 0);
    radialLayout->addWidget(m_radialHistory, 1, 1);
    tabs->addTab(radial, tr("Radial speed comparison"));
    tabs->addTab(m_summary, tr("Group statistics"));
    tabs->addTab(m_detail, tr("Selected plot and aliases"));
    tabs->addTab(m_residualPlot, tr("Selected plot residuals"));
    splitter->addWidget(tabs);
    splitter->setSizes({ 300, 400 });
    layout->addWidget(splitter, 2, 0, 1, 4);
    layout->setRowStretch(2, 1);
    for (auto *browser : { m_summary, m_detail }) {
        browser->setOpenLinks(false);
        browser->setOpenExternalLinks(false);
    }
    configure(m_frequencyPlot, tr("Logged valid Doppler — colour identifies comparison group"),
              tr("Plot # (chronological; table identity)"), tr("Logged Hz"));
    configure(m_qualityPlot, tr("Raw-phase fit and reference prediction"), tr("Plot #"), tr("Weighted RMS °"));
    configure(m_residualPlot, tr("Selected plot — supported transitions only"),
              tr("Transition n→n+1 at n+0.5"), tr("Wrapped residual °"));
    configure(m_radialPlot, tr("Restored Doppler versus local range/time — MH and NFM"),
              tr("Local radial speed, ingress clock (m/s)"), tr("Restored Doppler radial speed (m/s)"));
    configure(m_radialHistory, tr("Selected geometry chain"), tr("Ingress seconds from chain start"),
              tr("Signed radial speed (m/s)"));
    m_radialPlot->setObjectName(QStringLiteral("prp3RadialComparison"));
    m_radialHistory->setObjectName(QStringLiteral("prp3RadialHistory"));
    m_residualPlot->setAxisScale(QwtPlot::xBottom, 0, 12, 1);
    m_residualPlot->setAxisScale(QwtPlot::yLeft, -180, 180, 90);
    connect(m_timer, &QTimer::timeout, this, [this] { analyseBatch(); });
    connect(m_channel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { startAnalysis(); });
    connect(m_mapping, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { startAnalysis(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { selectPlot(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { openPlot(); });
    connect(open, &QPushButton::clicked, this, [this] { openPlot(); });
    connect(reference, &QPushButton::clicked, this, [this] {
        const auto index = selectedIndex();
        if (index < 0 || m_rows[index].fits.empty())
            return;
        m_references[m_rows[index].group] = index;
        present();
    });
    resize(1450, 900);
    startAnalysis();
}

void Prp3ComparisonWindow::startAnalysis()
{
    m_timer->stop();
    m_rows.clear();
    m_groups.clear();
    m_references.clear();
    m_table->setEnabled(false);
    m_table->setRowCount(0);
    m_summary->clear();
    m_detail->clear();
    for (auto *plot : { m_frequencyPlot, m_qualityPlot, m_residualPlot, m_radialPlot, m_radialHistory }) {
        plot->detachItems(QwtPlotItem::Rtti_PlotCurve);
        plot->detachItems(QwtPlotItem::Rtti_PlotMarker);
        plot->replot();
    }
    m_radialStatus->setText(tr("Calculating geometry-only local range/time estimates…"));
    m_timer->start(0);
}

void Prp3ComparisonWindow::analyseBatch()
{
    QElapsedTimer elapsed;
    elapsed.start();
    do {
        if (m_rows.size() == m_records.size()) {
            if (m_radialAnalysis.advance())
                continue;
            m_timer->stop();
            present();
            return;
        }
        const auto record = m_records[m_rows.size()];
        Prp3ComparisonRow row;
        row.record = record;
        row.ordinal = m_rows.size() + 1;
        const auto key = groupKey(*record, row.timing);
        const auto group = m_groups.find(key);
        row.group = group == m_groups.end() ? m_groups.insert(key, m_groups.size() + 1).value() : group.value();
        const auto *branch = diagnosticBranch(*record, m_channel->currentIndex());
        row.hasBranch = branch != nullptr;
        if (branch) {
            row.evidence = phaseEvidence(*branch, m_mapping->currentData().toInt());
            row.fits = fitPhases(row.evidence.pairs);
        }
        const auto best = referenceIndex(row.group);
        // Automatic references require PRI diversity and no competitive alias within the 1° allowance.
        if (competitiveCount(row) == 1 && intervalCount(row.evidence.pairs) >= 2
            && (best < 0 || row.evidence.pairs.size() > m_rows[best].evidence.pairs.size()
                || (row.evidence.pairs.size() == m_rows[best].evidence.pairs.size()
                    && row.fits.front().rmsDeg < m_rows[best].fits.front().rmsDeg)))
            m_references[row.group] = m_rows.size();
        m_rows.append(std::move(row));
    } while (elapsed.elapsed() < 12);
    m_status->setText(m_rows.size() == m_records.size()
        ? tr("Associating positions and ingress times for local radial speed…")
        : tr("Analysing %1/%2 visible PRP3 plots…").arg(m_rows.size()).arg(m_records.size()));
}

int Prp3ComparisonWindow::selectedIndex() const
{
    const auto *item = m_table->item(m_table->currentRow(), 0);
    return item && m_table->isEnabled() ? item->data(Qt::UserRole).toInt() : -1;
}

int Prp3ComparisonWindow::referenceIndex(int group) const
{
    return m_references.value(group, -1);
}

void Prp3ComparisonWindow::present()
{
    const auto selected = selectedIndex();
    const QSignalBlocker blocker(m_table);
    m_table->setEnabled(true);
    m_table->setSortingEnabled(false);
    const QStringList headings { tr("Plot #"), tr("Group"), tr("UTC"), tr("File / sequence"), tr("Status"),
        tr("Logged Hz"), tr("Δ group median Hz"), tr("Logged RMS °"), tr("Logged margin"),
        tr("Raw best Hz"), tr("Raw RMS °"), tr("Valid / signal / pairs"), tr("PRIs"), tr("Aliases ≤ best+1°"),
        tr("Reference plot #"), tr("Reference RMS °"), tr("Nearest alias ΔHz"), tr("Alias RMS excess °"),
        tr("Range raw / km"), tr("Azimuth °"), tr("Geometry chain"), tr("Doppler m/s"),
        tr("Local radial m/s"), tr("Doppler − local m/s"), tr("Local points"), tr("Local span s"),
        tr("Local conditional SE m/s") };
    m_table->setColumnCount(headings.size());
    m_table->setHorizontalHeaderLabels(headings);
    m_table->setRowCount(m_rows.size());
    QMap<int, std::vector<double>> frequencies;
    QMap<int, int> counts;
    QMap<int, QVector<QPointF>> points;
    for (const auto &row : qAsConst(m_rows)) {
        ++counts[row.group];
        const auto &restored = row.record->restoration;
        if (restored.valid && std::isfinite(restored.frequencyHz)) {
            frequencies[row.group].push_back(restored.frequencyHz);
            points[row.group].append({ double(row.ordinal), restored.frequencyHz });
        }
    }
    QMap<int, double> medians;
    auto summary = tr("<p>Snapshot of plots passing Analyzer's filters and visible layers when the task ran; "
                      "pan/zoom is not a selection filter. Run the task again to capture changed visibility. "
                      "Groups separate file, input channel, producer build, mode/signal/bin, logged branch mask, "
                      "source SIC, recorded PRI association and calibration profiles. "
                      "A group is not an identified track. "
                      "Filter time/area to isolate the target before interpreting frequency differences.</p>"
                      "<p>Statistics include valid finite logged frequencies only. Median deviation is descriptive: "
                      "it includes the plot itself and is not a fault threshold. Raw references are separate from "
                      "logged physical Hz; no calibration, sign reversal, MTD or preferred physical alias is applied. "
                      "Automatic raw reference: most supported pairs, then lowest RMS, among plots with ≥2 PRIs "
                      "and exactly one candidate within 1° of the best. "
                      "A manually selected reference may be ambiguous.</p>")
        + QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
        + htmlRow({ tr("Group"), tr("Plots / valid Hz"), tr("Median Hz"), tr("MAD Hz"), tr("Mean Hz"),
                    tr("Population SD Hz"), tr("Min Hz"), tr("Max Hz"), tr("Raw reference plot #") }, true);
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const auto values = frequencies.value(it.key());
        const auto centre = median(values);
        medians[it.key()] = centre;
        std::vector<double> deviations(values.size());
        std::transform(values.cbegin(), values.cend(), deviations.begin(), [centre](double value) {
            return std::abs(value - centre);
        });
        const auto mean = values.empty() ? UNAVAILABLE
            : std::accumulate(values.cbegin(), values.cend(), 0.0) / values.size();
        const auto variance = values.empty() ? UNAVAILABLE
            : std::accumulate(values.cbegin(), values.cend(), 0.0, [mean](double sum, double value) {
                return sum + (value - mean) * (value - mean);
            }) / values.size();
        const auto extrema = std::minmax_element(values.cbegin(), values.cend());
        const auto reference = referenceIndex(it.key());
        summary += htmlRow({ QString::number(it.key()), QStringLiteral("%1 / %2").arg(it.value()).arg(values.size()),
            number(centre), number(median(deviations)), number(mean), number(std::sqrt(variance)),
            values.empty() ? tr("—") : number(*extrema.first), values.empty() ? tr("—") : number(*extrema.second),
            reference < 0 ? tr("none; select a reference") : QString::number(m_rows[reference].ordinal) });
    }
    m_summary->setHtml(summary + QStringLiteral("</table>"));
    for (auto *plot : { m_frequencyPlot, m_qualityPlot })
        plot->detachItems(QwtPlotItem::Rtti_PlotCurve);
    for (auto it = points.cbegin(); it != points.cend(); ++it)
        curve(m_frequencyPlot, tr("Group %1").arg(it.key()), QColor::fromHsv((it.key() * 137) % 360, 190, 180),
              it.value());
    QVector<QPointF> bestRms, referenceRms;
    for (int index = 0; index < m_rows.size(); ++index) {
        const auto &row = m_rows[index];
        const auto &record = *row.record;
        const auto &restored = record.restoration;
        const auto reference = referenceIndex(row.group);
        const auto referenceHz = reference < 0 ? UNAVAILABLE : m_rows[reference].fits.front().frequencyHz;
        const auto predictedRms = phaseRms(row.evidence.pairs, referenceHz);
        const auto closest = std::min_element(row.fits.cbegin(), row.fits.cend(), [referenceHz](auto a, auto b) {
            return std::abs(a.frequencyHz - referenceHz) < std::abs(b.frequencyHz - referenceHz);
        });
        const auto rawHz = row.fits.empty() ? UNAVAILABLE : row.fits.front().frequencyHz;
        const auto rawRms = row.fits.empty() ? UNAVAILABLE : row.fits.front().rmsDeg;
        const auto logged = restored.valid ? restored.frequencyHz : UNAVAILABLE;
        const auto text = [this, index](int column, const QString &value) {
            m_table->setItem(index, column, new QTableWidgetItem(value));
        };
        const auto numeric = [this, index](int column, double value) {
            m_table->setItem(index, column,
                             new NumericItem(value, column <= 1 || (column >= 12 && column <= 14) ? 0 : 3));
        };
        numeric(0, row.ordinal);
        m_table->item(index, 0)->setData(Qt::UserRole, index);
        numeric(1, row.group);
        text(2, QDateTime::fromMSecsSinceEpoch(record.arrivalUtcMs, Qt::UTC).toString(Qt::ISODateWithMs));
        text(3, QStringLiteral("%1 / %2").arg(QFileInfo(record.filePath).fileName()).arg(record.sequence));
        text(4, tr("%1; %2%3").arg(restored.valid ? tr("valid") : tr("invalid"), restored.reasonName,
                                   record.filter.dropPlot ? tr("; filter dropped") : QString()));
        numeric(5, logged);
        numeric(6, logged - medians.value(row.group, UNAVAILABLE));
        numeric(7, restored.valid && record.richDiagnostics ? restored.fitResidualRad * 180.0 / PI : UNAVAILABLE);
        numeric(8, restored.valid && record.richDiagnostics ? restored.ambiguityMargin : UNAVAILABLE);
        numeric(9, rawHz);
        numeric(10, rawRms);
        numeric(11, row.evidence.pairs.size());
        m_table->item(index, 11)->setText(QStringLiteral("%1 / %2 / %3")
                     .arg(std::count(row.evidence.valid.cbegin(), row.evidence.valid.cend(), true))
                     .arg(std::count(row.evidence.supported.cbegin(), row.evidence.supported.cend(), true))
                     .arg(row.evidence.pairs.size()));
        numeric(12, intervalCount(row.evidence.pairs));
        numeric(13, competitiveCount(row));
        numeric(14, reference < 0 ? UNAVAILABLE : m_rows[reference].ordinal);
        numeric(15, predictedRms);
        numeric(16, closest == row.fits.cend() ? UNAVAILABLE : closest->frequencyHz - referenceHz);
        numeric(17, closest == row.fits.cend() || reference < 0 ? UNAVAILABLE : closest->rmsDeg - rawRms);
        text(18, record.legacy.valid ? QStringLiteral("%1 / %2").arg(record.legacy.rangeRaw)
                                          .arg(number(record.legacy.rangeKm)) : tr("—"));
        numeric(19, record.legacy.valid ? record.legacy.azimuthDegrees : UNAVAILABLE);
        const auto &local = m_radialAnalysis.estimates()[size_t(index)];
        const auto doppler = restored.valid ? restored.radialSpeedMps : UNAVAILABLE;
        numeric(20, local.chain < 0 ? UNAVAILABLE : local.chain + 1);
        numeric(21, doppler);
        numeric(22, local.radialMps);
        numeric(23, doppler - local.radialMps);
        numeric(24, local.chain < 0 ? UNAVAILABLE : local.samples);
        numeric(25, local.chain < 0 ? UNAVAILABLE : local.spanSeconds);
        numeric(26, local.standardErrorMps);
        for (int column = 0; column < headings.size(); ++column)
            m_table->item(index, column)->setToolTip(tr("%1\nSequence %2, byte %3\n%4 / %5 / %6; input channel %7\n"
                                                       "Logged PRI association: %8")
                .arg(record.filePath).arg(record.sequence).arg(record.envelopeOffset)
                .arg(record.snapshot.modeName, record.snapshot.signalName, record.snapshot.binName)
                .arg(record.inputChannel).arg(row.timing));
        if (std::isfinite(rawRms))
            bestRms.append({ double(row.ordinal), rawRms });
        if (std::isfinite(predictedRms))
            referenceRms.append({ double(row.ordinal), predictedRms });
    }
    curve(m_qualityPlot, tr("Best raw fit"), QColor(20, 80, 170), bestRms);
    curve(m_qualityPlot, tr("At group reference raw Hz"), QColor(205, 90, 20), referenceRms);
    const auto maximumRms = std::accumulate(referenceRms.cbegin(), referenceRms.cend(),
        std::accumulate(bestRms.cbegin(), bestRms.cend(), 1.0, [](double value, const auto &point) {
            return std::max(value, point.y());
        }), [](double value, const auto &point) { return std::max(value, point.y()); });
    m_qualityPlot->setAxisScale(QwtPlot::yLeft, 0.0, maximumRms * 1.1);
    for (auto *plot : { m_frequencyPlot, m_qualityPlot })
        plot->setAxisScale(QwtPlot::xBottom, 0.5, m_rows.size() + 0.5);
    m_table->resizeColumnsToContents();
    for (int column = 0; column < headings.size(); ++column)
        m_table->setColumnWidth(column, m_table->columnWidth(column) + 20);
    m_table->setColumnWidth(3, 240);
    m_table->setColumnWidth(4, 180);
    m_table->setSortingEnabled(true);
    const auto wanted = selected < 0 ? 0 : selected;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, 0)->data(Qt::UserRole).toInt() == wanted) {
            m_table->selectRow(row);
            break;
        }
    }
    m_status->setText(tr("%1 visible PRP3 plots • %2 comparison groups • %3 • %4\n"
                        "Select a row to highlight it. Raw Hz and logged Doppler have different reference conventions. "
                        "Sort by Δ median, RMS, alias count or support to investigate differences.")
                         .arg(m_rows.size()).arg(m_groups.size())
                         .arg(m_channel->currentText(), m_mapping->currentText()));
    presentRadial();
    selectPlot();
}

void Prp3ComparisonWindow::presentRadial()
{
    m_radialPlot->detachItems(QwtPlotItem::Rtti_PlotCurve);
    QVector<QPointF> mh, nfm;
    std::vector<double> errors;
    int localCount = 0;
    auto low = 0.0, high = 0.0;
    for (int index = 0; index < m_records.size(); ++index) {
        const auto &local = m_radialAnalysis.estimates()[size_t(index)];
        const auto &record = *m_records[index];
        if (!std::isfinite(local.radialMps))
            continue;
        ++localCount;
        const auto doppler = record.restoration.radialSpeedMps;
        if (!record.restoration.valid || !std::isfinite(doppler))
            continue;
        (record.snapshot.evidence.snapshot.signal == PsrDoppler::Signal::MH ? mh : nfm)
            .append({ local.radialMps, doppler });
        errors.push_back(doppler - local.radialMps);
        low = std::min({ low, local.radialMps, doppler });
        high = std::max({ high, local.radialMps, doppler });
    }
    const auto rmse = errors.empty() ? UNAVAILABLE
        : std::sqrt(std::inner_product(errors.cbegin(), errors.cend(), errors.cbegin(), 0.0) / errors.size());
    std::transform(errors.begin(), errors.end(), errors.begin(), [](double value) { return std::abs(value); });
    m_radialStatus->setText(tr("Ingress clock only: replay acceleration/batching can distort calculated speed; "
        "physical timing is unverified. Positive local speed means increasing range. "
        "Geometry-only chains are not tracker identities and never use Doppler to select plots. "
        "Each local fit uses up to 9 nearest-time points, including future points; SE is conditional, "
        "not association or clock uncertainty.\n"
        "%1 chains; local estimates %2/%3 plots; paired approved Doppler: MH %4, NFM %5. "
        "Paired median |error| %6 m/s; RMSE %7 m/s. "
        "No estimate means invalid/missing geometry, insufficient support or no qualifying chain.\n"
        "Eligibility: ≥8 plots, ≥35 ingress seconds, ≥60% scan coverage, ≤350 m/s planar speed; "
        "200 m radial / max(100 m, range × 0.35°) tangential gates. Files/inputs/beams and clock/scan resets "
        "are separate. Select a table row to inspect its chain. Phase channel/timing controls do not change this fit.")
        .arg(m_radialAnalysis.chains().size()).arg(localCount).arg(m_records.size()).arg(mh.size()).arg(nfm.size())
        .arg(number(median(errors)), number(rmse)));
    curve(m_radialPlot, tr("MH — approved Doppler"), QColor(180, 85, 10), mh);
    curve(m_radialPlot, tr("NFM — approved Doppler"), QColor(20, 80, 170), nfm);
    const auto margin = std::max(1.0, (high - low) * 0.05);
    auto *identity = new QwtPlotCurve(tr("Equal radial speed"));
    identity->setPen(QPen(Qt::darkGray, 1, Qt::DashLine));
    identity->setSamples(QVector<QPointF> { { low - margin, low - margin }, { high + margin, high + margin } });
    identity->attach(m_radialPlot);
    m_radialPlot->setAxisScale(QwtPlot::xBottom, low - margin, high + margin);
    m_radialPlot->setAxisScale(QwtPlot::yLeft, low - margin, high + margin);
    m_radialPlot->replot();
}

void Prp3ComparisonWindow::selectRadial(int index)
{
    m_radialPlot->detachItems(QwtPlotItem::Rtti_PlotMarker);
    m_radialHistory->detachItems(QwtPlotItem::Rtti_PlotCurve);
    m_radialHistory->detachItems(QwtPlotItem::Rtti_PlotMarker);
    const auto &local = m_radialAnalysis.estimates()[size_t(index)];
    const auto &restored = m_records[index]->restoration;
    if (std::isfinite(local.radialMps) && restored.valid && std::isfinite(restored.radialSpeedMps)) {
        auto *marker = new QwtPlotMarker;
        marker->setValue(local.radialMps, restored.radialSpeedMps);
        marker->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, QBrush(Qt::NoBrush), QPen(Qt::black, 2), QSize(13, 13)));
        marker->attach(m_radialPlot);
    }
    m_radialHistory->setTitle(local.chain < 0 ? tr("Selected plot has no qualifying geometry chain")
                                            : tr("Geometry chain %1 — ingress clock").arg(local.chain + 1));
    if (local.chain >= 0) {
        const auto &chain = m_radialAnalysis.chains()[size_t(local.chain)];
        const auto origin = m_records[chain.rows.front()]->arrivalUtcMs;
        QVector<QPointF> calculated, doppler;
        for (const auto row : chain.rows) {
            const auto &record = *m_records[row];
            const auto time = (record.arrivalUtcMs - origin) / 1000.0;
            calculated.append({ time, m_radialAnalysis.estimates()[size_t(row)].radialMps });
            if (record.restoration.valid && std::isfinite(record.restoration.radialSpeedMps))
                doppler.append({ time, record.restoration.radialSpeedMps });
        }
        curve(m_radialHistory, tr("Local range/time — includes rejected Doppler plots"),
              QColor(20, 80, 170), calculated);
        curve(m_radialHistory, tr("Approved restored Doppler"), QColor(180, 85, 10), doppler);
        auto *marker = new QwtPlotMarker;
        marker->setLineStyle(QwtPlotMarker::VLine);
        marker->setXValue((m_records[index]->arrivalUtcMs - origin) / 1000.0);
        marker->setLinePen(QPen(Qt::darkGray, 1, Qt::DashLine));
        marker->attach(m_radialHistory);
    }
    m_radialPlot->replot();
    m_radialHistory->replot();
}

void Prp3ComparisonWindow::selectPlot()
{
    const auto index = selectedIndex();
    if (index < 0)
        return;
    const auto &row = m_rows[index];
    selectRadial(index);
    const auto reference = referenceIndex(row.group);
    const auto referenceHz = reference < 0 ? UNAVAILABLE : m_rows[reference].fits.front().frequencyHz;
    auto detail = tr("<h3>Plot #%1 — group %2</h3><p>%3<br>Sequence %4, byte %5; input channel %6. "
                     "Logged timing: %7. Diagnostic timing: %8.</p>")
        .arg(row.ordinal).arg(row.group).arg(row.record->filePath.toHtmlEscaped()).arg(row.record->sequence)
        .arg(row.record->envelopeOffset).arg(row.record->inputChannel).arg(row.timing.toHtmlEscaped())
        .arg(m_mapping->currentText().toHtmlEscaped())
        + tr("<p>Raw reference plot: %1; raw frequency %2 Hz. %3 "
             "Its frequency predicts this plot without refitting. The reference row's own residual is an in-sample "
             "fit. Cross-plot disagreement may reflect different targets or changing radial velocity.</p>")
              .arg(reference < 0 ? tr("none") : QString::number(m_rows[reference].ordinal), number(referenceHz),
                   reference >= 0 && competitiveCount(m_rows[reference]) > 1
                       ? tr("Warning: selected reference has competing aliases.") : QString())
        + tr("<p>Signal mask uses valid finite samples with magnitude ≥ max(40, 0.2 × valid peak); both endpoints "
             "and a nonzero PRI are required. The mask is independent of frequency or phase agreement. "
             "Raw search ±20000 Hz, increasing arg(I+jQ) is positive. MH B is excluded.</p>")
        + tr("<p><b>Reading the evidence:</b> low best RMS but many competitive aliases or few PRIs suggests weak "
             "frequency discrimination. A nearby reference alias with little RMS excess supports an alias-jump "
             "hypothesis. High reference RMS with low best RMS also permits a real frequency change. High best RMS "
             "suggests phase inconsistency under this timing/model. Missing support identifies weak/invalid samples; "
             "none of these clues alone proves an erroneous restoration.</p>");
    if (row.record->snapshot.decoded) {
        const auto &snapshot = row.record->snapshot.evidence.snapshot;
        detail += tr("<p>Scan %1; azimuth cell %2; range bin %3; representative CPI %4. "
                     "These are logged identities, not track assignments.</p>")
                      .arg(snapshot.scanNumber).arg(snapshot.azimuthCellNumber).arg(snapshot.rangeBin)
                      .arg(snapshot.representativeCpi);
    }
    if (row.fits.empty())
        detail += tr("<p><b>No supported phase fit for the selected channel.</b> Check decoded snapshot, contributing "
                     "branch mask, validity, magnitude and PRI values in the per-plot diagnostics.</p>");
    detail += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
        + htmlRow({ tr("Rank"), tr("Raw Hz"), tr("RMS °"), tr("Δ reference Hz"), tr("RMS excess °") }, true);
    for (size_t rank = 0; rank < row.fits.size()
         && (rank < 8 || row.fits[rank].rmsDeg <= row.fits.front().rmsDeg + COMPETITIVE_RMS_DEG); ++rank) {
        const auto &fit = row.fits[rank];
        detail += htmlRow({ QString::number(rank + 1), number(fit.frequencyHz), number(fit.rmsDeg),
                            number(fit.frequencyHz - referenceHz), number(fit.rmsDeg - row.fits.front().rmsDeg) });
    }
    detail += QStringLiteral("</table><br><table border='1' cellspacing='0' cellpadding='4'>")
        + htmlRow({ tr("n→n+1"), tr("PRI µs"), tr("Measured °"), tr("Best residual °"),
                    tr("Reference residual °"), tr("Support") }, true);
    QVector<QPointF> best, predicted;
    const auto bestHz = row.fits.empty() ? UNAVAILABLE : row.fits.front().frequencyHz;
    for (const auto &pair : row.evidence.transitions) {
        if (!row.hasBranch)
            break;
        const auto supported = row.evidence.supported[pair.index] && row.evidence.supported[pair.index + 1]
            && pair.intervalUs;
        detail += htmlRow({ QStringLiteral("%1→%2").arg(pair.index).arg(pair.index + 1),
            QString::number(pair.intervalUs), pair.intervalUs ? number(pair.phaseDeg) : tr("—"),
            supported ? number(phaseResidual(pair, bestHz)) : tr("—"),
            supported ? number(phaseResidual(pair, referenceHz)) : tr("—"), supported ? tr("used") : tr("excluded") });
        if (supported && std::isfinite(bestHz))
            best.append({ pair.index + 0.5, phaseResidual(pair, bestHz) });
        if (supported && std::isfinite(referenceHz))
            predicted.append({ pair.index + 0.5, phaseResidual(pair, referenceHz) });
    }
    m_detail->setHtml(detail + QStringLiteral("</table>"));
    m_residualPlot->detachItems(QwtPlotItem::Rtti_PlotCurve);
    curve(m_residualPlot, tr("Best raw fit"), QColor(20, 80, 170), best);
    curve(m_residualPlot, tr("Group reference"), QColor(205, 90, 20), predicted);
    for (auto *plot : { m_frequencyPlot, m_qualityPlot }) {
        plot->detachItems(QwtPlotItem::Rtti_PlotMarker);
        auto *marker = new QwtPlotMarker;
        marker->setLineStyle(QwtPlotMarker::VLine);
        marker->setXValue(row.ordinal);
        marker->setLinePen(QPen(Qt::darkGray, 1, Qt::DashLine));
        marker->attach(plot);
        plot->replot();
    }
    m_residualPlot->replot();
}

void Prp3ComparisonWindow::openPlot()
{
    const auto index = selectedIndex();
    if (index < 0)
        return;
    auto *window = new Prp3SignalWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->setRecord(m_rows[index].record);
    window->show();
}

ComparePrp3Task::ComparePrp3Task(IAnalyser *host, QWidget *parent, std::function<Prp3Records()> records)
    : AnalyserTask(host), m_parent(parent), m_records(std::move(records))
{
}

bool ComparePrp3Task::execute(bool)
{
    auto records = m_records();
    if (records.isEmpty()) {
        QMessageBox::information(m_parent, getName(false), tr("No visible PRP3 plots. Load a PRP3 CBOR file and "
                                                             "check the plot filters and layer visibility."));
        return false;
    }
    if (m_window)
        m_window->close();
    m_window = new Prp3ComparisonWindow(std::move(records), m_parent);
    m_window->show();
    return true;
}

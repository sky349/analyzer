#include "prp3signalwindow.h"

#include "prp3record.h"
#include "prp3phasediagnostics.h"

#include <qwt_legend.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_rescaler.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

#include <QGridLayout>
#include <QDateTime>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <type_traits>
#include <iterator>
#include <set>
#include <tuple>
#include <vector>

namespace
{
using namespace Prp3PhaseDiagnostics;
const QColor BRANCH_COLORS[] = { QColor(21, 67, 151), QColor(190, 45, 45) };

class PhaseCurve : public QwtPlotCurve
{
public:
    using QwtPlotCurve::QwtPlotCurve;

protected:
    void drawLines(QPainter *painter, const QwtScaleMap &xMap, const QwtScaleMap &yMap,
                   const QRectF &canvasRect, int from, int to) const override
    {
        for (int start = from; start <= to;) {
            auto end = start;
            while (end < to && sample(end + 1).x() == sample(end).x() + 1.0)
                ++end;
            QwtPlotCurve::drawLines(painter, xMap, yMap, canvasRect, start, end);
            start = end + 1;
        }
    }
};

void configurePlot(QwtPlot &plot, const QString &title, const QString &xTitle, const QString &yTitle)
{
    plot.setTitle(title);
    plot.setCanvasBackground(Qt::white);
    plot.setAxisTitle(QwtPlot::xBottom, xTitle);
    plot.setAxisTitle(QwtPlot::yLeft, yTitle);
    plot.insertLegend(new QwtLegend(&plot), QwtPlot::BottomLegend);
    auto *grid = new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(195, 195, 195), 0, Qt::DashLine));
    grid->attach(&plot);
}

template<typename Curve = QwtPlotCurve>
QwtPlotCurve *makeCurve(QwtPlot &plot, const QString &title, const QColor &color)
{
    static_assert(std::is_base_of<QwtPlotCurve, Curve>::value, "curve must be a QwtPlotCurve");
    auto *curve = new Curve(title);
    curve->setPen(QPen(color, 2));
    curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, QBrush(color), QPen(color), QSize(6, 6)));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve->attach(&plot);
    return curve;
}

QString number(double value, int decimals = 3)
{
    return std::isfinite(value) ? QString::number(value, 'f', decimals) : QStringLiteral("—");
}

QString tableRow(const QStringList &cells, bool heading = false)
{
    const auto tag = heading ? QStringLiteral("th") : QStringLiteral("td");
    return std::accumulate(cells.cbegin(), cells.cend(), QStringLiteral("<tr>"),
                           [&tag](const QString &row, const auto &cell) {
        return row + QStringLiteral("<%1>%2</%1>").arg(tag, cell.toHtmlEscaped());
    }) + QStringLiteral("</tr>");
}

QString pairIndices(const std::vector<PhasePair> &pairs)
{
    QStringList indices;
    std::transform(pairs.cbegin(), pairs.cend(), std::back_inserter(indices), [](const auto &pair) {
        return QStringLiteral("%1→%2").arg(pair.index).arg(pair.index + 1);
    });
    return indices.isEmpty() ? QObject::tr("none") : indices.join(QStringLiteral(", "));
}

QString branchList(quint8 mask)
{
    return mask == 3 ? QStringLiteral("A/B") : mask == 1 ? QStringLiteral("A")
        : mask == 2 ? QStringLiteral("B") : QObject::tr("none");
}
}

Prp3SignalWindow::Prp3SignalWindow(QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_summary(new QLabel(this))
    , m_diagnosticBranch(new QComboBox(this))
    , m_priAssociation(new QComboBox(this))
    , m_manualFrequency(new QCheckBox(tr("Manual raw Hz:"), this))
    , m_rawFrequency(new QDoubleSpinBox(this))
    , m_diagnostics(new QTextBrowser(this))
    , m_predictionPlot(new QwtPlot(this))
    , m_residualPlot(new QwtPlot(this))
    , m_observedCurve(makeCurve<PhaseCurve>(*m_predictionPlot, tr("Measured"), BRANCH_COLORS[0]))
    , m_predictedCurve(makeCurve<PhaseCurve>(*m_predictionPlot, tr("Model"), QColor(0, 135, 80)))
    , m_residualCurve(makeCurve<PhaseCurve>(*m_residualPlot, tr("Supported"), BRANCH_COLORS[0]))
    , m_excludedResidualCurve(makeCurve(*m_residualPlot, tr("Excluded"), QColor(145, 145, 145)))
    , m_magnitudePlot(new QwtPlot(this))
    , m_constellationPlot(new QwtPlot(this))
    , m_phasePlot(new QwtPlot(this))
    , m_trajectoryRescaler(new QwtPlotRescaler(m_constellationPlot->canvas(), QwtPlot::xBottom,
                                             QwtPlotRescaler::Fitting))
{
    setWindowTitle(tr("PRP3 signal analysis"));
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::PlainText);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    configurePlot(*m_magnitudePlot, tr("Magnitude abs(z)"), tr("Sample index n"), tr("|I + jQ|"));
    configurePlot(*m_constellationPlot, tr("Complex-plane trajectory: 0 → 12"), tr("I"), tr("Q"));
    configurePlot(*m_phasePlot, tr("Phase increment arg(z[n+1] · conj(z[n]))"),
                  tr("Transition n → n+1 (at n+0.5)"), tr("Wrapped phase (rad)"));
    m_magnitudePlot->setAxisScale(QwtPlot::xBottom, 0, PsrDoppler::SAMPLE_COUNT - 1, 1);
    m_phasePlot->setAxisScale(QwtPlot::xBottom, 0, PsrDoppler::SAMPLE_COUNT - 1, 1);
    m_phasePlot->setAxisScale(QwtPlot::yLeft, -PI, PI, PI / 2.0);
    m_trajectoryRescaler->setExpandingDirection(QwtPlotRescaler::ExpandBoth);
    m_constellationPlot->plotLayout()->setAlignCanvasToScales(true);

    for (int branch = 0; branch < 2; ++branch) {
        const auto name = branch ? QStringLiteral("B") : QStringLiteral("A");
        m_magnitudeCurves[branch]
            = makeCurve(*m_magnitudePlot, tr("Channel %1").arg(name), BRANCH_COLORS[branch]);
        m_constellationCurves[branch]
            = makeCurve(*m_constellationPlot, tr("Channel %1").arg(name), BRANCH_COLORS[branch]);
        m_phaseCurves[branch]
            = makeCurve<PhaseCurve>(*m_phasePlot, tr("Channel %1").arg(name), BRANCH_COLORS[branch]);
        for (int endpoint = 0; endpoint < 2; ++endpoint) {
            auto *marker = m_endpointMarkers[branch][endpoint] = new QwtPlotMarker;
            QwtText label(name + QString::number(endpoint ? PsrDoppler::SAMPLE_COUNT - 1 : 0));
            label.setColor(BRANCH_COLORS[branch]);
            marker->setLabel(label);
            marker->setSpacing(6);
            marker->attach(m_constellationPlot);
        }
    }

    auto *tabs = new QTabWidget(this);
    auto *signalPage = new QWidget(tabs);
    auto *diagnostics = new QWidget(tabs);
    tabs->addTab(signalPage, tr("Signal samples"));
    tabs->addTab(diagnostics, tr("Phase diagnostics"));
    auto *outerLayout = new QGridLayout(this);
    outerLayout->addWidget(m_summary, 0, 0);
    outerLayout->addWidget(tabs, 1, 0);
    outerLayout->setRowStretch(1, 1);
    auto *layout = new QGridLayout(signalPage);
    layout->addWidget(m_constellationPlot, 1, 0, 2, 1);
    layout->addWidget(m_magnitudePlot, 1, 1);
    layout->addWidget(m_phasePlot, 2, 1);
    layout->setRowStretch(1, 1);
    layout->setRowStretch(2, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);
    m_priAssociation->addItem(tr("n→n+1: period[n] (original)"), 0);
    m_priAssociation->addItem(tr("n→n+1: period[n+1] (next sample)"), 1);
    m_rawFrequency->setRange(-RAW_FREQUENCY_LIMIT_HZ, RAW_FREQUENCY_LIMIT_HZ);
    m_rawFrequency->setDecimals(3);
    m_rawFrequency->setSingleStep(1.0);
    m_rawFrequency->setKeyboardTracking(false);
    m_diagnostics->setOpenLinks(false);
    m_diagnostics->setOpenExternalLinks(false);
    m_diagnostics->document()->setDefaultStyleSheet(QStringLiteral("th { background-color: #e8edf3; }"));
    auto *diagnosticLayout = new QGridLayout(diagnostics);
    diagnosticLayout->addWidget(new QLabel(tr("Channel:"), diagnostics), 0, 0);
    diagnosticLayout->addWidget(m_diagnosticBranch, 0, 1);
    diagnosticLayout->addWidget(m_priAssociation, 0, 2);
    diagnosticLayout->addWidget(m_manualFrequency, 0, 3);
    diagnosticLayout->addWidget(m_rawFrequency, 0, 4);
    auto *splitter = new QSplitter(Qt::Vertical, diagnostics);
    auto *plots = new QWidget(splitter);
    auto *plotLayout = new QGridLayout(plots);
    plotLayout->setContentsMargins(0, 0, 0, 0);
    plotLayout->addWidget(m_predictionPlot, 0, 0);
    plotLayout->addWidget(m_residualPlot, 0, 1);
    splitter->addWidget(plots);
    splitter->addWidget(m_diagnostics);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setSizes({ 250, 350 });
    diagnosticLayout->addWidget(splitter, 1, 0, 1, 5);
    diagnosticLayout->setRowStretch(1, 1);
    configurePlot(*m_predictionPlot, tr("Observed and model phase"), tr("Transition n→n+1 (at n+0.5)"), tr("Phase °"));
    configurePlot(*m_residualPlot, tr("Wrapped phase residual"), tr("Transition n→n+1 (at n+0.5)"), tr("Residual °"));
    for (auto *plot : { m_predictionPlot, m_residualPlot }) {
        plot->setAxisScale(QwtPlot::xBottom, 0, PsrDoppler::SAMPLE_COUNT - 1, 1);
        plot->setAxisScale(QwtPlot::yLeft, -180.0, 180.0, 90.0);
        for (int group = 1; group < CPI_COUNT; ++group) {
            auto *marker = new QwtPlotMarker;
            marker->setLineStyle(QwtPlotMarker::VLine);
            marker->setLinePen(QPen(QColor(160, 160, 160), 1, Qt::DashLine));
            marker->setXValue(group * CPI_TRANSITIONS);
            marker->attach(plot);
        }
    }
    m_excludedResidualCurve->setStyle(QwtPlotCurve::NoCurve);
    connect(m_diagnosticBranch, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { updateDiagnostics(); });
    connect(m_priAssociation, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { updateDiagnostics(); });
    connect(m_manualFrequency, &QCheckBox::toggled, this, [this] { updateDiagnostics(); });
    connect(m_rawFrequency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this] { updateDiagnostics(); });
    resize(1100, 800);
    clearRecord();
}

void Prp3SignalWindow::setRecord(const QSharedPointer<const Prp3PlotRecord> &record)
{
    m_record = record;
    if (m_record.isNull()) {
        clearRecord();
        return;
    }

    const auto &snapshotInfo = m_record->snapshot;
    const auto &snapshot = snapshotInfo.evidence.snapshot;
    const auto mh = snapshotInfo.decoded && snapshot.signal == PsrDoppler::Signal::MH;
    const auto recordedMask = m_record->restoration.branchMask;
    const auto selectedMask = (m_record->restoration.valid ? recordedMask : 3) & (mh ? 1 : 3);
    const QSignalBlocker branchBlocker(m_diagnosticBranch);
    const auto previousBranch = m_diagnosticBranch->currentData().toInt();
    m_diagnosticBranch->clear();
    quint8 displayedMask = 0;
    auto extent = 1.0;
    auto undefinedPhases = 0;
    for (int branchSlot = 0; branchSlot < 2; ++branchSlot) {
        const auto expectedBranch = static_cast<PsrDoppler::Branch>(branchSlot);
        const auto begin = snapshot.branches.begin();
        const auto end = begin + (snapshotInfo.decoded && (selectedMask & (1U << branchSlot))
                                     ? snapshot.branchCount : 0);
        const auto branch = std::find_if(begin, end, [expectedBranch](const PsrDoppler::BranchSnapshot &candidate) {
            return candidate.branch == expectedBranch;
        });
        QVector<QPointF> constellation, magnitudes, phaseDifferences;
        if (branch != end) {
            displayedMask |= quint8(1U << branchSlot);
            m_diagnosticBranch->addItem(branchSlot ? QStringLiteral("B") : QStringLiteral("A"), branchSlot);
            constellation.resize(PsrDoppler::SAMPLE_COUNT);
            magnitudes.resize(PsrDoppler::SAMPLE_COUNT);
            phaseDifferences.resize(PsrDoppler::SAMPLE_COUNT - 1);
            const auto &samples = branch->samples;
            std::transform(samples.cbegin(), samples.cend(), constellation.begin(), [](const auto &sample) {
                return QPointF(sample.i, sample.q);
            });
            std::transform(samples.cbegin(), samples.cend(), magnitudes.begin(), [&samples](const auto &sample) {
                return QPointF(&sample - samples.data(), std::hypot(double(sample.i), double(sample.q)));
            });
            std::transform(samples.cbegin(), samples.cend() - 1, samples.cbegin() + 1, phaseDifferences.begin(),
                           [&samples](const auto &sample, const auto &next) {
                const auto product = std::complex<double>(next.i, next.q)
                    * std::conj(std::complex<double>(sample.i, sample.q));
                return QPointF(&sample - samples.data() + 0.5,
                               std::abs(product) > 0.0 ? std::arg(product)
                                                       : std::numeric_limits<double>::quiet_NaN());
            });
            // Zero-magnitude pairs have no phase and zero estimator weight; do not draw invented zero increments.
            phaseDifferences.erase(std::remove_if(phaseDifferences.begin(), phaseDifferences.end(),
                                                  [](const auto &point) { return !std::isfinite(point.y()); }),
                                   phaseDifferences.end());
            undefinedPhases += PsrDoppler::SAMPLE_COUNT - 1 - phaseDifferences.size();
            extent = std::accumulate(constellation.cbegin(), constellation.cend(), extent,
                                     [](double limit, const auto &point) {
                return std::max({ limit, std::abs(point.x()), std::abs(point.y()) });
            });
            for (int endpoint = 0; endpoint < 2; ++endpoint) {
                const auto point = endpoint ? constellation.last() : constellation.first();
                auto *marker = m_endpointMarkers[branchSlot][endpoint];
                marker->setValue(point);
                marker->setLabelAlignment((point.x() >= 0.0 ? Qt::AlignLeft : Qt::AlignRight)
                                          | (endpoint ? Qt::AlignBottom : Qt::AlignTop));
            }
        }
        m_magnitudeCurves[branchSlot]->setSamples(magnitudes);
        m_constellationCurves[branchSlot]->setSamples(constellation);
        m_phaseCurves[branchSlot]->setSamples(phaseDifferences);
        for (auto *curve : { m_magnitudeCurves[branchSlot], m_constellationCurves[branchSlot],
                             m_phaseCurves[branchSlot] }) {
            curve->setVisible(branch != end);
            curve->setItemAttribute(QwtPlotItem::Legend, branch != end);
        }
        for (auto *marker : m_endpointMarkers[branchSlot])
            marker->setVisible(branch != end);
    }

    m_diagnosticBranch->setCurrentIndex(std::max(0, m_diagnosticBranch->findData(previousBranch)));
    updateDiagnostics();
    const auto doppler = m_record->restoration.valid
        ? tr("%1 Hz (%2 m/s nominal at 2.8 GHz)")
              .arg(m_record->restoration.frequencyHz, 0, 'f', 3)
              .arg(m_record->restoration.radialSpeedMps, 0, 'f', 3)
        : tr("invalid: %1").arg(m_record->restoration.reasonName);
    const auto snapshotText = snapshotInfo.decoded
        ? tr("%1, %2/%3, displayed channel %4; %5")
              .arg(snapshotInfo.modeName, snapshotInfo.signalName, snapshotInfo.binName, branchList(displayedMask),
                   snapshot.flags & PsrDoppler::FOLLOWING_INTERVAL_US_ASSUMED
                       ? tr("following-interval timing assumed")
                       : tr("timing semantics not marked assumed"))
        : tr("not decoded: %1").arg(snapshotInfo.decodeErrorName);
    const auto evidenceText = !displayedMask ? tr("No contributing I/Q samples available for display.")
        : !m_record->restoration.valid ? tr("Diagnostic samples only: no valid restored Doppler value.")
                                      : tr("Samples from the logged contributing channels; Doppler is not recomputed.");
    const auto mhText = !mh ? QString()
        : (recordedMask & 2) ? tr(" MH: B is noise and hidden. This record reports B participation; its Doppler "
                                 "must not be interpreted as an A-only result.")
                            : tr(" MH: channel A only; channel B is noise, including in Diversity mode.");
    m_summary->setText(
        tr("Sequence %1 | %2 UTC | input channel %3 | Doppler %4 | DPS1 %5\nProducer %6 | %7 @ byte %8 | "
           "CBOR/input/APOI %9/%10/%11 bytes\n%12%13\n"
           "13 samples per displayed channel; 12 adjacent phase increments. "
           "%14 undefined zero-magnitude increments omitted.")
            .arg(m_record->sequence)
            .arg(QDateTime::fromMSecsSinceEpoch(m_record->arrivalUtcMs, Qt::UTC).toString(Qt::ISODateWithMs))
            .arg(m_record->inputChannel)
            .arg(doppler, snapshotText, m_record->producerBuild, m_record->filePath)
            .arg(m_record->envelopeOffset)
            .arg(m_record->cborPayload.size())
            .arg(m_record->inputFrame.size())
            .arg(m_record->outputApoi.size())
            .arg(evidenceText, mhText)
            .arg(undefinedPhases));
    setWindowTitle(tr("PRP3 signal analysis — plot %1").arg(m_record->sequence));
    m_magnitudePlot->setAxisAutoScale(QwtPlot::yLeft);
    m_trajectoryRescaler->setIntervalHint(QwtPlot::xBottom, QwtInterval(-1.15 * extent, 1.15 * extent));
    m_trajectoryRescaler->setIntervalHint(QwtPlot::yLeft, QwtInterval(-1.15 * extent, 1.15 * extent));
    m_trajectoryRescaler->rescale();
    m_magnitudePlot->replot();
    m_constellationPlot->replot();
    m_phasePlot->replot();
}

void Prp3SignalWindow::updateDiagnostics()
{
    const auto branchId = m_diagnosticBranch->currentData().toInt();
    const auto *branch = [&]() -> const PsrDoppler::BranchSnapshot * {
        if (m_record.isNull() || !m_record->snapshot.decoded || m_diagnosticBranch->currentIndex() < 0)
            return nullptr;
        const auto &snapshot = m_record->snapshot.evidence.snapshot;
        const auto end = snapshot.branches.cbegin() + snapshot.branchCount;
        const auto it = std::find_if(snapshot.branches.cbegin(), end, [branchId](const auto &candidate) {
            return int(candidate.branch) == branchId;
        });
        return it == end ? nullptr : &*it;
    }();
    for (auto *curve : { m_residualCurve, m_excludedResidualCurve, m_observedCurve, m_predictedCurve })
        curve->setSamples(QVector<QPointF>());
    for (auto *control : std::array<QWidget *, 4>{ m_diagnosticBranch, m_priAssociation,
                                                  m_manualFrequency, m_rawFrequency })
        control->setEnabled(branch != nullptr);
    m_rawFrequency->setEnabled(branch && m_manualFrequency->isChecked());
    if (!branch) {
        m_diagnostics->setPlainText(tr("No admissible decoded channel available for per-plot diagnostics."));
        m_residualPlot->replot();
        m_predictionPlot->replot();
        return;
    }

    const auto offset = m_priAssociation->currentData().toInt();
    const auto evidence = phaseEvidence(*branch, offset);
    const auto ranked = fitPhases(evidence.pairs);
    const auto frequency = m_manualFrequency->isChecked() ? m_rawFrequency->value()
        : ranked.empty() ? std::numeric_limits<double>::quiet_NaN() : ranked.front().frequencyHz;
    const auto competitive = ranked.empty() ? 0 : std::count_if(ranked.cbegin(), ranked.cend(),
                                                               [&ranked](const auto &fit) {
        return fit.rmsDeg <= ranked.front().rmsDeg + COMPETITIVE_RMS_DEG;
    });
    const auto count = intervalCount(evidence.pairs);
    auto html = tr("<h3>Single-plot phase consistency — channel %1</h3>")
                    .arg(branchId ? QStringLiteral("B") : QStringLiteral("A"))
        + tr("<p>Raw-phase diagnostic search: −20000…+20000 Hz. Positive frequency means increasing "
             "arg(I+jQ); no polarity reversal, calibration or MTD input. The logged Doppler is unchanged. "
             "Ranking minimizes weighted wrapped pair RMS, not the restorer's coherent score.</p>")
        + tr("<p><b>Support:</b> %1/13 valid samples; %2/13 above amplitude gate %3 = max(40, 0.2 × valid peak). "
             "%4/12 usable transitions, %5 distinct PRIs. Both endpoints must be valid and pass the gate; "
             "a zero PRI cannot be fitted. No samples are removed for phase disagreement.</p>")
              .arg(std::count(evidence.valid.cbegin(), evidence.valid.cend(), true))
              .arg(std::count(evidence.supported.cbegin(), evidence.supported.cend(), true))
              .arg(number(evidence.threshold)).arg(evidence.pairs.size()).arg(count)
        + tr("<p><b>Model:</b> %1 raw Hz; supported-pair RMS %2°. %3 "
             "<b>Timing:</b> n→n+1 uses period[n+%4], directly from this channel's samples. "
             "Both associations are diagnostic hypotheses; the wire's following-interval flag is provisional.</p>")
              .arg(number(frequency), number(phaseRms(evidence.pairs, frequency)),
                   m_manualFrequency->isChecked() ? tr("Manual raw-frequency prediction.")
                                                  : tr("Best full-window fit."))
              .arg(offset)
        + tr("<p><b>Alias support:</b> %1 candidates within 1° RMS of the optimum. %2 "
             "The 1° allowance is a diagnostic convention, not a confidence interval. "
             "Equal-fitting aliases and adjacent-pair error correlation prevent a uniqueness claim.</p>")
              .arg(competitive).arg(count < 2 ? tr("Underdetermined: fewer than two supported PRI values.")
                                             : tr("Multiple PRIs do not guarantee an unambiguous physical frequency."));

    html += tr("<h3>All 12 transitions</h3><p>Grey residual points are excluded from fitting. "
               "Undefined phases remain gaps. Model phases are shown in green; all angles below are degrees.</p>")
        + QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
        + tableRow({ tr("n→n+1"), tr("|z[n]|"), tr("|z[n+1]|"), tr("PRI entry"), tr("PRI µs"),
                     tr("Measured °"), tr("Model °"), tr("Residual °"), tr("Support") }, true);
    QVector<QPointF> residuals, excludedResiduals, observed, predicted;
    for (const auto &pair : evidence.transitions) {
        const auto n = pair.index;
        const auto supported = evidence.supported[n] && evidence.supported[n + 1] && pair.intervalUs;
        const auto prediction = pair.intervalUs && std::isfinite(frequency)
            ? std::remainder(360.0 * frequency * pair.intervalUs / 1e6, 360.0)
            : std::numeric_limits<double>::quiet_NaN();
        const auto residual = std::isfinite(prediction) ? phaseResidual(pair, frequency)
                                                       : std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(pair.phaseDeg))
            observed.append(QPointF(n + 0.5, pair.phaseDeg));
        if (std::isfinite(prediction))
            predicted.append(QPointF(n + 0.5, prediction));
        if (std::isfinite(residual))
            (supported ? residuals : excludedResiduals).append(QPointF(n + 0.5, residual));
        html += tableRow({ QStringLiteral("%1→%2").arg(n).arg(n + 1), number(evidence.amplitudes[n], 1),
                          number(evidence.amplitudes[n + 1], 1), QString::number(n + offset),
                          QString::number(pair.intervalUs), number(pair.phaseDeg), number(prediction), number(residual),
                          !evidence.valid[n] || !evidence.valid[n + 1] ? tr("invalid endpoint")
                              : !std::isfinite(pair.phaseDeg) ? tr("zero endpoint")
                              : !evidence.supported[n] || !evidence.supported[n + 1] ? tr("weak endpoint")
                              : !pair.intervalUs ? tr("missing PRI") : tr("used") });
    }
    html += QStringLiteral("</table>")
        + tr("<h3>Full-window aliases</h3><p>Showing the best eight and every candidate within 1° of the best. "
             "Raw Hz are not physical Doppler. Enter a candidate in the manual model to inspect its residuals.</p>")
        + QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
        + tableRow({ tr("Rank"), tr("Raw Hz"), tr("RMS °"), tr("Within 1°") }, true);
    for (size_t i = 0; i < ranked.size()
         && (i < 8 || ranked[i].rmsDeg <= ranked.front().rmsDeg + COMPETITIVE_RMS_DEG); ++i)
        html += tableRow({ QString::number(i + 1), number(ranked[i].frequencyHz), number(ranked[i].rmsDeg),
                          ranked[i].rmsDeg <= ranked.front().rmsDeg + COMPETITIVE_RMS_DEG ? tr("yes") : tr("no") });
    html += QStringLiteral("</table>")
        + tr("<h3>Within-CPI phase dispersion</h3><p>Fixed hardware windows are samples 0–3, 3–6, 6–9 and 9–12; "
             "adjacent windows share an endpoint. The representative CPI does not move these windows. "
             "Dispersion about the weighted circular mean is meaningful only for at least two supported "
             "transitions with the same PRI under the selected mapping.</p>")
        + QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
        + tableRow({ tr("CPI / samples"), tr("Pairs"), tr("PRIs"), tr("Mean phase °"), tr("Dispersion °") }, true);
    for (int group = 0; group < CPI_COUNT; ++group) {
        std::vector<PhasePair> pairs;
        std::copy_if(evidence.pairs.cbegin(), evidence.pairs.cend(), std::back_inserter(pairs),
                     [group](const auto &pair) {
            return pair.index / CPI_TRANSITIONS == group;
        });
        const auto sum = std::accumulate(pairs.cbegin(), pairs.cend(), std::complex<double>{},
                                         [](auto value, const auto &pair) {
            return value + std::polar(pair.weight, pair.phaseDeg * PI / 180.0);
        });
        const auto weight = std::accumulate(pairs.cbegin(), pairs.cend(), 0.0, [](double value, const auto &pair) {
            return value + pair.weight;
        });
        const auto mean = std::abs(sum) > 1e-12 * weight ? std::arg(sum) * 180.0 / PI
                                                      : std::numeric_limits<double>::quiet_NaN();
        const auto spread = std::sqrt(std::accumulate(pairs.cbegin(), pairs.cend(), 0.0,
                                                     [mean](double value, const auto &pair) {
            const auto residual = std::remainder(pair.phaseDeg - mean, 360.0);
            return value + pair.weight * residual * residual;
        }) / (weight > 0.0 ? weight : 1.0));
        html += tableRow({ QStringLiteral("%1 / %2–%3").arg(group).arg(group * CPI_TRANSITIONS)
                              .arg((group + 1) * CPI_TRANSITIONS), QString::number(pairs.size()),
                          QString::number(intervalCount(pairs)), number(mean),
                          pairs.size() < 2 ? tr("insufficient pairs")
                              : intervalCount(pairs) != 1 ? tr("mixed PRI: not comparable") : number(spread) });
    }
    html += QStringLiteral("</table>")
        + tr("<h3>Strict CPI holdouts within this plot</h3><p>Each fit excludes all four samples of the tested CPI "
             "and every training transition touching them. It predicts supported transitions inside that CPI "
             "without refitting. All candidates within 1° of the training optimum are retained and ranked by "
             "training RMS only. A poor prediction from one alias alone is not evidence of a faulty CPI. "
             "These fits use the selected timing; the manual full-window model does not affect them.</p>");
    for (int group = 0; group < CPI_COUNT; ++group) {
        std::vector<PhasePair> training, testing;
        std::copy_if(evidence.pairs.cbegin(), evidence.pairs.cend(), std::back_inserter(training),
                     [group](const auto &pair) {
            return pair.index + 1 < group * CPI_TRANSITIONS || pair.index > (group + 1) * CPI_TRANSITIONS;
        });
        std::copy_if(evidence.pairs.cbegin(), evidence.pairs.cend(), std::back_inserter(testing),
                     [group](const auto &pair) { return pair.index / CPI_TRANSITIONS == group; });
        const auto fits = fitPhases(training);
        html += tr("<h4>CPI %1 — exclude samples %2–%3</h4><p>Train: %4.<br>Test: %5. %6</p>")
                    .arg(group).arg(group * CPI_TRANSITIONS).arg((group + 1) * CPI_TRANSITIONS)
                    .arg(pairIndices(training), pairIndices(testing),
                         testing.empty() ? tr("Untestable: no supported held-out transitions.")
                             : training.empty() ? tr("Unavailable: no training transitions.")
                             : intervalCount(training) < 2 ? tr("Underdetermined: only one training PRI.")
                             : tr("Interpret every competitive training alias."));
        if (fits.empty() || testing.empty())
            continue;
        html += QStringLiteral("<table border='1' cellspacing='0' cellpadding='4'>")
            + tableRow({ tr("Raw Hz"), tr("Training RMS °"), tr("Held-out RMS °") }, true);
        for (const auto &fit : fits) {
            if (fit.rmsDeg > fits.front().rmsDeg + COMPETITIVE_RMS_DEG)
                break;
            html += tableRow({ number(fit.frequencyHz), number(fit.rmsDeg),
                               number(phaseRms(testing, fit.frequencyHz)) });
        }
        html += QStringLiteral("</table>");
    }
    m_diagnostics->setHtml(html);
    m_residualCurve->setSamples(residuals);
    m_excludedResidualCurve->setSamples(excludedResiduals);
    m_observedCurve->setSamples(observed);
    m_predictedCurve->setSamples(predicted);
    m_residualPlot->replot();
    m_predictionPlot->replot();
}

void Prp3SignalWindow::clearRecord()
{
    m_record.clear();
    const QSignalBlocker branchBlocker(m_diagnosticBranch);
    m_diagnosticBranch->clear();
    updateDiagnostics();
    m_summary->setText(tr("Select a PRP3 plot to inspect its logged I/Q evidence."));
    const QVector<QPointF> emptyPoints;
    for (int branch = 0; branch < 2; ++branch) {
        for (auto *curve : { m_magnitudeCurves[branch], m_constellationCurves[branch], m_phaseCurves[branch] }) {
            curve->setSamples(emptyPoints);
            curve->setVisible(false);
            curve->setItemAttribute(QwtPlotItem::Legend, false);
        }
        for (auto *marker : m_endpointMarkers[branch])
            marker->setVisible(false);
    }
    m_magnitudePlot->replot();
    m_constellationPlot->replot();
    m_phasePlot->replot();
}

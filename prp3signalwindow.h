#pragma once

#include <QSharedPointer>
#include <QWidget>

class QLabel;
class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QTextBrowser;
class QwtPlot;
class QwtPlotCurve;
class QwtPlotMarker;
class QwtPlotRescaler;
struct Prp3PlotRecord;

class Prp3SignalWindow : public QWidget
{
public:
    explicit Prp3SignalWindow(QWidget *parent = nullptr);

    void setRecord(const QSharedPointer<const Prp3PlotRecord> &record);
    void clearRecord();

private:
    void updateDiagnostics();

    QLabel *m_summary = nullptr;
    QComboBox *m_diagnosticBranch = nullptr;
    QComboBox *m_priAssociation = nullptr;
    QCheckBox *m_manualFrequency = nullptr;
    QDoubleSpinBox *m_rawFrequency = nullptr;
    QTextBrowser *m_diagnostics = nullptr;
    QTextBrowser *m_archive = nullptr;
    QwtPlot *m_predictionPlot = nullptr;
    QwtPlot *m_residualPlot = nullptr;
    QwtPlotCurve *m_observedCurve = nullptr;
    QwtPlotCurve *m_predictedCurve = nullptr;
    QwtPlotCurve *m_residualCurve = nullptr;
    QwtPlotCurve *m_excludedResidualCurve = nullptr;
    QwtPlot *m_magnitudePlot = nullptr;
    QwtPlot *m_constellationPlot = nullptr;
    QwtPlot *m_phasePlot = nullptr;
    QwtPlotRescaler *m_trajectoryRescaler = nullptr;
    QwtPlotCurve *m_magnitudeCurves[2] = { nullptr, nullptr };
    QwtPlotCurve *m_constellationCurves[2] = { nullptr, nullptr };
    QwtPlotCurve *m_phaseCurves[2] = { nullptr, nullptr };
    QwtPlotMarker *m_endpointMarkers[2][2] = {};
    QSharedPointer<const Prp3PlotRecord> m_record;
};

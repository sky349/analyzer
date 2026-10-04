#pragma once

#include "ianalyser.h"
#include "prp3phasediagnostics.h"
#include "prp3record.h"
#include "prp3radialdiagnostics.h"

#include <QPointer>
#include <QWidget>
#include <functional>

class QComboBox;
class QLabel;
class QTableWidget;
class QTextBrowser;
class QTimer;
class QwtPlot;

using Prp3Records = QVector<QSharedPointer<const Prp3PlotRecord>>;

struct Prp3ComparisonRow
{
    QSharedPointer<const Prp3PlotRecord> record;
    int ordinal = 0;
    int group = 0;
    bool hasBranch = false;
    QString timing;
    Prp3PhaseDiagnostics::PhaseEvidence evidence;
    std::vector<Prp3PhaseDiagnostics::PhaseFit> fits;
};

class Prp3ComparisonWindow : public QWidget
{
public:
    explicit Prp3ComparisonWindow(Prp3Records records, QWidget *parent = nullptr);

private:
    void startAnalysis();
    void analyseBatch();
    void present();
    void selectPlot();
    void presentRadial();
    void selectRadial(int index);
    int selectedIndex() const;
    int referenceIndex(int group) const;
    void openPlot();

    Prp3Records m_records;
    Prp3RadialDiagnostics::Analysis m_radialAnalysis;
    QVector<Prp3ComparisonRow> m_rows;
    QMap<QStringList, int> m_groups;
    QMap<int, int> m_references;
    QComboBox *m_channel = nullptr;
    QComboBox *m_mapping = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_radialStatus = nullptr;
    QTableWidget *m_table = nullptr;
    QTextBrowser *m_summary = nullptr;
    QTextBrowser *m_detail = nullptr;
    QwtPlot *m_frequencyPlot = nullptr;
    QwtPlot *m_qualityPlot = nullptr;
    QwtPlot *m_residualPlot = nullptr;
    QwtPlot *m_radialPlot = nullptr;
    QwtPlot *m_radialHistory = nullptr;
    QTimer *m_timer = nullptr;
};

class ComparePrp3Task : public AnalyserTask
{
public:
    ComparePrp3Task(IAnalyser *analyser, QWidget *parent, std::function<Prp3Records()> records);
    TaskType getType() const override { return SingleStage; }
    QString getName(bool) const override { return tr("Compare PRP3 plots"); }
    bool execute(bool) override;

private:
    QWidget *m_parent = nullptr;
    std::function<Prp3Records()> m_records;
    QPointer<Prp3ComparisonWindow> m_window;
};

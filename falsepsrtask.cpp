
#include <radardata/nradarmarker.h>
#include <radardata/nradartrackplot.h>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_symbol.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

#include "falsepsrtask.h"

namespace
{

QVector<double> countsForTimePeriod(const QVector<double>& scanFalseTrackCounts,
                                    const QVector<QDateTime>& scanBeginTimes,
                                    const QDateTime& begin,
                                    const QDateTime& end,
                                    qint64 periodMs)
{
    const qint64 durationMs=begin.msecsTo(end);
    const int periodCount=qMax(1,int((durationMs+periodMs-1)/periodMs));
    QVector<double> counts(periodCount,0.0);

    const int scanCount=qMin(scanFalseTrackCounts.size(),scanBeginTimes.size());
    for(int scanIndex=0;scanIndex<scanCount;scanIndex++)
    {
        const qint64 elapsedMs=begin.msecsTo(scanBeginTimes.at(scanIndex));
        const int periodIndex=qBound(0,int(elapsedMs/periodMs),periodCount-1);
        counts[periodIndex]+=scanFalseTrackCounts.at(scanIndex);
    }
    return counts;
}

void updateFalseTracksChart(QwtPlot *plot,QwtPlotCurve *curve,
                            const QVector<double>& falseTrackCounts,
                            const QString& title,const QString& xAxisTitle)
{
    QVector<double> periods;
    periods.reserve(falseTrackCounts.size());
    for(int i=0;i<falseTrackCounts.size();i++)
        periods.append(i+1);

    plot->setTitle(title);
    plot->setAxisTitle(QwtPlot::xBottom,xAxisTitle);
    plot->setAxisTitle(QwtPlot::yLeft,QObject::tr("Number of false tracks"));

    int maximumCount=0;
    foreach(double value,falseTrackCounts)
        maximumCount=qMax(maximumCount,qRound(value));

    const int periodCount=periods.size();
    const int horizontalStep=qMax(1,qCeil((periodCount-1)/10.0));
    if(periodCount==1)
        plot->setAxisScale(QwtPlot::xBottom,0.0,2.0,1.0);
    else
        plot->setAxisScale(QwtPlot::xBottom,1.0,periodCount,horizontalStep);

    const int verticalStep=qMax(1,qCeil(maximumCount/10.0));
    const int verticalMaximum=maximumCount ?
            ((maximumCount/verticalStep)+1)*verticalStep : 1;
    plot->setAxisScale(QwtPlot::yLeft,0.0,verticalMaximum,verticalStep);

    curve->setSamples(periods.data(),falseTrackCounts.data(),periodCount);
    plot->replot();
}

void showFalseTracksChart(const QVector<double>& scanFalseTrackCounts,
                          const QVector<QDateTime>& scanBeginTimes,
                          const QDateTime& begin,const QDateTime& end,
                          const QString& baseTitle,const QString& titleSuffix)
{
    QWidget *window=new QWidget;
    window->setAttribute(Qt::WA_DeleteOnClose);

    QwtPlot *plot=new QwtPlot(window);
    plot->setCanvasBackground(Qt::white);

    QwtPlotGrid *grid=new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(190,190,190),0,Qt::DashLine));
    grid->attach(plot);

    QwtPlotCurve *curve=new QwtPlotCurve(QObject::tr("False tracks"));
    curve->setPen(QPen(QColor(35,100,190),2));
    curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
                                   QBrush(QColor(35,100,190)),
                                   QPen(QColor(35,100,190)),QSize(7,7)));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve->attach(plot);

    new QwtPlotMagnifier(plot->canvas());
    new QwtPlotPanner(plot->canvas());

    QComboBox *periodCombo=new QComboBox(window);
    periodCombo->setObjectName(QStringLiteral("falseTracksPeriodCombo"));
    periodCombo->addItem(QObject::tr("Scans"));
    periodCombo->addItem(QObject::tr("1 min"));
    periodCombo->addItem(QObject::tr("5 min"));

    QHBoxLayout *controlsLayout=new QHBoxLayout;
    controlsLayout->addStretch();
    controlsLayout->addWidget(periodCombo);

    QVBoxLayout *windowLayout=new QVBoxLayout(window);
    windowLayout->addWidget(plot);
    windowLayout->addLayout(controlsLayout);

    const auto refreshChart=[=](int periodIndex)
    {
        QVector<double> counts;
        QString periodTitle;
        QString xAxisTitle;
        if(periodIndex==0)
        {
            counts=scanFalseTrackCounts;
            periodTitle=QObject::tr(" per scan");
            xAxisTitle=QObject::tr("Scan number");
        }
        else
        {
            const int minutes=periodIndex==1 ? 1 : 5;
            counts=countsForTimePeriod(scanFalseTrackCounts,scanBeginTimes,
                                       begin,end,
                                       minutes*60*1000);
            periodTitle=QObject::tr(" per %1 min").arg(minutes);
            xAxisTitle=QObject::tr("%1-minute period").arg(minutes);
        }

        const QString title=baseTitle+periodTitle+titleSuffix;
        window->setWindowTitle(title);
        updateFalseTracksChart(plot,curve,counts,title,xAxisTitle);
    };

    QObject::connect(periodCombo,
                     static_cast<void (QComboBox::*)(int)>(
                         &QComboBox::currentIndexChanged),
                     refreshChart);

    refreshChart(0);
    window->resize(1000,600);
    window->show();
}

}

//IAnalyser holds all the data and provides QGraphicsScene for drawing
FalsePSRTask::FalsePSRTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    //nothing to do
}

//name for adding to the main menu
QString FalsePSRTask::getName(bool firstStage) const
{
    Q_UNUSED(firstStage);
    return tr("False tracks");
}

//run it!
bool FalsePSRTask::execute(bool firstStage)
{
    Q_UNUSED(firstStage);

    const IAnalyser::SourceSelection source=analyser->getSelectedSource();
    if(source==IAnalyser::SourceAny)
    {
        QMessageBox::critical(0,tr("Error"),
                tr("Both radars are selected. Choose just one for this task"));
        return false;
    }
    if(source!=IAnalyser::SourcePSR && source!=IAnalyser::SourceSSR)
    {
        QMessageBox::critical(0,tr("Error"),
                tr("Choose either PSR or SSR for this task"));
        return false;
    }

    bool ok;
    int maxLen = QInputDialog::getInt(0,tr("False tracks task"),
            tr("Maximum number of track points"),3,1,100,1,&ok);
    if(!ok) return false;

    int count=0;
    bool hasPreviousNorthMarker=false;
    QVector<double> falseTrackCounts;
    QVector<QDateTime> scanBeginTimes;
    QDateTime collectionBegin;
    QDateTime collectionEnd;
    QDateTime currentScanBegin;

    QMap<quint64,int> trackLen;
    const IAnalyser::ModeSSelection modeS=analyser->getSelectedModeS();

    //run through every plot
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        if(data->getType()==NRadarAbstractPlot::TypeMarker)
        {
            NRadarMarker* m = static_cast<NRadarMarker*>(data);
            if(m->getRadarId()==51 && m->isNorthMarker())
            {
                if(hasPreviousNorthMarker)
                {
                    falseTrackCounts.append(count);
                    scanBeginTimes.append(currentScanBegin);
                    collectionEnd=m->getTime();
                    currentScanBegin=m->getTime();
                }
                else
                {
                    hasPreviousNorthMarker=true;
                    collectionBegin=m->getTime();
                    currentScanBegin=m->getTime();
                }
                count = 0;
            }
            continue;
        }

        //we need only tracks
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        //get the Track object
        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);

        const quint64 trackKey=(quint64(tplot->getRadarId())<<32) |
                quint64(tplot->getTrackId());

        // Endpoints often do not preserve the source and Mode-S metadata of
        // the preceding track points. Always close the matching track state,
        // but count it only if at least one qualifying point was accumulated.
        if(tplot->getTrackPlotType()==NRadarTrackPlot::EndPoint)
        {
            QMap<quint64,int>::iterator track=trackLen.find(trackKey);
            if(track!=trackLen.end())
            {
                if(track.value()<=maxLen)
                    count++;
                trackLen.erase(track);
            }
            continue;
        }

        if(tplot->getTrackPlotType()!=NRadarTrackPlot::NormalPoint &&
                tplot->getTrackPlotType()!=NRadarTrackPlot::PredictedPoint)
            continue;

        const NRadarPlot::NPlotSourceType trackSource=tplot->getSource();
        if((source==IAnalyser::SourcePSR && trackSource!=NRadarPlot::PSR) ||
                (source==IAnalyser::SourceSSR &&
                 trackSource!=NRadarPlot::SSR && trackSource!=NRadarPlot::Combined))
            continue;

        if(source==IAnalyser::SourceSSR)
        {
            const bool isModeS=tplot->getSSRType()==NRadarPlot::ModeS;
            if((modeS==IAnalyser::ModeSDisabled && isModeS) ||
                    (modeS==IAnalyser::ModeSEnabled && !isModeS))
                continue;
        }

        //don't process in if it's no selected with current filters
        if(!analyser->isPlotVisible(data))
            continue;

        trackLen[trackKey]++;
    }

    if(falseTrackCounts.isEmpty())
    {
        QMessageBox::critical(0,tr("Error"),
                tr("At least two North markers are required to build the chart"));
        return false;
    }

    const QString baseTitle=(source==IAnalyser::SourcePSR ?
            tr("False PSR tracks (\u2264 %1 points)") :
            tr("False SSR tracks (\u2264 %1 points)")).arg(maxLen);
    QString titleSuffix;
    if(source==IAnalyser::SourceSSR && modeS!=IAnalyser::ModeSAny)
        titleSuffix=modeS==IAnalyser::ModeSEnabled ?
                    tr(" (Mode-S)") : tr(" (non-Mode-S)");

    showFalseTracksChart(falseTrackCounts,scanBeginTimes,
                         collectionBegin,collectionEnd,
                         baseTitle,titleSuffix);

    return true;
}

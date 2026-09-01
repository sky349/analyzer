
#include <libradardata/nradarmarker.h>
#include <libradardata/nradartrackplot.h>

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

#include <algorithm>

#include "falsepsrtask.h"

namespace
{

const qint64 TrackEndInactivityMs=30000;

struct TrackSummary
{
    TrackSummary():trackKey(0),pointCount(0),hasVisiblePoint(false),
        hasExplicitEnd(false) {}

    quint64 trackKey;
    int pointCount;
    bool hasVisiblePoint;
    bool hasExplicitEnd;
    QDateTime lastPointTime;
    QDateTime explicitEndTime;
};

int scanIndexForTime(const QVector<QDateTime>& northMarkerTimes,
                     const QDateTime& time)
{
    if(northMarkerTimes.size()<2 || time<northMarkerTimes.first() ||
            time>=northMarkerTimes.last())
        return -1;

    int first=0;
    int last=northMarkerTimes.size()-1;
    while(first+1<last)
    {
        const int middle=(first+last)/2;
        if(northMarkerTimes.at(middle)<=time)
            first=middle;
        else
            last=middle;
    }
    return first;
}

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

    QVector<QDateTime> northMarkerTimes;
    QMap<quint64,TrackSummary> tracks;
    const IAnalyser::ModeSSelection modeS=analyser->getSelectedModeS();
    const QDateTime selectedBegin=analyser->getSelectedBeginTime();
    const QDateTime selectedEnd=analyser->getSelectedEndTime();
    const bool showPredictedPoints=analyser->getShowPredictedTrackPoints();
    const quint8 northRadarId=source==IAnalyser::SourcePSR ? 51 : 50;
    QDateTime availableDataEnd;

    // Build the same continuous track instances used by the short-track
    // display filter, while retaining North markers for scan assignment.
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        if(!availableDataEnd.isValid() || availableDataEnd<data->getTime())
            availableDataEnd=data->getTime();

        if(data->getType()==NRadarAbstractPlot::TypeMarker)
        {
            NRadarMarker* m = static_cast<NRadarMarker*>(data);
            if(m->getRadarId()==northRadarId && m->isNorthMarker() &&
                    m->getTime()>=selectedBegin && m->getTime()<=selectedEnd)
                northMarkerTimes.append(m->getTime());
            continue;
        }

        //we need only tracks
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        //get the Track object
        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);
        const quint64 trackInstance=analyser->getTrackInstanceId(tplot);
        if(!trackInstance)
            continue;

        const quint64 trackKey=(quint64(tplot->getRadarId())<<32) |
                quint64(tplot->getTrackId());

        if(tplot->getTrackPlotType()==NRadarTrackPlot::EndPoint)
        {
            TrackSummary& track=tracks[trackInstance];
            track.trackKey=trackKey;
            track.hasExplicitEnd=true;
            track.explicitEndTime=tplot->getTime();
            continue;
        }

        if(tplot->getTrackPlotType()!=NRadarTrackPlot::NormalPoint &&
                tplot->getTrackPlotType()!=NRadarTrackPlot::PredictedPoint)
            continue;
        if(tplot->getTrackPlotType()==NRadarTrackPlot::PredictedPoint &&
                !showPredictedPoints)
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

        TrackSummary& track=tracks[trackInstance];
        track.trackKey=trackKey;
        track.pointCount++;
        if(!track.lastPointTime.isValid() || track.lastPointTime<tplot->getTime())
            track.lastPointTime=tplot->getTime();
        if(analyser->isPlotVisible(data))
            track.hasVisiblePoint=true;
    }

    std::sort(northMarkerTimes.begin(),northMarkerTimes.end());
    QVector<QDateTime> physicalNorthMarkerTimes;
    physicalNorthMarkerTimes.reserve(northMarkerTimes.size());
    foreach(const QDateTime& markerTime,northMarkerTimes)
    {
        // CAT034 can contain separate PSR/MSSR North reports for the same
        // antenna rotation. Most have identical timestamps, but recordings
        // also contain pairs a few milliseconds apart. A real rotation is
        // several seconds, so reports less than one second apart are one scan.
        if(physicalNorthMarkerTimes.isEmpty() ||
                physicalNorthMarkerTimes.last().msecsTo(markerTime)>=1000)
            physicalNorthMarkerTimes.append(markerTime);
    }
    northMarkerTimes.swap(physicalNorthMarkerTimes);

    if(northMarkerTimes.size()<2)
    {
        QMessageBox::critical(0,tr("Error"),
                tr("At least two North markers are required to build the chart"));
        return false;
    }

    QVector<double> falseTrackCounts(northMarkerTimes.size()-1,0.0);
    QVector<QDateTime> scanBeginTimes;
    scanBeginTimes.reserve(falseTrackCounts.size());
    for(int scanIndex=0;scanIndex<falseTrackCounts.size();scanIndex++)
        scanBeginTimes.append(northMarkerTimes.at(scanIndex));

    QMap<quint64,QDateTime> latestPointByTrackKey;
    QMapIterator<quint64,TrackSummary> latestTrack(tracks);
    while(latestTrack.hasNext())
    {
        latestTrack.next();
        const TrackSummary& value=latestTrack.value();
        if(value.pointCount &&
                (!latestPointByTrackKey.contains(value.trackKey) ||
                 latestPointByTrackKey.value(value.trackKey)<value.lastPointTime))
            latestPointByTrackKey[value.trackKey]=value.lastPointTime;
    }

    QMapIterator<quint64,TrackSummary> track(tracks);
    while(track.hasNext())
    {
        track.next();
        const TrackSummary& value=track.value();
        if(!value.pointCount || !value.hasVisiblePoint ||
                value.pointCount>maxLen)
            continue;

        // CAT048 recordings often omit explicit endpoint reports. Treat an
        // instance as ended when it has an endpoint, a later reuse of the
        // same radar/track number, or enough following data to establish
        // that it did not continue. This excludes tracks censored by the end
        // of the imported recording.
        const bool hasLaterInstance=
                value.lastPointTime<latestPointByTrackKey.value(value.trackKey);
        const bool inactiveBeforeDataEnd=value.lastPointTime.isValid() &&
                value.lastPointTime.msecsTo(availableDataEnd)>
                TrackEndInactivityMs;
        if(!value.hasExplicitEnd && !hasLaterInstance &&
                !inactiveBeforeDataEnd)
            continue;

        const QDateTime endTime=value.hasExplicitEnd ?
                    value.explicitEndTime : value.lastPointTime;
        const int scanIndex=scanIndexForTime(northMarkerTimes,endTime);
        if(scanIndex>=0)
            falseTrackCounts[scanIndex]++;
    }

    const QString baseTitle=(source==IAnalyser::SourcePSR ?
            tr("False PSR tracks (\u2264 %1 points)") :
            tr("False SSR tracks (\u2264 %1 points)")).arg(maxLen);
    QString titleSuffix;
    if(source==IAnalyser::SourceSSR && modeS!=IAnalyser::ModeSAny)
        titleSuffix=modeS==IAnalyser::ModeSEnabled ?
                    tr(" (Mode-S)") : tr(" (non-Mode-S)");

    showFalseTracksChart(falseTrackCounts,scanBeginTimes,
                         selectedBegin,selectedEnd,
                         baseTitle,titleSuffix);

    return true;
}

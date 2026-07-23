
#include <radardata/nradarmarker.h>
#include <radardata/nradartrackplot.h>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_panner.h>
#include <qwt_symbol.h>

#include "falsepsrtask.h"

namespace
{

void showFalseTracksChart(const QVector<double>& scanNumbers,
                          const QVector<double>& falseTrackCounts,
                          const QString& title)
{
    QwtPlot *plot=new QwtPlot;
    plot->setAttribute(Qt::WA_DeleteOnClose);
    plot->setWindowTitle(title);
    plot->setTitle(title);
    plot->setAxisTitle(QwtPlot::xBottom,QObject::tr("Scan number"));
    plot->setAxisTitle(QwtPlot::yLeft,QObject::tr("Number of false tracks"));
    plot->setCanvasBackground(Qt::white);

    int maximumCount=0;
    foreach(double value,falseTrackCounts)
        maximumCount=qMax(maximumCount,qRound(value));

    const int scanCount=scanNumbers.size();
    const int horizontalStep=qMax(1,qCeil((scanCount-1)/10.0));
    if(scanCount==1)
        plot->setAxisScale(QwtPlot::xBottom,0.0,2.0,1.0);
    else
        plot->setAxisScale(QwtPlot::xBottom,1.0,scanCount,horizontalStep);

    const int verticalStep=qMax(1,qCeil(maximumCount/10.0));
    const int verticalMaximum=maximumCount ?
            ((maximumCount/verticalStep)+1)*verticalStep : 1;
    plot->setAxisScale(QwtPlot::yLeft,0.0,verticalMaximum,verticalStep);

    QwtPlotGrid *grid=new QwtPlotGrid;
    grid->setMajorPen(QPen(QColor(190,190,190),0,Qt::DashLine));
    grid->attach(plot);

    QwtPlotCurve *curve=new QwtPlotCurve(QObject::tr("False tracks"));
    curve->setSamples(scanNumbers.data(),falseTrackCounts.data(),scanCount);
    curve->setPen(QPen(QColor(35,100,190),2));
    curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
                                   QBrush(QColor(35,100,190)),
                                   QPen(QColor(35,100,190)),QSize(7,7)));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased);
    curve->attach(plot);

    new QwtPlotMagnifier(plot->canvas());
    new QwtPlotPanner(plot->canvas());

    plot->resize(1000,600);
    plot->replot();
    plot->show();
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
    QVector<double> scanNumbers;
    QVector<double> falseTrackCounts;

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
                    scanNumbers.append(scanNumbers.size()+1);
                    falseTrackCounts.append(count);
                }
                else
                    hasPreviousNorthMarker=true;
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

    if(scanNumbers.isEmpty())
    {
        QMessageBox::critical(0,tr("Error"),
                tr("At least two North markers are required to build the chart"));
        return false;
    }

    QString title=source==IAnalyser::SourcePSR ?
            tr("False PSR tracks per scan") : tr("False SSR tracks per scan");
    if(source==IAnalyser::SourceSSR && modeS!=IAnalyser::ModeSAny)
        title+=modeS==IAnalyser::ModeSEnabled ? tr(" (Mode-S)") : tr(" (non-Mode-S)");

    showFalseTracksChart(scanNumbers,falseTrackCounts,title);

    return true;
}

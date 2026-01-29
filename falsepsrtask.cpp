
#include <radardata/nradarmarker.h>
#include <radardata/nradartrackplot.h>

#include "falsepsrtask.h"

//IAnalyser holds all the data and provides QGraphicsScene for drawing
FalsePSRTask::FalsePSRTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    //nothing to do
}

//name for adding to the main menu
QString FalsePSRTask::getName(bool firstStage) const
{
    return tr("False PSR tracks");
}

//run it!
bool FalsePSRTask::execute(bool firstStage)
{
    bool ok;
    int minLen = QInputDialog::getInt(0,tr("False PSR task"),tr("Min number of plots"),3,1,100,1,&ok);
    if(!ok) return false;

    int count=0;

    QTemporaryFile f(QDir::tempPath()+"/falsepsr-XXXXXX.csv");
    f.setAutoRemove(false);
    f.open();

    QTextStream out(&f);
    out << "Time" << "Count" << endl;

    QMap<uint,int> trackLen;

    //run through every plot
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        if(data->getType()==NRadarAbstractPlot::TypeMarker)
        {
            NRadarMarker* m = static_cast<NRadarMarker*>(data);
            if(m->getRadarId()==51 && m->isNorthMarker())
            {
                out << m->getTime().toString(Qt::ISODate)+";"+QString::number(count) << endl;
                count = 0;
            }
            continue;
        }

        //we need only tracks
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        //get the Track object
        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);

        uint tid = tplot->getTrackId();

        if(tplot->getSource()>NRadarPlot::PSR) continue;

        if(tplot->getTrackPlotType() == NRadarTrackPlot::EndPoint)
        {
            int len = trackLen[tid];
            if(len <= minLen)
                count++;

            trackLen.remove(tid);
            continue;
        }

        //don't process in if it's no selected with current filters
        if(!analyser->isPlotVisible(data))
            continue;

        if(tplot->getTrackPlotType() == NRadarTrackPlot::NormalPoint)
            trackLen[tid]++;
    }

    f.close();
    QDesktopServices::openUrl(QUrl(f.fileName()));

    return true;
}

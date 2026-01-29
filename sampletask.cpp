
//header for tracks class
#include <radardata/nradartrackplot.h>

#include "sampletask.h"

//IAnalyser holds all the data and provides QGraphicsScene for drawing
SampleTask::SampleTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    //nothing to do
}

//name for adding to the main menu
QString SampleTask::getName(bool firstStage) const
{
    return tr("The Sample Task");
}

//run it!
bool SampleTask::execute(bool firstStage)
{
    int count=0;

    //run through every plot
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        //don't process in if it's no selected with current filters
        if(!analyser->isPlotVisible(data))
            continue;

        //we need only tracks
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        //get the Track object
        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);

        //count all tracks within 1st quarter
        if(tplot->getADCoord().x()<90.0) count++;
    }

    //finally show the message box
    QMessageBox::information(0,"The Sample Task","Number of tracks within 1st quarter is: "+QString::number(count));
}

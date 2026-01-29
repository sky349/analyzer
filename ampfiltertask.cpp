
//header for tracks class
#include <radardata/nradartrackplot.h>
#include <radardata/nradarmarker.h>

#include "ampfiltertask.h"
#define NM_256 474100

//IAnalyser holds all the data and provides QGraphicsScene for drawing
AmpFilterTask::AmpFilterTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    //nothing to do
}

//name for adding to the main menu
QString AmpFilterTask::getName(bool firstStage) const
{
    return tr("Filter plots according to the amplitude map");
}

static int _mapToAmpl(int c,int w)
{
    return qMax(qMin(int(double(c+NM_256)/(2*NM_256)*w),w-1),0);
}

bool AmpFilterTask::execute(bool firstStage)
{
    QString fileName = QFileDialog::getOpenFileName(nullptr,
                                            tr("Open Amplitude Map Image"), "/nrpl/etc/rdps", tr("*.png"));

    QImage img;
    if(!img.load(fileName))
    {
        QMessageBox::critical(0, "Amplitude filter", "Error: unable to open file");
        return false;
    }
    int w=img.width();
    if(w!=img.height() || w%2==1)
    {
        QMessageBox::critical(0, "Amplitude filter", "Image for limits map must be the exact square with even side value");
        return false;
    }

    qDebug()<<"Using"<<w<<"x"<<w<<"image as amplitude limits map, scale ="<<scale;

    amplLimits = new quint16[w*w];
    amplMapSize = w;

    for(int y=0;y<w;y++)
        for(int x=0;x<w;x++)
            amplLimits[(w-y-1)*w+x] = qRound(2000*(1.0-QColor(img.pixel(x,y)).valueF())*scale);

    quint64 total = analyser->getAllData().size();

    analyser->setTaskProgress(0);
    lastWorkingUpdate.start();
    quint64 count = 0;

    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        count++;
        if(lastWorkingUpdate.elapsed()>1000)
        {
            analyser->setTaskProgress(count*100/total);
            lastWorkingUpdate.restart();
        }

        if(data->getType()!=NRadarAbstractPlot::TypePlot)
            continue;

        if(!analyser->isPlotVisible(data)) continue;

        NRadarPlot* plot=static_cast<NRadarPlot*>(data);

        if(plot->getOption(NRadarPlot::TestPlot).toBool())
        {
            analyser->setPlotColor(plot,Qt::white);
            continue;
        }

        if(plot->getSource()!=NRadarPlot::SSR) continue;

        //if(plot->getADCoord().y()>MAX_DISTANCE || plot->getADCoord().y()<11500) continue;

        QList<int> controls;
        controls<<7777<<5555;
        if(controls.contains(plot->getBoardNumber())) continue;

        int ampl=plot->getOption(NRadarPlot::RAW_SSRAmplitude).toInt();
        if(!ampl) ampl=20000;

        int x = _mapToAmpl(plot->getXYCoord().x(),amplMapSize);
        int y = _mapToAmpl(plot->getXYCoord().y(),amplMapSize);

        if (ampl < amplLimits[x+y*amplMapSize])
            analyser->setPlotColor(plot,Qt::white);

    }

    analyser->setTaskProgress(0,false);
    analyser->getRadarScene()->views().first()->update();

    emit finished(true);
    return true;
}

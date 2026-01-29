
//header for tracks class
#include <radardata/nradartrackplot.h>
#include <radardata/nradarmarker.h>

#include "duplicatetask.h"

//IAnalyser holds all the data and provides QGraphicsScene for drawing
DuplicateTask::DuplicateTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    //nothing to do
}

//name for adding to the main menu
QString DuplicateTask::getName(bool firstStage) const
{
    return tr("Find duplicate plots");
}

bool DuplicateTask::execute(bool firstStage)
{
    analyser->setTaskProgress(0);
    lastWorkingUpdate.start();

    QElapsedTimer timer;
    timer.start();

    quint64 total = analyser->getAllData().size();

    // reading all data to localTrack
    localTrack *tracks = new localTrack[total];

    quint64 totalVisible = 0;
    localTrack temp;
    QVariant icao;

    for (quint64 i=0; i<total; i++)
    {
        NRadarAbstractPlot* data = analyser->getAllData()[i];
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        if(!analyser->isPlotVisible(data))
            continue;

        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);

        if(tplot->getSource()!=NRadarPlot::SSR && tplot->getSource()!=NRadarPlot::Combined)
            continue;

        temp.time = tplot->getTime();
        temp.trackId = tplot->getTrackId();
        temp.isModeS = (tplot->getSSRType()==NRadarPlot::ModeS);
        temp.boardNumber = tplot->getBoardNumber();

        icao=tplot->getOption(NRadarPlot::AircraftAddress);
        temp.icaoAddr = icao.isNull()? 0 : icao.toUInt();
        temp.ADCoord = tplot->getADCoord();

        tracks[totalVisible] = temp;
        totalVisible++;
    }

    qDebug() << "moved all data to local," << totalVisible << "plots;" << timer.elapsed() << "ms";

    // ---

    quint64 foundDuplicate = 0;
    for (quint64 i=0; i<totalVisible; i++)
    {
        if(lastWorkingUpdate.elapsed()>1000)
        {
            analyser->setTaskProgress(i*100/totalVisible);
            lastWorkingUpdate.restart();
        }

        for (quint64 j=i+1; j<totalVisible; j++)
        {
            if (tracks[i].trackId == tracks[j].trackId)
                continue;

            if (tracks[i].isModeS && tracks[j].isModeS && (tracks[i].icaoAddr == tracks[j].icaoAddr))
            {
                if (tracks[i].time.msecsTo(tracks[j].time) >0.1 &&
                    tracks[i].time.msecsTo(tracks[j].time) < 4700)
                {
                    dLine* dline = new dLine(NRadarMap::convertADToXY(tracks[i].ADCoord),
                                             NRadarMap::convertADToXY(tracks[j].ADCoord), analyser->getRadarScene(), Qt::red);
                    foundDuplicate++;
                }
            }

            if (!tracks[i].isModeS && !tracks[j].isModeS && (tracks[i].boardNumber == tracks[j].boardNumber))
            {
                if (tracks[i].time.msecsTo(tracks[j].time) >0.1 &&
                    tracks[i].time.msecsTo(tracks[j].time) < 4700)
                {
                    dLine* dline = new dLine(NRadarMap::convertADToXY(tracks[i].ADCoord),
                                             NRadarMap::convertADToXY(tracks[j].ADCoord), analyser->getRadarScene(), Qt::blue);
                    foundDuplicate++;
                }
            }
        }
    }

    analyser->setTaskProgress(0,false);
    qDebug() << "done:" << timer.elapsed();
    QMessageBox::information(0, "Duplicate tracks",
        foundDuplicate==0?"No duplicate tracks found":"Found "+QString::number(foundDuplicate)+" tracks");

    delete[] tracks;
    emit finished(true);
    return true;
}

dLine::dLine(const QPointF &from, const QPointF &to, NRadarScene *scene, QColor color):NRadarObjectItem(Area)
{
    scene->addItem(this);

    m_line = new QGraphicsLineItem(from.x(), from.y(), to.x(), to.y());

    QPen pen(color,1);
    pen.setCosmetic(true);
    m_line->setPen(pen);
    m_line->setFlags(0);
    m_line->setZValue(1000-1);
    getScene()->addItem(m_line);
}

dLine::~dLine()
{
    delete m_line;
}

void dLine::paint(QPainter *painter, const NRadarItemOption *option)
{

}

QRect dLine::boundingRect() const
{
    return QRect(-10,-10,21,21);
}

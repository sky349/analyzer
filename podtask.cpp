
#include <QtGui>

#include <radardata/nradartrackplot.h>

#include "podtask.h"

#define POD_AZIMUTH_CELL 20
#define POD_DISTANCE_CELL 20 //miles

PoDTask::PoDTask(IAnalyser *analyser):AnalyserTask(analyser)
{
}

QString PoDTask::getName(bool) const
{
    return tr("Probability of detection matrix");
}

bool PoDTask::execute(bool)
{
    analyser->setTaskProgress(0);
    lastWorkingUpdate.start();

    int total_az_cells=360/POD_AZIMUTH_CELL+1;
    int total_dist_cells=280/POD_DISTANCE_CELL; //miles
    QVector<QPoint> cells(total_az_cells*total_dist_cells);

    qDebug()<<"PoD: step 1";

    QMap<uint,bool> trackHasSSR;

    qint64 count=0,passed=0;
    qint64 total=analyser->getAllData().size();
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
        count++;
        if(count%10000==0 && lastWorkingUpdate.elapsed()>1000)
        {
            analyser->setTaskProgress(count*100/total);
            lastWorkingUpdate.restart();
        }

        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        if(!analyser->isPlotVisible(data)) continue;

        NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);
        NRadarPlot::NPlotSourceType src=tplot->getSource();
        if(src==NRadarPlot::ADSB) continue;

        bool hasSSR=trackHasSSR.value(tplot->getTrackId(),false);

        if(tplot->getTrackPlotType()==NRadarTrackPlot::EndPoint)
        {
            trackHasSSR.remove(tplot->getTrackId());
            continue;
        }

        if(!hasSSR && tplot->getTrackPlotType()==NRadarTrackPlot::NormalPoint &&
                (src==NRadarPlot::SSR || src==NRadarPlot::Combined))
        {
            hasSSR=true;
            trackHasSSR[tplot->getTrackId()]=true;
        }

        if(!hasSSR) continue;

        if(!tplot->hasBoardNumber() || !tplot->hasHeight()) continue;

        QPointF ad=tplot->getADCoord();
        if(tplot->hasHeight() && (tplot->getHeight() > ad.y())) continue;

        if(ad.y()/1852.0 >= 280.0) continue;

        bool goodPoint=(tplot->getTrackPlotType()==NRadarTrackPlot::NormalPoint && tplot->getSource()!=NRadarPlot::PSR);

        int cell_az=int(ad.x())/POD_AZIMUTH_CELL;
        int cell_dist=int(ad.y()/1852.0)/POD_DISTANCE_CELL;

        QPoint& p=cells[cell_az*total_dist_cells+cell_dist];
        p.rx()++;
        if(goodPoint) p.ry()++;

        passed++;
    }
    qDebug()<<"PoD: step 2"<<passed;

    QImage img(total_az_cells*50,total_dist_cells*50,QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);

    QFont fnt=painter.font();
    fnt.setPointSize(8);
    painter.setFont(fnt);

    for(int y=0;y<total_dist_cells;y++)
        for(int x=0;x<total_az_cells;x++)
        {
            const QPoint& pt=cells[x*total_dist_cells+y];
            int pod=(pt.x()>0 ? pt.y()*100/pt.x() : -1);

            QColor cl=Qt::white;
            switch(100-pod)
            {
            case 101: cl=Qt::white; break;
            case 0: cl=Qt::blue; break;
            case 1: cl=Qt::cyan; break;
            case 2:
            case 3: cl=Qt::yellow; break;
            default: cl=Qt::red; break;
            }

            QRectF r(x*50,(total_dist_cells-y-1)*50,50,50);
            painter.setBrush(cl);
            painter.drawRect(r);
            if(pod>=0)
                painter.drawText(r.adjusted(2,2,-2,-2),
                                 Qt::AlignBottom|Qt::AlignRight,
                                QString("%1%\n%2").arg(pod).arg(pt.x()));
        }

    qDebug()<<"PoD: step 3";

    painter.end();

    QTemporaryFile file("pod_XXXXXX.png");
    file.setAutoRemove(false);
    bool ok=img.save(&file,"PNG");
    QDesktopServices::openUrl(file.fileName());

    analyser->setTaskProgress(0,false);

    emit finished(true);
    return true;
}

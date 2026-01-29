
//header for tracks class
#include <radardata/nradarplot.h>

#include "amplitudestask.h"
#include "amplitudedlg.h"

#define MAX_DISTANCE 474100

//IAnalyser holds all the data and provides QGraphicsScene for drawing
AmplitudesTask::AmplitudesTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    gradient.setStartValue(QColor(Qt::red));
    gradient.setKeyValueAt(0.1,QColor(255,128,0));
    gradient.setKeyValueAt(0.2,QColor(Qt::yellow));
    gradient.setKeyValueAt(0.3,QColor(Qt::green));
    gradient.setKeyValueAt(0.5,QColor(Qt::cyan));
    gradient.setEndValue(QColor(Qt::blue));
    gradient.setDuration(20000);

    increaseRatio=0.35;
    minLevel=0;
}

//name for adding to the main menu
QString AmplitudesTask::getName(bool firstStage) const
{
    return "Colorize SSR aplitudes";
}

//run it!
bool AmplitudesTask::execute(bool firstStage)
{
    AmplitudeDlg dlg(qRound(increaseRatio*100),minLevel);
    dlg.exec();
    increaseRatio = dlg.getIncreaseRatio()/100.0;
    minLevel=dlg.getMinLevel();

    int mapSize=256;
    quint64 sums[mapSize*mapSize];
    memset(sums,0,mapSize*mapSize*8);
    int counts[mapSize*mapSize];
    memset(counts,0,mapSize*mapSize*4);

    //run through every plot
    foreach(NRadarAbstractPlot* data,analyser->getAllData())
    {
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

        if(plot->getADCoord().y()>MAX_DISTANCE || plot->getADCoord().y()<11500) continue;

        QList<int> controls;
        controls<<7777<<5555;
        if(controls.contains(plot->getBoardNumber())) continue;

        int ampl=plot->getOption(NRadarPlot::RAW_SSRAmplitude).toInt();
        if(!ampl) ampl=20000;
        else if(increaseRatio>0.001)
            ampl += ampl*(plot->getADCoord().y()/100000.0)*increaseRatio;

        gradient.setCurrentTime(ampl);
        analyser->setPlotColor(plot,gradient.currentValue().value<QColor>());

        ampl = 4000 - qMin(ampl,4000);

        static_cast<NRadarItem*>(plot->getUserData())->setVisible(ampl>=minLevel);

        if(plot->getPlotAssociation()!=NRadarPlot::AssociatedWithTrack)
        {
            int x=(plot->getXYCoord().x()+MAX_DISTANCE)*mapSize/(2*MAX_DISTANCE);
            int y=(MAX_DISTANCE-plot->getXYCoord().y())*mapSize/(2*MAX_DISTANCE);

            sums[x+y*mapSize]+=ampl;
            counts[x+y*mapSize]++;
        }
    }
    analyser->getRadarScene()->views().first()->update();

    //paint map

    QImage img(mapSize,mapSize,QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter painter(&img);

    if(false)
    {
        painter.setPen(QPen(QColor(200,200,200)));
        for(int x=50;x<=450;x+=50)
        {
            int r=x*1000*mapSize/2/MAX_DISTANCE;
            painter.drawEllipse(QPoint(mapSize/2,mapSize/2),r,r);
        }
        painter.save();
        painter.translate(mapSize/2,mapSize/2);
        for(int y=0;y<=330;y+=30)
        {
            painter.rotate(30);
            painter.drawLine(0,0,MAX_DISTANCE,0);
        }
        painter.restore();
    }

    int maxSum=0;
    for(int y=0;y<mapSize;y++)
        for(int x=0;x<mapSize;x++)
        {
            int s=sums[x+y*mapSize],c=counts[x+y*mapSize];
            if(s>maxSum) maxSum=s;
        }
    if(!maxSum) maxSum=1;

    int R=5;
    for(int y=0;y<mapSize;y++)
        for(int x=0;x<mapSize;x++)
        {
            qint64 sum=0;
            int c=0;
            for(int y1=qMax(y-R,0);y1<=qMin(y+R,mapSize-1);y1++)
                for(int x1=qMax(x-R,0);x1<=qMin(x+R,mapSize-1);x1++)
                {
                    int rr=abs(x1-x)+abs(y1-y);
                    if(rr > R) continue;

                    //if(counts[x1+y1*mapSize]>1)
                        sum += sums[x1+y1*mapSize]*(1.0-(double)rr/(R+1));
                    c++;
                }

            int s = sum/c;

            if(c)
            {
                int v = 255 - qMin(s*255*50/maxSum,255);
                //v = 255 - qMin(s,4000)*255/4000;
                v = pow(v/255.0,2)*255.0;

                v = qMin(v, (int)img.pixel(x,y)&0xff);
                img.setPixel(x,y,qRgb(v,v,v));
            }
        }

    painter.end();

    QTemporaryFile file("ampl_XXXXXX.png");
    file.setAutoRemove(false);
    img.scaled(1024,1024,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).save(&file,"PNG");
    QDesktopServices::openUrl(file.fileName());
}

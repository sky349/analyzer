
#include <radardata/nradartrackplot.h>
#include <nradarmap.h>

#include "reflectorstask.h"

////////////////

ReflectorItem::ReflectorItem(const QPointF& pos,const QString& text,NRadarScene* scene):NRadarObjectItem(NRadarItem::Point)
{
    path.addEllipse(QPointF(0,0),9,9);

    setFlag(QGraphicsItem::ItemIsSelectable,true);
    setFlag(QGraphicsItem::ItemIsMovable,false);
    setZValue(1000);

    markPen=QPen(Qt::blue,2);
    markPen.setCosmetic(true);

    setPos(pos);
    scene->addItem(this);

    label=new QGraphicsTextItem();
    label->setHtml("<font color=\"white\"><b>"+text+"</b><br><a href=\"link\"><font color=\"white\">remove me!</font></a></font>");
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QObject::connect(label,SIGNAL(linkActivated(const QString&)),this,SIGNAL(removeMe()));
    label->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
    label->setPos(pos);
    label->setZValue(1000);
    getScene()->addItem(label);
    label->setVisible(false);
}

void ReflectorItem::addHintLines(const QPointF bug,const QPointF good)
{
    lines<<new QGraphicsLineItem(0,0,good.x(),good.y());
    lines<<new QGraphicsLineItem(good.x(),good.y(),pos().x(),pos().y());
    lines<<new QGraphicsLineItem(0,0,bug.x(),bug.y());

    QPen pen(Qt::green,1);
    pen.setCosmetic(true);
    lines[0]->setPen(pen);
    pen.setColor(Qt::red);
    lines[1]->setPen(pen);
    lines[2]->setPen(pen);

    foreach(QGraphicsLineItem *l,lines)
    {
        l->setFlags(0);
        l->setZValue(1000-1);
        getScene()->addItem(l);
    }
}

ReflectorItem::~ReflectorItem()
{
    qDeleteAll(lines);
    delete label;
}

QRect ReflectorItem::boundingRect() const
{
    return QRect(-10,-10,21,21);
}

void ReflectorItem::paint(QPainter *painter,const NRadarItemOption *option)
{
    bool aa=painter->renderHints()&QPainter::Antialiasing;
    painter->setRenderHint(QPainter::Antialiasing,true);
    painter->setPen(markPen);
    painter->drawPath(path);
    painter->setRenderHint(QPainter::Antialiasing,aa);
}

void ReflectorItem::selectionEvent(bool on)
{
    markPen=QPen(on?Qt::red:Qt::blue, on?3:2);
    markPen.setCosmetic(true);

    foreach(QGraphicsLineItem *l,lines)
    {
        QPen p=l->pen();
        p.setWidth(on?2:1);
        l->setPen(p);
    }

    if(on) label->setVisible(on);
    else QTimer::singleShot(100,this,SLOT(hideLabel()));

    NRadarItem::selectionEvent(on);
}

void ReflectorItem::hideLabel()
{
    label->setVisible(false);
    //TODO: почему не перерисовывается??

    scene->update(this->sceneRect());
}

////////////////

ReflectorsTask::ReflectorsTask(IAnalyser *analyser):AnalyserTask(analyser)
{
    azim_err = 2.0;
    rev_time = 4.7;
}

QString ReflectorsTask::getName(bool on) const
{
    return on ? tr("Find reflectors") : tr("Save reflectors and clear");
}

void ReflectorsTask::clear()
{
    foreach(Reflector r,reflectors)
        delete r.mark;

    reflectors.clear();
    tracks.clear();
}

bool ReflectorsTask::execute(bool on)
{
    if(!on)
    {
        QTemporaryFile file("reflectors_XXXXXX.txt");
        file.setAutoRemove(false);

        file.open();
        foreach(Reflector r,reflectors)
            file.write(QString("%1 %2\r\n").arg(r.pos.x(),0,'f',6).arg(qRound(r.pos.y())).toLatin1());
        file.close();

        clear();

        QDesktopServices::openUrl(file.fileName());
        return true;
    }

    analyser->setTaskProgress(0);
    lastWorkingUpdate.start();

    qint64 count=0;
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

        if(tplot->getSource()!=NRadarPlot::SSR && tplot->getSource()!=NRadarPlot::Combined)
            continue;

        uint tid=tplot->getTrackId();
        switch(tplot->getTrackPlotType())
        {
        case NRadarTrackPlot::PredictedPoint: continue;

        case NRadarTrackPlot::EndPoint:
            tracks.remove(tid);
            continue;

        case NRadarTrackPlot::NormalPoint:
            tracks[tid].history<<tplot;
            break;
        }

        if(hasReflector(tplot->getADCoord().x())>=0)
            continue;

        findReflector(tplot);
    }

    analyser->setTaskProgress(0,false);

    qDebug()<<"Reflectors: end";

    emit finished(true);
    return true;
}

int ReflectorsTask::hasReflector(double azimuth)
{
    for(int i=0;i<reflectors.size();i++)
    {
        double AzimDelta = fabs(azimuth - reflectors[i].pos.x());
        if(AzimDelta <= azim_err || AzimDelta >= (360.0 - azim_err))
            return i;
    }
    return -1;
}

void ReflectorsTask::findReflector(const NRadarTrackPlot *tplot)
{
    if(!tplot->hasBoardNumber() || !tplot->hasHeight() ||
            tplot->testPredictionFlag(NRadarTrackPlot::PredictedBoardNumber) ||
            tplot->testPredictionFlag(NRadarTrackPlot::PredictedHeight)) return;

    const Track& track=tracks[tplot->getTrackId()];
    if(!track.history.size()) return;

    QMapIterator<int,Track> it(tracks);
    const NRadarTrackPlot* foundPlot=0;
    while(it.hasNext())
    {
        it.next();

        if(it.key()==tplot->getTrackId()) continue;

        const Track& other=it.value();

        int hs=track.history.size(),ohs=other.history.size();
        if(!ohs || hs<3 || ohs<3 || hs>=ohs)
            continue;

        const NRadarTrackPlot* oPlot=other.history.last();

        if(tplot->getBoardNumber() != oPlot->getBoardNumber() ||
                !oPlot->hasHeight())
            continue;

        int msecs=oPlot->getTime().msecsTo(tplot->getTime());
        if(msecs>2*rev_time*1000) continue;

        if(foundPlot) return;
        foundPlot=oPlot;
    }

    if(!foundPlot) return;

    QPointF ad=predict(foundPlot,foundPlot->getTime().msecsTo(tplot->getTime()));

    if((tplot->getADCoord().y() - ad.y() > 70) &&
            (tplot->getHeight() <= foundPlot->getHeight()+50) &&
            (tplot->getHeight() >= foundPlot->getHeight()-50))
    {
        addReflector(tplot->getADCoord(),tplot->getHeight(),ad,tplot->getBoardNumber());
        qDebug()<<"... this"<<tplot->getBoardNumber()<<tplot->getADCoord()<<", other"<<foundPlot->getADCoord();
    }
}

QPointF ReflectorsTask::predict(const NRadarTrackPlot *tplot, int msecs)
{
    QPointF move(tplot->getHeading(),tplot->getSpeed()/3.6*msecs/1000);
    QPointF xy = tplot->getXYCoord() + NRadarMap::convertADToXY(move);
    return NRadarMap::convertXYToAD(xy);
}

void ReflectorsTask::addReflector(const QPointF& pos,int heigth,const QPointF& other,int boardNumber)
{
    Reflector ref;

    ref.pos.setX(pos.x());

    double rangeSqued = pow(pos.y(),2);
    double otherRangeSqued = pow(other.y(),2);
    double heightSqued = pow(heigth,2);
    double alfa=(other.x()-pos.x())*M_PI/180.0;

    ref.pos.setY(qRound((rangeSqued-otherRangeSqued) / (2*(pos.y() - sqrt(otherRangeSqued-heightSqued)*cos(alfa)))));

    ReflectorItem* ri=new ReflectorItem(NRadarMap::convertADToXY(ref.pos),QString::number(boardNumber),analyser->getRadarScene());
    ri->addHintLines(NRadarMap::convertADToXY(pos),NRadarMap::convertADToXY(other));
    connect(ri,SIGNAL(removeMe()),this,SLOT(removeReflector()));

    ref.mark=ri;
    reflectors<<ref;
    qDebug()<<"New reflector added:"<<ref.pos;
}

void ReflectorsTask::removeReflector()
{
    if(!sender()) return;

    int c=0;
    foreach(Reflector ref,reflectors)
        if(ref.mark==sender())
        {
            qDebug()<<"Reflector removed:"<<ref.pos;
            ref.mark->deleteLater();
            reflectors.remove(c);
            break;
        }
        else c++;
}


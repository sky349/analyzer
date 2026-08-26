#include <radardata/nradarplot.h>
#include <radarview/nradaritem.h>
#include <radarview/nradarscenelayer.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGraphicsObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QSet>

#include <algorithm>

#include "closepsrplotstask.h"

namespace
{

const double DefaultRangeGateMeters=200.0;
const double DefaultAzimuthGateDegrees=2.0;
const qint64 MaximumTimeDifferenceMs=5000;

struct PSRPlotPoint
{
    NRadarPlot *plot;
    qint64 timestampMs;
    bool hasValidTime;
    double azimuthDegrees;
    double rangeMeters;
};

struct MatchCandidate
{
    int firstPoint;
    int secondPoint;
    double cost;
};

class ClosePSRPlotsOverlay:public QGraphicsObject
{
public:
    explicit ClosePSRPlotsOverlay(const QVector<QLineF>& lines):
        matchLines(lines)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        setFlag(QGraphicsItem::ItemIsSelectable,false);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption,true);
        setZValue(1200.0);

        foreach(const QLineF& line,matchLines)
        {
            extendBounds(line.p1());
            extendBounds(line.p2());
        }
        bounds.adjust(-1000.0,-1000.0,1000.0,1000.0);
    }

    QRectF boundingRect() const
    {
        return bounds;
    }

    void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,
               QWidget *)
    {
        if(matchLines.isEmpty())
            return;

        QRectF exposed=option ? option->exposedRect :
                                painter->clipBoundingRect();
        if(exposed.isEmpty())
            exposed=painter->clipBoundingRect();

        QVector<QLineF> visibleLines;
        visibleLines.reserve(matchLines.size());
        foreach(const QLineF& line,matchLines)
        {
            QRectF lineBounds(line.p1(),line.p2());
            lineBounds=lineBounds.normalized();
            lineBounds.adjust(-1.0,-1.0,1.0,1.0);
            if(exposed.intersects(lineBounds))
                visibleLines.append(line);
        }

        if(visibleLines.isEmpty())
            return;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing,true);
        QPen pen(QColor(0,255,80),1.5);
        pen.setCosmetic(true);
        painter->setPen(pen);
        painter->drawLines(visibleLines);
        painter->restore();
    }

private:
    void extendBounds(const QPointF& point)
    {
        const QRectF pointBounds(point,QSizeF(1.0,1.0));
        if(bounds.isNull())
            bounds=pointBounds;
        else
            bounds|=pointBounds;
    }

    QVector<QLineF> matchLines;
    QRectF bounds;
};

bool selectGates(double& rangeGateMeters,double& azimuthGateDegrees)
{
    QDialog dialog;
    dialog.setWindowTitle(QObject::tr("Find close PSR plots"));

    QFormLayout layout(&dialog);

    QDoubleSpinBox azimuthGate;
    azimuthGate.setDecimals(2);
    azimuthGate.setRange(0.01,180.0);
    azimuthGate.setSingleStep(0.1);
    azimuthGate.setSuffix(QObject::tr(" deg"));
    azimuthGate.setValue(azimuthGateDegrees);
    layout.addRow(QObject::tr("Maximum azimuth difference:"),&azimuthGate);

    QDoubleSpinBox rangeGate;
    rangeGate.setDecimals(1);
    rangeGate.setRange(1.0,1000000.0);
    rangeGate.setSingleStep(50.0);
    rangeGate.setSuffix(QObject::tr(" m"));
    rangeGate.setValue(rangeGateMeters);
    layout.addRow(QObject::tr("Maximum range difference:"),&rangeGate);

    QLabel timeGate(QObject::tr("5 s (same antenna scan)"));
    layout.addRow(QObject::tr("Maximum time difference:"),&timeGate);

    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QObject::connect(&buttons,&QDialogButtonBox::accepted,
                     &dialog,&QDialog::accept);
    QObject::connect(&buttons,&QDialogButtonBox::rejected,
                     &dialog,&QDialog::reject);
    layout.addRow(&buttons);
    dialog.setMinimumWidth(380);

    if(dialog.exec()!=QDialog::Accepted)
        return false;

    azimuthGateDegrees=azimuthGate.value();
    rangeGateMeters=rangeGate.value();
    return true;
}

double normalizedAzimuth(double azimuth)
{
    while(azimuth<0.0)
        azimuth+=360.0;
    while(azimuth>=360.0)
        azimuth-=360.0;
    return azimuth;
}

double azimuthDifference(double first,double second)
{
    const double difference=qAbs(normalizedAzimuth(first)-
                                 normalizedAzimuth(second));
    return qMin(difference,360.0-difference);
}

bool isClose(const PSRPlotPoint& first,const PSRPlotPoint& second,
             double maximumAzimuthDifferenceDegrees,
             double maximumRangeDifferenceMeters)
{
    if(!first.hasValidTime || !second.hasValidTime ||
            qAbs(first.timestampMs-second.timestampMs)>
            MaximumTimeDifferenceMs)
        return false;

    return azimuthDifference(first.azimuthDegrees,second.azimuthDegrees)<=
            maximumAzimuthDifferenceDegrees &&
            qAbs(first.rangeMeters-second.rangeMeters)<=
            maximumRangeDifferenceMeters;
}

NRadarItem *radarItem(const NRadarAbstractPlot *plot)
{
    return static_cast<NRadarItem*>(plot->getUserData());
}

bool isCurrentlyVisible(const NRadarAbstractPlot *plot)
{
    NRadarItem *item=radarItem(plot);
    if(!item || !item->isVisible())
        return false;

    NRadarSceneLayer *layer=item->getLayer();
    return !layer || layer->isVisible();
}

}

ClosePSRPlotsTask::ClosePSRPlotsTask(IAnalyser *analyser):
    AnalyserTask(analyser),
    rangeGateMeters(DefaultRangeGateMeters),
    azimuthGateDegrees(DefaultAzimuthGateDegrees)
{
}

ClosePSRPlotsTask::~ClosePSRPlotsTask()
{
    delete overlay.data();
}

QString ClosePSRPlotsTask::getName(bool firstStage) const
{
    Q_UNUSED(firstStage);
    return tr("Find close PSR plots");
}

bool ClosePSRPlotsTask::execute(bool firstStage)
{
    Q_UNUSED(firstStage);

    if(!selectGates(rangeGateMeters,azimuthGateDegrees))
        return false;

    const QList<NRadarAbstractPlot*>& allData=analyser->getAllData();
    QVector<PSRPlotPoint> points;
    points.reserve(allData.size());

    analyser->setTaskProgress(0);
    for(int i=0;i<allData.size();i++)
    {
        if(i && i%10000==0)
            analyser->setTaskProgress(int((qint64(i)*25)/allData.size()));

        NRadarAbstractPlot *data=allData.at(i);
        if(data->getType()!=NRadarAbstractPlot::TypePlot ||
                !analyser->isPlotVisible(data) ||
                !isCurrentlyVisible(data))
            continue;

        NRadarPlot *plot=static_cast<NRadarPlot*>(data);
        if(plot->getSource()!=NRadarPlot::PSR)
            continue;

        const QPointF polar=plot->getADCoord();
        PSRPlotPoint point;
        point.plot=plot;
        point.hasValidTime=plot->getTime().isValid();
        point.timestampMs=point.hasValidTime ?
                    plot->getTime().toMSecsSinceEpoch() : 0;
        point.azimuthDegrees=normalizedAzimuth(polar.x());
        point.rangeMeters=polar.y();
        points.append(point);
    }

    std::stable_sort(points.begin(),points.end(),
                     [](const PSRPlotPoint& left,const PSRPlotPoint& right)
    {
        return left.rangeMeters<right.rangeMeters;
    });

    QVector<MatchCandidate> candidates;
    QElapsedTimer progressTimer;
    progressTimer.start();
    for(int i=0;i<points.size();i++)
    {
        if(progressTimer.elapsed()>1000)
        {
            analyser->setTaskProgress(25+int((qint64(i)*70)/
                                             qMax(1,points.size())));
            progressTimer.restart();
        }

        const PSRPlotPoint& point=points.at(i);
        for(int j=i+1;j<points.size();j++)
        {
            const PSRPlotPoint& other=points.at(j);
            if(other.rangeMeters-point.rangeMeters>rangeGateMeters)
                break;
            if(!isClose(point,other,azimuthGateDegrees,rangeGateMeters))
                continue;

            const double normalizedAzimuthError=
                    azimuthDifference(point.azimuthDegrees,
                                      other.azimuthDegrees)/
                    azimuthGateDegrees;
            const double normalizedRangeError=
                    qAbs(point.rangeMeters-other.rangeMeters)/rangeGateMeters;
            const double normalizedTimeError=
                    double(qAbs(point.timestampMs-other.timestampMs))/
                    MaximumTimeDifferenceMs;

            MatchCandidate candidate;
            candidate.firstPoint=i;
            candidate.secondPoint=j;
            candidate.cost=normalizedAzimuthError*normalizedAzimuthError+
                    normalizedRangeError*normalizedRangeError+
                    normalizedTimeError*normalizedTimeError;
            candidates.append(candidate);
        }
    }

    std::stable_sort(candidates.begin(),candidates.end(),
                     [](const MatchCandidate& left,
                        const MatchCandidate& right)
    {
        return left.cost<right.cost;
    });

    QSet<const NRadarPlot*> foundPlots;
    QVector<QLineF> matchLines;
    QVector<char> pointIsPaired(points.size(),0);
    foreach(const MatchCandidate& candidate,candidates)
    {
        if(pointIsPaired.at(candidate.firstPoint) ||
                pointIsPaired.at(candidate.secondPoint))
            continue;

        const PSRPlotPoint& first=points.at(candidate.firstPoint);
        const PSRPlotPoint& second=points.at(candidate.secondPoint);
        Q_ASSERT(isClose(first,second,
                         azimuthGateDegrees,rangeGateMeters));

        pointIsPaired[candidate.firstPoint]=1;
        pointIsPaired[candidate.secondPoint]=1;
        foundPlots.insert(first.plot);
        foundPlots.insert(second.plot);
        matchLines.append(QLineF(first.plot->getXYCoord(),
                                 second.plot->getXYCoord()));
    }

    delete overlay.data();
    if(!matchLines.isEmpty())
    {
        overlay=new ClosePSRPlotsOverlay(matchLines);
        analyser->getRadarScene()->addItem(overlay.data());
    }

    foreach(NRadarAbstractPlot *data,allData)
    {
        if(data->getType()!=NRadarAbstractPlot::TypePlot &&
                data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        NRadarItem *item=radarItem(data);
        if(!item)
            continue;

        const NRadarPlot *plot=data->getType()==NRadarAbstractPlot::TypePlot ?
                    static_cast<const NRadarPlot*>(data) : 0;
        item->setVisible(plot && foundPlots.contains(plot));
    }

    analyser->setTaskProgress(0,false);
    const QList<QGraphicsView*> views=analyser->getRadarScene()->views();
    if(!views.isEmpty())
        views.first()->viewport()->update();

    if(foundPlots.isEmpty())
        QMessageBox::warning(0,tr("Find close PSR plots"),
                             tr("No plots found"));

    emit finished(true);
    return true;
}

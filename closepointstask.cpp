#include <radardata/nradartrackplot.h>
#include <radarview/nradaritem.h>
#include <radarview/nradarscenelayer.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QGraphicsObject>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QSpinBox>

#include <algorithm>

#include "closepointstask.h"

namespace
{

const double DefaultRangeGateMeters=200.0;
const double DefaultAzimuthGateDegrees=2.0;
const int DefaultMinimumTrackLength=10;
const qint64 MaximumTimeDifferenceMs=5000;

struct TrackPoint
{
    NRadarTrackPlot *plot;
    quint64 trackInstance;
    uint trackNumber;
    uint icaoAddress;
    bool hasIcaoAddress;
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

class CloseTrackPointsOverlay:public QGraphicsObject
{
public:
    explicit CloseTrackPointsOverlay(const QVector<QLineF>& lines):
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

bool selectGates(double& rangeGateMeters,double& azimuthGateDegrees,
                 int& minimumTrackLength)
{
    QDialog dialog;
    dialog.setWindowTitle(QObject::tr("Find close track points"));

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

    QSpinBox trackLength;
    trackLength.setRange(1,1000000);
    trackLength.setValue(minimumTrackLength);
    layout.addRow(QObject::tr("Minimum track length:"),&trackLength);

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
    minimumTrackLength=trackLength.value();
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

bool isCloseInAzimuthAndRange(
        const TrackPoint& first,const TrackPoint& second,
        double maximumAzimuthDifferenceDegrees,
        double maximumRangeDifferenceMeters)
{
    const bool closeInAzimuth=
            azimuthDifference(first.azimuthDegrees,second.azimuthDegrees)<=
            maximumAzimuthDifferenceDegrees;
    const bool closeInRange=
            qAbs(first.rangeMeters-second.rangeMeters)<=
            maximumRangeDifferenceMeters;
    return closeInAzimuth && closeInRange;
}

bool isCloseInTime(const TrackPoint& first,const TrackPoint& second)
{
    return first.hasValidTime && second.hasValidTime &&
            qAbs(first.timestampMs-second.timestampMs)<=
            MaximumTimeDifferenceMs;
}

bool belongToDifferentTracks(const TrackPoint& first,
                             const TrackPoint& second)
{
    if(first.trackInstance==second.trackInstance ||
            first.trackNumber==second.trackNumber)
        return false;

    if(first.hasIcaoAddress && second.hasIcaoAddress &&
            first.icaoAddress==second.icaoAddress)
        return false;

    return true;
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

quint64 trackInstance(IAnalyser *analyser,const NRadarTrackPlot *track)
{
    const quint64 instance=analyser->getTrackInstanceId(track);
    if(instance)
        return instance;

    // The application normally supplies a continuous-instance ID. Keep a
    // stable fallback for hosts which only know the radar and track numbers.
    return (quint64(1)<<63)|(quint64(track->getRadarId())<<32)|
            quint64(track->getTrackId());
}

}

CloseTrackPointsTask::CloseTrackPointsTask(IAnalyser *analyser):
    AnalyserTask(analyser),
    rangeGateMeters(DefaultRangeGateMeters),
    azimuthGateDegrees(DefaultAzimuthGateDegrees),
    minimumTrackLength(DefaultMinimumTrackLength)
{
}

CloseTrackPointsTask::~CloseTrackPointsTask()
{
    delete overlay.data();
}

QString CloseTrackPointsTask::getName(bool firstStage) const
{
    return firstStage ? tr("Find close track points") :
                        tr("--> Clear close-point results");
}

bool CloseTrackPointsTask::execute(bool firstStage)
{
    if(!firstStage)
    {
        delete overlay.data();

        const QList<QGraphicsView*> views=analyser->getRadarScene()->views();
        if(!views.isEmpty())
            views.first()->viewport()->update();

        emit finished(false);
        return true;
    }

    if(!selectGates(rangeGateMeters,azimuthGateDegrees,
                    minimumTrackLength))
        return false;

    const QList<NRadarAbstractPlot*>& allData=analyser->getAllData();
    QVector<TrackPoint> points;
    points.reserve(allData.size());
    QHash<quint64,int> visibleTrackLengths;

    analyser->setTaskProgress(0);
    for(int i=0;i<allData.size();i++)
    {
        if(i && i%10000==0)
            analyser->setTaskProgress(int((qint64(i)*25)/allData.size()));

        NRadarAbstractPlot *data=allData.at(i);
        if(data->getType()!=NRadarAbstractPlot::TypeTrack ||
                !analyser->isPlotVisible(data) ||
                !isCurrentlyVisible(data))
            continue;

        NRadarTrackPlot *track=static_cast<NRadarTrackPlot*>(data);
        if(track->getTrackPlotType()!=NRadarTrackPlot::NormalPoint)
            continue;

        const QPointF polar=track->getADCoord();
        TrackPoint point;
        point.plot=track;
        point.trackInstance=trackInstance(analyser,track);
        visibleTrackLengths[point.trackInstance]++;
        point.trackNumber=track->getTrackId()&0xffffu;
        const QVariant aircraftAddress=
                track->getOption(NRadarPlot::AircraftAddress);
        point.icaoAddress=aircraftAddress.toUInt();
        point.hasIcaoAddress=!aircraftAddress.isNull() &&
                point.icaoAddress!=0;
        point.hasValidTime=track->getTime().isValid();
        point.timestampMs=point.hasValidTime ?
                    track->getTime().toMSecsSinceEpoch() : 0;
        point.azimuthDegrees=normalizedAzimuth(polar.x());
        point.rangeMeters=polar.y();
        points.append(point);
    }

    points.erase(std::remove_if(points.begin(),points.end(),
                                [&](const TrackPoint& point)
    {
        return visibleTrackLengths.value(point.trackInstance)<
                minimumTrackLength;
    }),points.end());

    std::stable_sort(points.begin(),points.end(),
                     [](const TrackPoint& left,const TrackPoint& right)
    {
        return left.rangeMeters<right.rangeMeters;
    });

    // A point is ambiguous when the same spatial window contains a point
    // from another track at a distant time. Excluding both endpoints of such
    // a pair guarantees that every reported crossing is temporally coherent,
    // rather than merely having some other time-close neighbour.
    QVector<char> hasDistantTimeNeighbour(points.size(),0);
    QElapsedTimer progressTimer;
    progressTimer.start();

    for(int i=0;i<points.size();i++)
    {
        if(progressTimer.elapsed()>1000)
        {
            analyser->setTaskProgress(25+int((qint64(i)*35)/
                                             qMax(1,points.size())));
            progressTimer.restart();
        }

        const TrackPoint& point=points.at(i);
        for(int j=i+1;j<points.size();j++)
        {
            const TrackPoint& other=points.at(j);
            if(other.rangeMeters-point.rangeMeters>rangeGateMeters)
                break;
            if(!belongToDifferentTracks(point,other))
                continue;
            if(!isCloseInAzimuthAndRange(point,other,
                                         azimuthGateDegrees,
                                         rangeGateMeters))
                continue;
            if(isCloseInTime(point,other))
                continue;

            hasDistantTimeNeighbour[i]=1;
            hasDistantTimeNeighbour[j]=1;
        }
    }

    QVector<MatchCandidate> candidates;
    progressTimer.restart();
    for(int i=0;i<points.size();i++)
    {
        if(hasDistantTimeNeighbour.at(i))
            continue;

        if(progressTimer.elapsed()>1000)
        {
            analyser->setTaskProgress(60+int((qint64(i)*35)/
                                             qMax(1,points.size())));
            progressTimer.restart();
        }

        const TrackPoint& point=points.at(i);
        for(int j=i+1;j<points.size();j++)
        {
            const TrackPoint& other=points.at(j);
            if(other.rangeMeters-point.rangeMeters>rangeGateMeters)
                break;
            if(hasDistantTimeNeighbour.at(j) ||
                    !belongToDifferentTracks(point,other) ||
                    !isCloseInAzimuthAndRange(point,other,
                                              azimuthGateDegrees,
                                              rangeGateMeters) ||
                    !isCloseInTime(point,other))
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

    // Select disjoint pairs so each line identifies one concrete pair which
    // passed all three gates.
    QVector<QLineF> matchLines;
    QVector<char> pointIsPaired(points.size(),0);
    foreach(const MatchCandidate& candidate,candidates)
    {
        if(pointIsPaired.at(candidate.firstPoint) ||
                pointIsPaired.at(candidate.secondPoint))
            continue;

        const TrackPoint& first=points.at(candidate.firstPoint);
        const TrackPoint& second=points.at(candidate.secondPoint);
        Q_ASSERT(belongToDifferentTracks(first,second));
        Q_ASSERT(isCloseInAzimuthAndRange(first,second,
                                          azimuthGateDegrees,
                                          rangeGateMeters));
        Q_ASSERT(isCloseInTime(first,second));

        pointIsPaired[candidate.firstPoint]=1;
        pointIsPaired[candidate.secondPoint]=1;
        matchLines.append(QLineF(first.plot->getXYCoord(),
                                 second.plot->getXYCoord()));
    }

    delete overlay.data();
    if(!matchLines.isEmpty())
    {
        overlay=new CloseTrackPointsOverlay(matchLines);
        analyser->getRadarScene()->addItem(overlay.data());
    }

    analyser->setTaskProgress(0,false);
    const QList<QGraphicsView*> views=analyser->getRadarScene()->views();
    if(!views.isEmpty())
        views.first()->viewport()->update();

    if(matchLines.isEmpty())
    {
        QMessageBox::warning(0,tr("Find close track points"),
                             tr("No points found"));
        emit finished(false);
        return false;
    }

    emit finished(true);
    return true;
}

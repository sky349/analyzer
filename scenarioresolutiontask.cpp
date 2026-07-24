#include <radardata/nradarmarker.h>
#include <radardata/nradarplot.h>

#include <QAbstractItemView>
#include <QBrush>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsObject>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <limits>

#include "scenarioresolutiontask.h"

namespace
{

const quint8 ScenarioRadarId=50;
const double NauticalMileMeters=1852.0;
const double ScenarioRangeGateMeters=0.25*NauticalMileMeters;
const double ScenarioAzimuthGateDegrees=2.0;
const double ScenarioStartToleranceSeconds=0.010;
const uint ScenarioDiagnosticModeA=2522;
const bool ScenarioDiagnosticLogging=false;
const int ScenarioEpochDetectionTargetCount=96;
const int ScenarioEpochDetectionScanCount=4;
const double ScenarioEpochMissingMatchPenalty=4.0;
const double ScenarioEpochMinimumCoverage=0.50;
const qint64 ScenarioAssociationCostScale=1000000;

enum ScenarioAssociationMethod
{
    CurrentMaximumCardinalityAssociation,
    MinimumCostMaximumCardinalityAssociation
};

struct ScenarioTarget
{
    QString id;
    uint modeA;
    QString address;
    double startSeconds;
    double durationSeconds;
    double altitudeFeet;
    QPointF initialPosition;
    QPointF velocity;
};

struct ScenarioOpportunity
{
    int targetIndex;
    double elapsedSeconds;
    QPointF position;
    QPointF polarPosition;
};

struct TargetStatistics
{
    int expected;
    int detected;
    int correctADetected;

    TargetStatistics():expected(0),detected(0),correctADetected(0) {}
};

struct MatchCandidate
{
    int plotIndex;
    double cost;
};

struct MatchComponent
{
    QVector<int> opportunityIndices;
    QVector<int> plotIndices;
};

struct AssociationPair
{
    int opportunityIndex;
    int plotIndex;
    double rangeResidualMeters;
    double azimuthResidualDegrees;
    double cost;
};

struct AssociationResult
{
    QVector<int> plotForOpportunity;
    QVector<int> unassignedPlots;
    QVector<AssociationPair> selectedPairs;
    double totalCost;

    AssociationResult():totalCost(0.0) {}
};

struct ScenarioDiagnosticBasePlot
{
    ScenarioOpportunity opportunity;
    int scanIndex;
    QDateTime scanStart;
    QDateTime scanEnd;
    const NRadarPlot *matchedPlot;
};

struct ScenarioDiagnosticNeighbor
{
    const NRadarPlot *plot;
    int scanIndex;
    double rangeDifferenceMeters;
    double azimuthDifferenceDegrees;
    double cartesianDistanceMeters;
    double elapsedDifferenceSeconds;
    double cost;
};

struct ScenarioEpochDetection
{
    int markerIndex;
    int sampledTargetCount;
    int expectedCount;
    int matchedCount;
    double meanPenalty;
    double secondBestPenalty;

    ScenarioEpochDetection():
        markerIndex(-1),
        sampledTargetCount(0),
        expectedCount(0),
        matchedCount(0),
        meanPenalty(0.0),
        secondBestPenalty(0.0)
    {
    }
};

class ScenarioResolutionOverlay:public QGraphicsObject
{
public:
    ScenarioResolutionOverlay(const QVector<QPointF>& basePoints,
                              const QVector<QLineF>& matchLines):
        m_basePoints(basePoints),
        m_matchLines(matchLines)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        setFlag(QGraphicsItem::ItemIsSelectable,false);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption,true);
        setZValue(1200.0);

        foreach(const QPointF& point,m_basePoints)
            extendBounds(point);
        foreach(const QLineF& line,m_matchLines)
        {
            extendBounds(line.p1());
            extendBounds(line.p2());
        }
        m_bounds.adjust(-1000.0,-1000.0,1000.0,1000.0);
    }

    QRectF boundingRect() const
    {
        return m_bounds;
    }

    void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,
               QWidget *)
    {
        if(m_basePoints.isEmpty())
            return;

        QRectF exposed=option ? option->exposedRect : painter->clipBoundingRect();
        if(exposed.isEmpty())
            exposed=painter->clipBoundingRect();

        const double levelOfDetail=
                QStyleOptionGraphicsItem::levelOfDetailFromTransform(
                    painter->worldTransform());
        const double halfSize=levelOfDetail>0.0 ? 4.0/levelOfDetail : 4.0;
        const QRectF pointArea=exposed.adjusted(
                    -halfSize,-halfSize,halfSize,halfSize);

        QVector<QLineF> visibleLines;
        visibleLines.reserve(m_matchLines.size());
        foreach(const QLineF& line,m_matchLines)
        {
            QRectF lineBounds(line.p1(),line.p2());
            lineBounds=lineBounds.normalized();
            lineBounds.adjust(-1.0,-1.0,1.0,1.0);
            if(exposed.intersects(lineBounds))
                visibleLines.append(line);
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing,true);

        QPen linePen(QColor(0,255,80),1.5);
        linePen.setCosmetic(true);
        painter->setPen(linePen);
        if(!visibleLines.isEmpty())
            painter->drawLines(visibleLines);

        QPainterPath squares;
        foreach(const QPointF& point,m_basePoints)
            if(pointArea.contains(point))
                squares.addRect(QRectF(point.x()-halfSize,point.y()-halfSize,
                                       2.0*halfSize,2.0*halfSize));

        QPen squarePen(QColor(255,0,255),2.0);
        squarePen.setCosmetic(true);
        painter->setPen(squarePen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(squares);
        painter->restore();
    }

private:
    void extendBounds(const QPointF& point)
    {
        if(m_bounds.isNull())
            m_bounds=QRectF(point,QSizeF(1.0,1.0));
        else
            m_bounds|=QRectF(point,QSizeF(1.0,1.0));
    }

    QVector<QPointF> m_basePoints;
    QVector<QLineF> m_matchLines;
    QRectF m_bounds;
};

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

bool parseScenarioModeA(const QString& rawText,uint& modeA)
{
    bool ok=false;
    const uint rawValue=rawText.toUInt(&ok,10);
    if(!ok || rawValue>0x0fff)
        return false;

    // Scenario printouts write the 12 Mode A bits as a decimal integer
    // (for example, 512).  NRadarPlot stores the four displayed octal
    // digits as a decimal number (the same example is stored as 1000).
    modeA=QString::number(rawValue,8).toUInt(&ok,10);
    return ok;
}

QPointF targetPosition(const ScenarioTarget& target,double elapsedSeconds)
{
    return target.initialPosition+
            target.velocity*(elapsedSeconds-target.startSeconds);
}

QVector<ScenarioOpportunity> scenarioOpportunitiesForScan(
        const QVector<ScenarioTarget>& targets,const NRadarMap *map,
        double scanStartElapsed,double scanDuration,
        const QVector<int>& targetIndices=QVector<int>())
{
    QVector<ScenarioOpportunity> opportunities;
    const int targetCount=targetIndices.isEmpty() ?
                targets.size() : targetIndices.size();
    opportunities.reserve(targetCount);

    for(int selectedIndex=0;selectedIndex<targetCount;selectedIndex++)
    {
        const int targetIndex=targetIndices.isEmpty() ?
                    selectedIndex : targetIndices.at(selectedIndex);
        const ScenarioTarget& target=targets.at(targetIndex);
        double crossingElapsed=scanStartElapsed;
        for(int iteration=0;iteration<4;iteration++)
        {
            const QPointF position=targetPosition(target,crossingElapsed);
            const QPointF polar=map->convertXYToAD_adjusted(position);
            crossingElapsed=scanStartElapsed+
                    normalizedAzimuth(polar.x())*scanDuration/360.0;
        }

        if(crossingElapsed<target.startSeconds)
        {
            // Scenario t0 is printed to milliseconds, while North-marker
            // timestamps have their own quantization.  A beam crossing can
            // therefore fall just before t0 even though both describe the
            // same first plot.
            if(target.startSeconds-crossingElapsed>
                    ScenarioStartToleranceSeconds)
                continue;
            crossingElapsed=target.startSeconds;
        }
        if(crossingElapsed>target.startSeconds+target.durationSeconds)
            continue;

        ScenarioOpportunity opportunity;
        opportunity.targetIndex=targetIndex;
        opportunity.elapsedSeconds=crossingElapsed;
        opportunity.position=targetPosition(target,crossingElapsed);
        opportunity.polarPosition=map->convertXYToAD_adjusted(
                    opportunity.position);
        opportunities.append(opportunity);
    }
    return opportunities;
}

bool parseScenarioFile(const QString& path,const NRadarMap *map,
                       QVector<ScenarioTarget>& targets,
                       QString& scenarioName,QString& error)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        error=QObject::tr("Cannot open scenario file: %1").arg(path);
        return false;
    }

    QStringList lines;
    QTextStream stream(&file);
    while(!stream.atEnd())
        lines.append(stream.readLine());

    scenarioName.clear();
    if(!lines.isEmpty())
    {
        const QString firstLine=lines.first().trimmed();
        const QRegularExpression scenarioNameExpression("\"([^\"]+)\"");
        const QRegularExpressionMatch scenarioNameMatch=
                scenarioNameExpression.match(firstLine);
        scenarioName=scenarioNameMatch.hasMatch() ?
                    scenarioNameMatch.captured(1).trimmed() : firstLine;
    }
    if(scenarioName.isEmpty())
        scenarioName=QFileInfo(path).completeBaseName();

    const QRegularExpression targetExpression(
                "^\\s*(\\S+)\\s+\\S+\\s+(\\d+)\\s+"
                "([0-9A-Fa-f]+)(?:\\s+\\S+)?",
                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression positionExpression(
                "t0\\s*=\\s*([0-9.]+)\\s*sec\\s+"
                "Ro\\s*=\\s*([0-9.]+)\\s*Nm\\s+"
                "Azo\\s*=\\s*([0-9.]+)\\s*Deg\\s+"
                "Zo\\s*=\\s*([0-9.]+)\\s*ft\\s+"
                "Head\\s*=\\s*([0-9.]+)\\s*Deg",
                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression speedExpression(
                "Flies\\s+([0-9.]+)\\s*Nm/h",
                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression durationExpression(
                "Flies\\s+([0-9.]+)\\s*"
                "(Seconds?|Secs?|Minutes?|Mins?|Hours?|Hrs?)\\b",
                QRegularExpression::CaseInsensitiveOption);

    int malformedTargets=0;
    for(int lineIndex=0;lineIndex<lines.size();lineIndex++)
    {
        const QRegularExpressionMatch targetMatch=
                targetExpression.match(lines.at(lineIndex));
        if(!targetMatch.hasMatch())
            continue;

        QStringList details;
        for(int next=lineIndex+1;next<lines.size() && details.size()<3;next++)
            if(!lines.at(next).trimmed().isEmpty())
                details.append(lines.at(next));

        if(details.size()!=3)
        {
            malformedTargets++;
            continue;
        }

        const QRegularExpressionMatch positionMatch=
                positionExpression.match(details.at(0));
        const QRegularExpressionMatch speedMatch=
                speedExpression.match(details.at(1));
        const QRegularExpressionMatch durationMatch=
                durationExpression.match(details.at(2));
        if(!positionMatch.hasMatch() || !speedMatch.hasMatch() ||
                !durationMatch.hasMatch())
        {
            malformedTargets++;
            continue;
        }

        ScenarioTarget target;
        target.id=targetMatch.captured(1).toUpper();
        if(!parseScenarioModeA(targetMatch.captured(2),target.modeA))
        {
            malformedTargets++;
            continue;
        }
        target.address=targetMatch.captured(3).toUpper();
        target.startSeconds=positionMatch.captured(1).toDouble();
        target.altitudeFeet=positionMatch.captured(4).toDouble();
        const double rangeMeters=
                positionMatch.captured(2).toDouble()*NauticalMileMeters;
        const double azimuth=positionMatch.captured(3).toDouble();
        const double heading=positionMatch.captured(5).toDouble();
        const double speedMetersPerSecond=
                speedMatch.captured(1).toDouble()*NauticalMileMeters/3600.0;
        target.durationSeconds=durationMatch.captured(1).toDouble();
        const QString durationUnit=durationMatch.captured(2).toLower();
        if(durationUnit.startsWith("min"))
            target.durationSeconds*=60.0;
        else if(durationUnit.startsWith("hour") ||
                durationUnit.startsWith("hr"))
            target.durationSeconds*=3600.0;
        target.initialPosition=map->convertADToXY_adjusted(
                    QPointF(azimuth,rangeMeters));
        target.velocity=map->convertADToXY_adjusted(
                    QPointF(heading,speedMetersPerSecond));

        if(target.durationSeconds<=0.0)
        {
            malformedTargets++;
            continue;
        }
        targets.append(target);
    }

    if(targets.isEmpty())
    {
        error=QObject::tr("No valid target definitions were found in %1")
                .arg(path);
        return false;
    }
    if(malformedTargets)
        error=QObject::tr("%1 malformed target blocks were skipped")
                .arg(malformedTargets);
    return true;
}

QString defaultScenarioPath()
{
    const QStringList candidates=QStringList()
            << QDir::current().filePath("base_tracks.txt")
            << QDir(QCoreApplication::applicationDirPath())
               .filePath("base_tracks.txt")
            << QDir(QCoreApplication::applicationDirPath())
               .filePath("../base_tracks.txt");
    foreach(const QString& candidate,candidates)
        if(QFile::exists(candidate))
            return QDir::cleanPath(candidate);
    return QDir::currentPath();
}

QVector<const NRadarMarker*> northMarkers(
        const QList<NRadarAbstractPlot*>& allData)
{
    QVector<const NRadarMarker*> markers;
    foreach(const NRadarAbstractPlot *data,allData)
    {
        if(data->getType()!=NRadarAbstractPlot::TypeMarker ||
                data->getRadarId()!=ScenarioRadarId)
            continue;

        const NRadarMarker *marker=static_cast<const NRadarMarker*>(data);
        if(marker->isNorthMarker())
            markers.append(marker);
    }
    std::sort(markers.begin(),markers.end(),
              [](const NRadarMarker *first,const NRadarMarker *second)
    {
        return first->getTime()<second->getTime();
    });

    QVector<const NRadarMarker*> uniqueMarkers;
    foreach(const NRadarMarker *marker,markers)
        if(uniqueMarkers.isEmpty() ||
                uniqueMarkers.last()->getTime()<marker->getTime())
            uniqueMarkers.append(marker);
    return uniqueMarkers;
}

bool isEligibleScenarioPlot(const NRadarPlot *plot)
{
    return plot->getRadarId()==ScenarioRadarId &&
            plot->getSource()==NRadarPlot::SSR &&
            plot->getSSRType()!=NRadarPlot::ModeS;
}

QVector<const NRadarPlot*> measuredPlots(
        const QList<NRadarAbstractPlot*>& allData)
{
    QVector<const NRadarPlot*> plots;
    foreach(const NRadarAbstractPlot *data,allData)
    {
        if(data->getType()!=NRadarAbstractPlot::TypePlot)
            continue;

        const NRadarPlot *plot=static_cast<const NRadarPlot*>(data);
        if(isEligibleScenarioPlot(plot))
            plots.append(plot);
    }
    // Preserve the recording's source order when timestamps are equal so
    // plot indices provide a stable tie-break for the weighted solver.
    std::stable_sort(plots.begin(),plots.end(),
              [](const NRadarPlot *first,const NRadarPlot *second)
    {
        return first->getTime()<second->getTime();
    });
    return plots;
}

bool findAugmentingMatch(
        int opportunityIndex,
        const QVector<QVector<MatchCandidate> >& candidates,
        QVector<int>& plotOwners,
        QVector<int>& opportunityPlots,
        QVector<char>& visitedPlots)
{
    foreach(const MatchCandidate& candidate,candidates.at(opportunityIndex))
    {
        if(visitedPlots.at(candidate.plotIndex))
            continue;
        visitedPlots[candidate.plotIndex]=1;

        const int previousOwner=plotOwners.at(candidate.plotIndex);
        if(previousOwner<0 ||
                findAugmentingMatch(previousOwner,candidates,plotOwners,
                                    opportunityPlots,visitedPlots))
        {
            plotOwners[candidate.plotIndex]=opportunityIndex;
            opportunityPlots[opportunityIndex]=candidate.plotIndex;
            return true;
        }
    }
    return false;
}

QVector<QVector<MatchCandidate> > buildMatchCandidates(
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots)
{
    QVector<QVector<MatchCandidate> > candidates(opportunities.size());
    for(int opportunityIndex=0;
            opportunityIndex<opportunities.size();opportunityIndex++)
    {
        const ScenarioOpportunity& opportunity=
                opportunities.at(opportunityIndex);
        QVector<MatchCandidate>& opportunityCandidates=
                candidates[opportunityIndex];
        if(!qIsFinite(opportunity.polarPosition.x()) ||
                !qIsFinite(opportunity.polarPosition.y()))
            continue;

        for(int plotIndex=0;plotIndex<plots.size();plotIndex++)
        {
            const NRadarPlot *plot=plots.at(plotIndex);
            const QPointF plotPolar=plot->getADCoord();
            if(!qIsFinite(plotPolar.x()) || !qIsFinite(plotPolar.y()))
                continue;
            const double rangeDifference=
                    qAbs(plotPolar.y()-opportunity.polarPosition.y());
            if(rangeDifference>ScenarioRangeGateMeters)
                continue;

            const double angleDifference=azimuthDifference(
                        plotPolar.x(),opportunity.polarPosition.x());
            if(angleDifference>ScenarioAzimuthGateDegrees)
                continue;

            MatchCandidate candidate;
            candidate.plotIndex=plotIndex;
            candidate.cost=
                    qPow(rangeDifference/ScenarioRangeGateMeters,2.0)+
                    qPow(angleDifference/ScenarioAzimuthGateDegrees,2.0);
            opportunityCandidates.append(candidate);
        }
    }
    return candidates;
}

QVector<int> matchScan(
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots)
{
    QVector<QVector<MatchCandidate> > candidates=
            buildMatchCandidates(opportunities,plots);
    for(int opportunityIndex=0;
            opportunityIndex<candidates.size();opportunityIndex++)
    {
        QVector<MatchCandidate>& opportunityCandidates=
                candidates[opportunityIndex];

        std::sort(opportunityCandidates.begin(),opportunityCandidates.end(),
                  [](const MatchCandidate& first,const MatchCandidate& second)
        {
            return first.cost<second.cost;
        });
    }

    QVector<int> opportunityOrder(opportunities.size());
    for(int i=0;i<opportunityOrder.size();i++)
        opportunityOrder[i]=i;
    std::sort(opportunityOrder.begin(),opportunityOrder.end(),
              [&candidates](int first,int second)
    {
        if(candidates.at(first).size()!=candidates.at(second).size())
            return candidates.at(first).size()<candidates.at(second).size();
        const double firstCost=candidates.at(first).isEmpty() ?
                    1e100 : candidates.at(first).first().cost;
        const double secondCost=candidates.at(second).isEmpty() ?
                    1e100 : candidates.at(second).first().cost;
        return firstCost<secondCost;
    });

    QVector<int> plotOwners(plots.size(),-1);
    QVector<int> opportunityPlots(opportunities.size(),-1);
    foreach(int opportunityIndex,opportunityOrder)
    {
        QVector<char> visitedPlots(plots.size(),0);
        findAugmentingMatch(opportunityIndex,candidates,plotOwners,
                            opportunityPlots,visitedPlots);
    }
    return opportunityPlots;
}

QVector<MatchComponent> matchComponents(
        const QVector<QVector<MatchCandidate> >& candidates,int plotCount)
{
    QVector<QVector<int> > opportunitiesByPlot(plotCount);
    for(int opportunityIndex=0;
            opportunityIndex<candidates.size();opportunityIndex++)
    {
        foreach(const MatchCandidate& candidate,
                candidates.at(opportunityIndex))
            opportunitiesByPlot[candidate.plotIndex].append(opportunityIndex);
    }

    QVector<char> visitedOpportunities(candidates.size(),0);
    QVector<char> visitedPlots(plotCount,0);
    QVector<MatchComponent> components;
    for(int startOpportunity=0;
            startOpportunity<candidates.size();startOpportunity++)
    {
        if(visitedOpportunities.at(startOpportunity) ||
                candidates.at(startOpportunity).isEmpty())
            continue;

        MatchComponent component;
        component.opportunityIndices.append(startOpportunity);
        visitedOpportunities[startOpportunity]=1;
        int opportunityCursor=0;
        int plotCursor=0;
        while(opportunityCursor<component.opportunityIndices.size() ||
              plotCursor<component.plotIndices.size())
        {
            while(opportunityCursor<component.opportunityIndices.size())
            {
                const int opportunityIndex=
                        component.opportunityIndices.at(opportunityCursor++);
                foreach(const MatchCandidate& candidate,
                        candidates.at(opportunityIndex))
                {
                    if(visitedPlots.at(candidate.plotIndex))
                        continue;
                    visitedPlots[candidate.plotIndex]=1;
                    component.plotIndices.append(candidate.plotIndex);
                }
            }
            while(plotCursor<component.plotIndices.size())
            {
                const int plotIndex=
                        component.plotIndices.at(plotCursor++);
                foreach(int opportunityIndex,
                        opportunitiesByPlot.at(plotIndex))
                {
                    if(visitedOpportunities.at(opportunityIndex))
                        continue;
                    visitedOpportunities[opportunityIndex]=1;
                    component.opportunityIndices.append(opportunityIndex);
                }
            }
        }

        std::sort(component.opportunityIndices.begin(),
                  component.opportunityIndices.end());
        std::sort(component.plotIndices.begin(),
                  component.plotIndices.end());
        components.append(component);
    }
    return components;
}

QVector<int> solveRectangularAssignment(
        const QVector<QVector<qint64> >& costs)
{
    const int rowCount=costs.size();
    if(!rowCount)
        return QVector<int>();
    const int columnCount=costs.first().size();
    const qint64 infinity=std::numeric_limits<qint64>::max()/4;

    // Shortest-augmenting-path form of the rectangular Hungarian algorithm.
    // Strict comparisons and ascending row/column traversal make quantised
    // exact ties deterministic.
    QVector<qint64> rowPotentials(rowCount+1,0);
    QVector<qint64> columnPotentials(columnCount+1,0);
    QVector<int> columnOwners(columnCount+1,0);
    QVector<int> previousColumns(columnCount+1,0);
    for(int row=1;row<=rowCount;row++)
    {
        columnOwners[0]=row;
        int currentColumn=0;
        QVector<qint64> minimumReducedCosts(columnCount+1,infinity);
        QVector<char> usedColumns(columnCount+1,0);
        do
        {
            usedColumns[currentColumn]=1;
            const int currentRow=columnOwners.at(currentColumn);
            qint64 delta=infinity;
            int nextColumn=0;
            for(int column=1;column<=columnCount;column++)
            {
                if(usedColumns.at(column))
                    continue;
                const qint64 reducedCost=
                        costs.at(currentRow-1).at(column-1)-
                        rowPotentials.at(currentRow)-
                        columnPotentials.at(column);
                if(reducedCost<minimumReducedCosts.at(column))
                {
                    minimumReducedCosts[column]=reducedCost;
                    previousColumns[column]=currentColumn;
                }
                if(minimumReducedCosts.at(column)<delta)
                {
                    delta=minimumReducedCosts.at(column);
                    nextColumn=column;
                }
            }

            Q_ASSERT(nextColumn);
            for(int column=0;column<=columnCount;column++)
            {
                if(usedColumns.at(column))
                {
                    rowPotentials[columnOwners.at(column)]+=delta;
                    columnPotentials[column]-=delta;
                }
                else if(minimumReducedCosts.at(column)<infinity)
                    minimumReducedCosts[column]-=delta;
            }
            currentColumn=nextColumn;
        }
        while(columnOwners.at(currentColumn));

        do
        {
            const int previousColumn=previousColumns.at(currentColumn);
            columnOwners[currentColumn]=
                    columnOwners.at(previousColumn);
            currentColumn=previousColumn;
        }
        while(currentColumn);
    }

    QVector<int> columnsByRow(rowCount,-1);
    for(int column=1;column<=columnCount;column++)
    {
        if(columnOwners.at(column)>0)
            columnsByRow[columnOwners.at(column)-1]=column-1;
    }
    return columnsByRow;
}

qint64 quantizedMatchCost(double cost)
{
    const double boundedCost=qBound(0.0,cost,2.0);
    return qRound64(boundedCost*ScenarioAssociationCostScale);
}

QVector<int> minimumCostMaximumMatches(
        const QVector<QVector<MatchCandidate> >& candidates,int plotCount)
{
    const QVector<MatchComponent> components=
            matchComponents(candidates,plotCount);
    QVector<int> opportunityPlots(candidates.size(),-1);
    QVector<int> localColumnByPlot(plotCount,-1);

    foreach(const MatchComponent& component,components)
    {
        const int rowCount=component.opportunityIndices.size();
        const int realColumnCount=component.plotIndices.size();
        const int maximumAssociations=qMin(rowCount,realColumnCount);
        const int columnCount=realColumnCount+rowCount;

        // Candidate costs are at most 2 * scale.  One missed-detection
        // penalty is therefore larger than every possible improvement in
        // all real edges of this component, preserving maximum cardinality
        // as the primary objective.
        const qint64 missedDetectionPenalty=
                (2*qint64(maximumAssociations)+1)*
                ScenarioAssociationCostScale;
        const qint64 forbiddenCost=
                (qint64(rowCount)+1)*missedDetectionPenalty;
        QVector<QVector<qint64> > costs(
                    rowCount,QVector<qint64>(columnCount,forbiddenCost));

        for(int localPlotIndex=0;
                localPlotIndex<component.plotIndices.size();
                localPlotIndex++)
        {
            localColumnByPlot[
                    component.plotIndices.at(localPlotIndex)]=localPlotIndex;
        }

        for(int localRow=0;localRow<rowCount;localRow++)
        {
            const int opportunityIndex=
                    component.opportunityIndices.at(localRow);
            foreach(const MatchCandidate& candidate,
                    candidates.at(opportunityIndex))
            {
                const int localColumn=
                        localColumnByPlot.at(candidate.plotIndex);
                Q_ASSERT(localColumn>=0);
                costs[localRow][localColumn]=
                        quantizedMatchCost(candidate.cost);
            }
            // Each base point has one private missed-detection column.
            costs[localRow][realColumnCount+localRow]=
                    missedDetectionPenalty;
        }

        const QVector<int> assignedColumns=
                solveRectangularAssignment(costs);
        Q_ASSERT(assignedColumns.size()==rowCount);
        for(int localRow=0;localRow<rowCount;localRow++)
        {
            const int assignedColumn=assignedColumns.at(localRow);
            if(assignedColumn>=0 && assignedColumn<realColumnCount)
            {
                const int opportunityIndex=
                        component.opportunityIndices.at(localRow);
                opportunityPlots[opportunityIndex]=
                        component.plotIndices.at(assignedColumn);
            }
        }

        foreach(int plotIndex,component.plotIndices)
            localColumnByPlot[plotIndex]=-1;
    }
    return opportunityPlots;
}

QVector<int> matchScanMinimumCostMaximum(
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots)
{
    return minimumCostMaximumMatches(
                buildMatchCandidates(opportunities,plots),plots.size());
}

AssociationResult associationResult(
        const QVector<int>& matches,
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots)
{
    AssociationResult result;
    result.plotForOpportunity=matches;
    QVector<char> assignedPlots(plots.size(),0);
    for(int opportunityIndex=0;
            opportunityIndex<matches.size();opportunityIndex++)
    {
        const int plotIndex=matches.at(opportunityIndex);
        if(plotIndex<0)
            continue;

        Q_ASSERT(plotIndex<plots.size());
        Q_ASSERT(!assignedPlots.at(plotIndex));
        assignedPlots[plotIndex]=1;
        const QPointF measuredPolar=plots.at(plotIndex)->getADCoord();
        const QPointF expectedPolar=
                opportunities.at(opportunityIndex).polarPosition;

        AssociationPair pair;
        pair.opportunityIndex=opportunityIndex;
        pair.plotIndex=plotIndex;
        pair.rangeResidualMeters=
                qAbs(measuredPolar.y()-expectedPolar.y());
        pair.azimuthResidualDegrees=azimuthDifference(
                    measuredPolar.x(),expectedPolar.x());
        pair.cost=
                qPow(pair.rangeResidualMeters/
                     ScenarioRangeGateMeters,2.0)+
                qPow(pair.azimuthResidualDegrees/
                     ScenarioAzimuthGateDegrees,2.0);
        result.selectedPairs.append(pair);
        result.totalCost+=pair.cost;
    }

    for(int plotIndex=0;plotIndex<plots.size();plotIndex++)
    {
        if(!assignedPlots.at(plotIndex))
            result.unassignedPlots.append(plotIndex);
    }
    return result;
}

AssociationResult associateScan(
        const QVector<ScenarioOpportunity>& opportunities,
        const QVector<const NRadarPlot*>& plots,
        ScenarioAssociationMethod method)
{
    QVector<int> matches;
    if(method==MinimumCostMaximumCardinalityAssociation)
        matches=matchScanMinimumCostMaximum(opportunities,plots);
    else
        matches=matchScan(opportunities,plots);
    return associationResult(matches,opportunities,plots);
}

QString associationMethodText(ScenarioAssociationMethod method)
{
    if(method==MinimumCostMaximumCardinalityAssociation)
        return QObject::tr("Global minimum-cost maximum matching");
    return QObject::tr("Current maximum-cardinality matching");
}

QVector<QVector<const NRadarPlot*> > groupPlotsByNorthScan(
        const QVector<const NRadarMarker*>& markers,
        const QVector<const NRadarPlot*>& plots)
{
    QVector<QVector<const NRadarPlot*> > plotsByScan(
                qMax(0,markers.size()-1));
    int plotIndex=0;
    for(int scanIndex=0;scanIndex<plotsByScan.size();scanIndex++)
    {
        const QDateTime scanStart=markers.at(scanIndex)->getTime();
        const QDateTime scanEnd=markers.at(scanIndex+1)->getTime();
        while(plotIndex<plots.size() &&
              plots.at(plotIndex)->getTime()<scanStart)
            plotIndex++;
        while(plotIndex<plots.size() &&
              plots.at(plotIndex)->getTime()<scanEnd)
        {
            plotsByScan[scanIndex].append(plots.at(plotIndex));
            plotIndex++;
        }
    }
    return plotsByScan;
}

QVector<int> scenarioEpochSampleTargets(
        const QVector<ScenarioTarget>& targets)
{
    QVector<int> sample;
    if(targets.isEmpty())
        return sample;

    const int sampleCount=qMin(
                ScenarioEpochDetectionTargetCount,targets.size());
    for(int sampleIndex=0;
            sampleIndex<sampleCount;sampleIndex++)
    {
        const int bucketStart=int(
                    qint64(sampleIndex)*targets.size()/sampleCount);
        const int bucketEnd=int(
                    qint64(sampleIndex+1)*targets.size()/sampleCount);
        const int alternatingOffset=
                (sampleIndex&1) && bucketEnd-bucketStart>1 ? 1 : 0;
        sample.append(bucketStart+alternatingOffset);
    }
    return sample;
}

ScenarioEpochDetection detectScenarioEpoch(
        const QVector<ScenarioTarget>& targets,const NRadarMap *map,
        const QVector<const NRadarMarker*>& markers,
        const QVector<QVector<const NRadarPlot*> >& plotsByScan,
        ScenarioAssociationMethod associationMethod,
        IAnalyser *analyser)
{
    ScenarioEpochDetection result;
    const QVector<int> sampleTargets=scenarioEpochSampleTargets(targets);
    result.sampledTargetCount=sampleTargets.size();
    if(sampleTargets.isEmpty() ||
            markers.size()<ScenarioEpochDetectionScanCount+2)
        return result;

    double bestPenalty=1e100;
    double secondBestPenalty=1e100;
    int bestExpectedCount=0;
    int bestMatchedCount=0;
    const int candidateCount=
            markers.size()-ScenarioEpochDetectionScanCount-1;

    for(int candidateIndex=0;
            candidateIndex<candidateCount;candidateIndex++)
    {
        const QDateTime candidateEpoch=
                markers.at(candidateIndex)->getTime();
        int expectedCount=0;
        int matchedCount=0;
        double totalPenalty=0.0;

        // Skip the candidate's first scan: a recording can start partway
        // through it.  Several following complete scans provide a stable
        // multi-aircraft alignment score.
        for(int scanOffset=1;
                scanOffset<=ScenarioEpochDetectionScanCount;scanOffset++)
        {
            const int scanIndex=candidateIndex+scanOffset;
            const QDateTime scanStart=markers.at(scanIndex)->getTime();
            const QDateTime scanEnd=markers.at(scanIndex+1)->getTime();
            const double scanDuration=
                    scanStart.msecsTo(scanEnd)/1000.0;
            if(scanDuration<=0.0)
                continue;

            const double scanStartElapsed=
                    candidateEpoch.msecsTo(scanStart)/1000.0;
            const QVector<ScenarioOpportunity> opportunities=
                    scenarioOpportunitiesForScan(
                        targets,map,scanStartElapsed,scanDuration,
                        sampleTargets);
            const AssociationResult association=associateScan(
                        opportunities,plotsByScan.at(scanIndex),
                        associationMethod);
            expectedCount+=opportunities.size();
            matchedCount+=association.selectedPairs.size();
            totalPenalty+=association.totalCost+
                    (opportunities.size()-
                     association.selectedPairs.size())*
                    ScenarioEpochMissingMatchPenalty;
        }

        const double coverage=expectedCount ?
                    double(matchedCount)/expectedCount : 0.0;
        const double meanPenalty=expectedCount ?
                    totalPenalty/expectedCount : 1e100;
        if(coverage>=ScenarioEpochMinimumCoverage)
        {
            if(meanPenalty<bestPenalty)
            {
                secondBestPenalty=bestPenalty;
                bestPenalty=meanPenalty;
                result.markerIndex=candidateIndex;
                bestExpectedCount=expectedCount;
                bestMatchedCount=matchedCount;
            }
            else if(meanPenalty<secondBestPenalty)
                secondBestPenalty=meanPenalty;
        }

        if(analyser && candidateIndex%4==0)
            analyser->setTaskProgress(
                    int(qint64(candidateIndex+1)*45/candidateCount));
    }

    if(result.markerIndex>=0)
    {
        result.expectedCount=bestExpectedCount;
        result.matchedCount=bestMatchedCount;
        result.meanPenalty=bestPenalty;
        result.secondBestPenalty=secondBestPenalty<1e99 ?
                    secondBestPenalty : bestPenalty;
    }
    return result;
}

QString modeAText(uint modeA)
{
    return QString("%1").arg(modeA,4,10,QChar('0'));
}

QString plotSourceText(NRadarPlot::NPlotSourceType source)
{
    switch(source)
    {
    case NRadarPlot::PSR: return "PSR";
    case NRadarPlot::SSR: return "SSR";
    case NRadarPlot::Combined: return "Combined";
    case NRadarPlot::ADSB: return "ADS-B";
    default: return "None";
    }
}

QString plotSSRTypeText(NRadarPlot::NPlotSSRType type)
{
    switch(type)
    {
    case NRadarPlot::ChannelRBS: return "RBS";
    case NRadarPlot::ChannelUVD: return "UVD";
    case NRadarPlot::ModeS: return "Mode-S";
    default: return "None";
    }
}

QString utcText(const QDateTime& time)
{
    return time.toUTC().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

int scanIndexForTime(const QVector<const NRadarMarker*>& markers,
                     const QDateTime& time)
{
    int first=0;
    int last=markers.size()-1;
    while(first<last)
    {
        const int middle=(first+last+1)/2;
        if(markers.at(middle)->getTime()<=time)
            first=middle;
        else
            last=middle-1;
    }

    if(first<0 || first>=markers.size()-1 ||
            time<markers.at(first)->getTime() ||
            time>=markers.at(first+1)->getTime())
        return -1;
    return first;
}

QString plotModeAText(const NRadarPlot *plot)
{
    return plot->hasBoardNumber() ?
                modeAText(plot->getBoardNumber()) : QString("N/A");
}

QString writeScenarioAssociationDiagnostics(
        const QString& scenarioPath,uint diagnosticModeA,
        const QDateTime& scenarioEpoch,const NRadarMap *map,
        const QVector<const NRadarMarker*>& markers,
        const QVector<const NRadarPlot*>& eligiblePlots,
        const QList<NRadarAbstractPlot*>& allData,
        const QVector<ScenarioTarget>& targets,
        const QVector<ScenarioDiagnosticBasePlot>& basePlots)
{
    const QString fileName=QString(
                "ac_resolution_scenario_mode_a_%1.log")
            .arg(modeAText(diagnosticModeA));
    QString logPath=QFileInfo(scenarioPath).dir().filePath(fileName);
    QFile file(logPath);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text))
    {
        logPath=QDir(QDir::tempPath()).filePath(fileName);
        file.setFileName(logPath);
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text))
        {
            qWarning().noquote()
                    << "Cannot create scenario diagnostic log:"
                    << file.errorString();
            return QString();
        }
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "A/C resolution w/ scenario - association diagnostic\n";
    out << "Scenario file: " << scenarioPath << '\n';
    out << "Requested Mode A: " << modeAText(diagnosticModeA) << '\n';
    out << "Automatically detected scenario epoch / radar 51 North marker: "
        << utcText(scenarioEpoch) << " UTC\n";
    out << "Current association criteria: same radar-51 North-marker scan, "
        << ScenarioRangeGateMeters/NauticalMileMeters
        << " NM range gate, " << ScenarioAzimuthGateDegrees
        << " degree azimuth gate, one measured plot per base plot\n\n";

    out << "SCENARIO TARGETS WITH THIS MODE A\n";
    int targetCount=0;
    for(int targetIndex=0;targetIndex<targets.size();targetIndex++)
    {
        const ScenarioTarget& target=targets.at(targetIndex);
        if(target.modeA!=diagnosticModeA)
            continue;

        targetCount++;
        const QPointF initialPolar=map->convertXYToAD_adjusted(
                    target.initialPosition);
        const QPointF velocityPolar=map->convertXYToAD_adjusted(
                    target.velocity);
        out << "targetIndex=" << targetIndex
            << " id=" << target.id
            << " modeA=" << modeAText(target.modeA)
            << " rawModeA=" << target.modeA
            << " address=" << target.address
            << " t0=" << QString::number(target.startSeconds,'f',6)
            << " duration=" << QString::number(target.durationSeconds,'f',3)
            << " end=" << QString::number(
                   target.startSeconds+target.durationSeconds,'f',6)
            << " RoNm=" << QString::number(
                   initialPolar.y()/NauticalMileMeters,'f',6)
            << " AzoDeg=" << QString::number(initialPolar.x(),'f',6)
            << " altitudeFt=" << QString::number(
                   target.altitudeFeet,'f',1)
            << " headingDeg=" << QString::number(velocityPolar.x(),'f',6)
            << " speedNmH=" << QString::number(
                   velocityPolar.y()*3600.0/NauticalMileMeters,'f',3)
            << '\n';
    }
    if(!targetCount)
        out << "NONE\n";
    out << '\n';

    int minimumScan=markers.size();
    int maximumScan=-1;
    foreach(const ScenarioDiagnosticBasePlot& basePlot,basePlots)
    {
        minimumScan=qMin(minimumScan,basePlot.scanIndex);
        maximumScan=qMax(maximumScan,basePlot.scanIndex);
    }

    out << "RADAR 51 NORTH MARKERS AROUND THE TARGET LIFETIME\n";
    if(maximumScan<0)
        out << "No generated base plots; all loaded North markers follow.\n";
    const int firstMarker=maximumScan>=0 ?
                qMax(0,minimumScan-1) : 0;
    const int lastMarker=maximumScan>=0 ?
                qMin(markers.size()-1,maximumScan+2) :
                markers.size()-1;
    for(int markerIndex=firstMarker;markerIndex<=lastMarker;markerIndex++)
    {
        const QDateTime markerTime=markers.at(markerIndex)->getTime();
        out << "marker=" << markerIndex
            << " utc=" << utcText(markerTime)
            << " elapsed=" << QString::number(
                   scenarioEpoch.msecsTo(markerTime)/1000.0,'f',6);
        if(markerIndex+1<markers.size())
            out << " nextInterval="
                << QString::number(markerTime.msecsTo(
                       markers.at(markerIndex+1)->getTime())/1000.0,
                       'f',6);
        out << '\n';
    }
    out << '\n';

    QVector<const NRadarPlot*> reportedModeAPlots;
    foreach(const NRadarAbstractPlot *data,allData)
    {
        if(data->getType()!=NRadarAbstractPlot::TypePlot)
            continue;
        const NRadarPlot *plot=static_cast<const NRadarPlot*>(data);
        if(plot->hasBoardNumber() &&
                plot->getBoardNumber()==diagnosticModeA)
            reportedModeAPlots.append(plot);
    }
    std::sort(reportedModeAPlots.begin(),reportedModeAPlots.end(),
              [](const NRadarPlot *first,const NRadarPlot *second)
    {
        return first->getTime()<second->getTime();
    });

    out << "ALL MEASURED TYPE-PLOT RECORDS REPORTING MODE A "
        << modeAText(diagnosticModeA) << '\n';
    out << "count=" << reportedModeAPlots.size() << '\n';
    foreach(const NRadarPlot *plot,reportedModeAPlots)
    {
        const QPointF polar=plot->getADCoord();
        const QPointF xy=plot->getXYCoord();
        out << "utc=" << utcText(plot->getTime())
            << " elapsed=" << QString::number(
                   scenarioEpoch.msecsTo(plot->getTime())/1000.0,'f',6)
            << " scan=" << scanIndexForTime(markers,plot->getTime())
            << " radar=" << plot->getRadarId()
            << " source=" << plotSourceText(plot->getSource())
            << " ssrType=" << plotSSRTypeText(plot->getSSRType())
            << " eligible=" << (isEligibleScenarioPlot(plot) ? "yes" : "no")
            << " modeA=" << plotModeAText(plot)
            << " rangeNm=" << QString::number(
                   polar.y()/NauticalMileMeters,'f',6)
            << " azDeg=" << QString::number(polar.x(),'f',6)
            << " xM=" << QString::number(xy.x(),'f',1)
            << " yM=" << QString::number(xy.y(),'f',1)
            << " heightFt="
            << (plot->hasHeight() ?
                QString::number(plot->getHeight()/0.3048,'f',1) :
                QString("N/A"))
            << " modeAInvalid="
            << (plot->getOption(
                    NRadarPlot::ModeACodeNotValidated).toBool() ? "yes" : "no")
            << " modeAGarbled="
            << (plot->getOption(
                    NRadarPlot::ModeACodeGarbled).toBool() ? "yes" : "no")
            << '\n';
    }
    out << '\n';

    out << "GENERATED BASE PLOTS AND NEAREST ELIGIBLE MEASURED PLOTS\n";
    out << "baseCount=" << basePlots.size()
        << " eligibleMeasuredPlotCount=" << eligiblePlots.size() << '\n';
    for(int baseIndex=0;baseIndex<basePlots.size();baseIndex++)
    {
        const ScenarioDiagnosticBasePlot& basePlot=basePlots.at(baseIndex);
        const ScenarioOpportunity& opportunity=basePlot.opportunity;
        const ScenarioTarget& target=targets.at(opportunity.targetIndex);
        const QDateTime baseTime=scenarioEpoch.addMSecs(
                    qRound64(opportunity.elapsedSeconds*1000.0));

        out << "\nBASE index=" << baseIndex
            << " target=" << target.id
            << " scan=" << basePlot.scanIndex
            << " scanStart=" << utcText(basePlot.scanStart)
            << " scanEnd=" << utcText(basePlot.scanEnd)
            << " baseUtc=" << utcText(baseTime)
            << " elapsed=" << QString::number(
                   opportunity.elapsedSeconds,'f',6)
            << " rangeNm=" << QString::number(
                   opportunity.polarPosition.y()/NauticalMileMeters,'f',6)
            << " azDeg=" << QString::number(
                   opportunity.polarPosition.x(),'f',6)
            << " xM=" << QString::number(opportunity.position.x(),'f',1)
            << " yM=" << QString::number(opportunity.position.y(),'f',1)
            << '\n';

        if(basePlot.matchedPlot)
            out << "ACTUAL_MATCH utc="
                << utcText(basePlot.matchedPlot->getTime())
                << " modeA=" << plotModeAText(basePlot.matchedPlot)
                << " rangeNm=" << QString::number(
                       basePlot.matchedPlot->getADCoord().y()/
                       NauticalMileMeters,'f',6)
                << " azDeg=" << QString::number(
                       basePlot.matchedPlot->getADCoord().x(),'f',6)
                << '\n';
        else
            out << "ACTUAL_MATCH none\n";

        QVector<ScenarioDiagnosticNeighbor> neighbors;
        int sameScanPlotCount=0;
        int sameScanPassingCount=0;
        foreach(const NRadarPlot *plot,eligiblePlots)
        {
            const int plotScan=scanIndexForTime(markers,plot->getTime());
            if(qAbs(plotScan-basePlot.scanIndex)>1)
                continue;

            ScenarioDiagnosticNeighbor neighbor;
            neighbor.plot=plot;
            neighbor.scanIndex=plotScan;
            const QPointF plotPolar=plot->getADCoord();
            neighbor.rangeDifferenceMeters=qAbs(
                        plotPolar.y()-opportunity.polarPosition.y());
            neighbor.azimuthDifferenceDegrees=azimuthDifference(
                        plotPolar.x(),opportunity.polarPosition.x());
            neighbor.cartesianDistanceMeters=QLineF(
                        plot->getXYCoord(),opportunity.position).length();
            neighbor.elapsedDifferenceSeconds=
                    scenarioEpoch.msecsTo(plot->getTime())/1000.0-
                    opportunity.elapsedSeconds;
            neighbor.cost=
                    qPow(neighbor.rangeDifferenceMeters/
                         ScenarioRangeGateMeters,2.0)+
                    qPow(neighbor.azimuthDifferenceDegrees/
                         ScenarioAzimuthGateDegrees,2.0);
            neighbors.append(neighbor);

            if(plotScan==basePlot.scanIndex)
            {
                sameScanPlotCount++;
                if(neighbor.rangeDifferenceMeters<=
                        ScenarioRangeGateMeters &&
                        neighbor.azimuthDifferenceDegrees<=
                        ScenarioAzimuthGateDegrees)
                    sameScanPassingCount++;
            }
        }

        std::sort(neighbors.begin(),neighbors.end(),
                  [&basePlot](const ScenarioDiagnosticNeighbor& first,
                              const ScenarioDiagnosticNeighbor& second)
        {
            const int firstScanDistance=qAbs(
                        first.scanIndex-basePlot.scanIndex);
            const int secondScanDistance=qAbs(
                        second.scanIndex-basePlot.scanIndex);
            if(firstScanDistance!=secondScanDistance)
                return firstScanDistance<secondScanDistance;
            return first.cost<second.cost;
        });

        out << "sameScanEligiblePlots=" << sameScanPlotCount
            << " sameScanPassingSpatialGates=" << sameScanPassingCount
            << " nearestCount=" << qMin(12,neighbors.size()) << '\n';
        const int neighborCount=qMin(12,neighbors.size());
        for(int neighborIndex=0;
                neighborIndex<neighborCount;neighborIndex++)
        {
            const ScenarioDiagnosticNeighbor& neighbor=
                    neighbors.at(neighborIndex);
            const QPointF polar=neighbor.plot->getADCoord();
            out << "  NEAR rank=" << neighborIndex+1
                << " scan=" << neighbor.scanIndex
                << " scanDelta="
                << neighbor.scanIndex-basePlot.scanIndex
                << " utc=" << utcText(neighbor.plot->getTime())
                << " dtSec=" << QString::number(
                       neighbor.elapsedDifferenceSeconds,'f',6)
                << " modeA=" << plotModeAText(neighbor.plot)
                << " rangeNm=" << QString::number(
                       polar.y()/NauticalMileMeters,'f',6)
                << " azDeg=" << QString::number(polar.x(),'f',6)
                << " dRangeNm=" << QString::number(
                       neighbor.rangeDifferenceMeters/
                       NauticalMileMeters,'f',6)
                << " dAzDeg=" << QString::number(
                       neighbor.azimuthDifferenceDegrees,'f',6)
                << " dXYNm=" << QString::number(
                       neighbor.cartesianDistanceMeters/
                       NauticalMileMeters,'f',6)
                << " passRange="
                << (neighbor.rangeDifferenceMeters<=
                    ScenarioRangeGateMeters ? "yes" : "no")
                << " passAz="
                << (neighbor.azimuthDifferenceDegrees<=
                    ScenarioAzimuthGateDegrees ? "yes" : "no")
                << '\n';
        }
    }

    file.close();
    return logPath;
}

void setProbabilityItem(QTableWidgetItem *item,double probability)
{
    item->setText(QString::number(probability,'f',2));
    item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    if(probability<30.0)
        item->setBackground(QBrush(Qt::red));
    else if(probability<96.0)
        item->setBackground(QBrush(Qt::yellow));
    else
        item->setBackground(QBrush(Qt::green));
}

void showResults(const QString& path,const QString& scenarioName,
                 const QDateTime& scenarioEpoch,
                 const QVector<ScenarioTarget>& targets,
                 const QVector<TargetStatistics>& statistics,
                 const ScenarioEpochDetection& epochDetection,
                 ScenarioAssociationMethod associationMethod)
{
    QWidget *widget=new QWidget;
    widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->setWindowTitle(QObject::tr("A/C resolution w/ scenario"));

    QVBoxLayout *layout=new QVBoxLayout(widget);
    QLabel *nameLabel=new QLabel(
                QObject::tr("Scenario: %1").arg(scenarioName),widget);
    nameLabel->setWordWrap(true);
    layout->addWidget(nameLabel);
    QLabel *fileLabel=new QLabel(
                QObject::tr("Scenario file: %1").arg(path),widget);
    fileLabel->setWordWrap(true);
    layout->addWidget(fileLabel);
    layout->addWidget(new QLabel(
            QObject::tr("Association: %1")
            .arg(associationMethodText(associationMethod)),widget));
    layout->addWidget(new QLabel(
            QObject::tr("Scenario time zero: automatically detected North "
                        "marker %1 at %2 UTC")
            .arg(epochDetection.markerIndex)
            .arg(scenarioEpoch.toUTC().toString("yyyy-MM-dd HH:mm:ss.zzz")),
            widget));

    QTableWidget *table=new QTableWidget(targets.size()+1,6,widget);
    table->setHorizontalHeaderLabels(QStringList()
            << QObject::tr("Target") << QObject::tr("Mode A")
            << QObject::tr("Exp. plots") << QObject::tr("Det. plots")
            << QObject::tr("Pd, %") << QObject::tr("Correct-A Pd, %"));
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5,QHeaderView::ResizeToContents);
    layout->addWidget(table);

    int totalExpected=0;
    int totalDetected=0;
    int totalCorrectADetected=0;
    for(int row=0;row<targets.size();row++)
    {
        const TargetStatistics& targetStatistics=statistics.at(row);
        totalExpected+=targetStatistics.expected;
        totalDetected+=targetStatistics.detected;
        totalCorrectADetected+=targetStatistics.correctADetected;

        QTableWidgetItem *targetItem=
                new QTableWidgetItem(targets.at(row).id);
        QTableWidgetItem *modeAItem=
                new QTableWidgetItem(modeAText(targets.at(row).modeA));
        QTableWidgetItem *expectedItem=
                new QTableWidgetItem(QString::number(targetStatistics.expected));
        QTableWidgetItem *detectedItem=
                new QTableWidgetItem(QString::number(targetStatistics.detected));
        QTableWidgetItem *probabilityItem=new QTableWidgetItem;
        QTableWidgetItem *correctAProbabilityItem=new QTableWidgetItem;
        modeAItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        expectedItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        detectedItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        const double probability=targetStatistics.expected ?
                    100.0*targetStatistics.detected/targetStatistics.expected :
                    0.0;
        const double correctAProbability=targetStatistics.expected ?
                    100.0*targetStatistics.correctADetected/
                    targetStatistics.expected : 0.0;
        setProbabilityItem(probabilityItem,probability);
        setProbabilityItem(correctAProbabilityItem,correctAProbability);

        table->setItem(row,0,targetItem);
        table->setItem(row,1,modeAItem);
        table->setItem(row,2,expectedItem);
        table->setItem(row,3,detectedItem);
        table->setItem(row,4,probabilityItem);
        table->setItem(row,5,correctAProbabilityItem);
    }

    const int totalRow=targets.size();
    QTableWidgetItem *totalItem=new QTableWidgetItem(QObject::tr("Total"));
    QTableWidgetItem *totalExpectedItem=
            new QTableWidgetItem(QString::number(totalExpected));
    QTableWidgetItem *totalDetectedItem=
            new QTableWidgetItem(QString::number(totalDetected));
    QTableWidgetItem *totalProbabilityItem=new QTableWidgetItem;
    QTableWidgetItem *totalCorrectAProbabilityItem=new QTableWidgetItem;
    QFont totalFont=totalItem->font();
    totalFont.setBold(true);
    totalItem->setFont(totalFont);
    totalExpectedItem->setFont(totalFont);
    totalDetectedItem->setFont(totalFont);
    totalProbabilityItem->setFont(totalFont);
    totalCorrectAProbabilityItem->setFont(totalFont);
    totalExpectedItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    totalDetectedItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    if(totalExpected)
    {
        setProbabilityItem(totalProbabilityItem,
                100.0*totalDetected/totalExpected);
        setProbabilityItem(totalCorrectAProbabilityItem,
                100.0*totalCorrectADetected/totalExpected);
    }
    table->setItem(totalRow,0,totalItem);
    table->setItem(totalRow,2,totalExpectedItem);
    table->setItem(totalRow,3,totalDetectedItem);
    table->setItem(totalRow,4,totalProbabilityItem);
    table->setItem(totalRow,5,totalCorrectAProbabilityItem);

    widget->resize(920,650);
    widget->show();
}

}

ScenarioResolutionTask::ScenarioResolutionTask(IAnalyser *analyser):
    AnalyserTask(analyser)
{
}

ScenarioResolutionTask::~ScenarioResolutionTask()
{
    delete m_overlay.data();
}

QString ScenarioResolutionTask::getName(bool firstStage) const
{
    Q_UNUSED(firstStage);
    return tr("A/C resolution w/ scenario");
}

bool ScenarioResolutionTask::execute(bool firstStage)
{
    Q_UNUSED(firstStage);

    const QString path=QFileDialog::getOpenFileName(
                0,tr("Select base tracks file"),defaultScenarioPath(),
                tr("Text files (*.txt);;All files (*)"));
    if(path.isEmpty())
        return false;

    const QString recommendedMethodText=
            tr("Global minimum-cost maximum matching (recommended)");
    const QString currentMethodText=
            tr("Current maximum-cardinality matching");
    bool methodSelected=false;
    const QString selectedMethod=QInputDialog::getItem(
                0,tr("A/C resolution w/ scenario"),
                tr("Association method:"),
                QStringList()<<recommendedMethodText<<currentMethodText,
                0,false,&methodSelected);
    if(!methodSelected)
        return false;
    const ScenarioAssociationMethod associationMethod=
            selectedMethod==currentMethodText ?
                CurrentMaximumCardinalityAssociation :
                MinimumCostMaximumCardinalityAssociation;

    QVector<ScenarioTarget> targets;
    QString scenarioName;
    QString parseMessage;
    if(!parseScenarioFile(path,analyser->getActiveMap(),targets,
                          scenarioName,parseMessage))
    {
        QMessageBox::critical(0,tr("A/C resolution w/ scenario"),parseMessage);
        return false;
    }
    if(!parseMessage.isEmpty())
        QMessageBox::warning(0,tr("A/C resolution w/ scenario"),parseMessage);

    const QList<NRadarAbstractPlot*>& allData=analyser->getAllData();
    const QVector<const NRadarMarker*> markers=northMarkers(allData);
    if(markers.size()<ScenarioEpochDetectionScanCount+2)
    {
        QMessageBox::critical(0,tr("A/C resolution w/ scenario"),
                tr("At least %1 radar 51 North markers are required for "
                   "automatic scenario-start detection")
                .arg(ScenarioEpochDetectionScanCount+2));
        return false;
    }

    const QVector<const NRadarPlot*> plots=measuredPlots(allData);
    const QVector<QVector<const NRadarPlot*> > plotsByScan=
            groupPlotsByNorthScan(markers,plots);
    analyser->setTaskProgress(0);
    const ScenarioEpochDetection epochDetection=detectScenarioEpoch(
                targets,analyser->getActiveMap(),markers,plotsByScan,
                associationMethod,analyser);
    if(epochDetection.markerIndex<0)
    {
        analyser->setTaskProgress(0,false);
        QMessageBox::critical(0,tr("A/C resolution w/ scenario"),
                tr("Could not determine the scenario start reliably. "
                   "No North-marker alignment matched enough scenario "
                   "targets."));
        return false;
    }

    const QDateTime scenarioEpoch=
            markers.at(epochDetection.markerIndex)->getTime();
    qDebug().noquote() << tr(
            "Epoch alignment: %1 sampled targets over %2 scans, "
            "%3/%4 matched, score %5 (next best %6)")
            .arg(epochDetection.sampledTargetCount)
            .arg(ScenarioEpochDetectionScanCount)
            .arg(epochDetection.matchedCount)
            .arg(epochDetection.expectedCount)
            .arg(epochDetection.meanPenalty,0,'f',4)
            .arg(epochDetection.secondBestPenalty,0,'f',4);
    qDebug().noquote() << tr(
            "Matching window: %1 NM range, %2 deg azimuth, "
            "within the same antenna scan")
            .arg(ScenarioRangeGateMeters/NauticalMileMeters,0,'f',2)
            .arg(ScenarioAzimuthGateDegrees,0,'f',2);
    qDebug().noquote() << tr("Association method: %1")
            .arg(associationMethodText(associationMethod));
    QVector<TargetStatistics> statistics(targets.size());
    QVector<QPointF> basePoints;
    QVector<QLineF> matchLines;
    QVector<ScenarioDiagnosticBasePlot> diagnosticBasePlots;
    double scenarioEndSeconds=0.0;
    foreach(const ScenarioTarget& target,targets)
        scenarioEndSeconds=qMax(scenarioEndSeconds,
                target.startSeconds+target.durationSeconds);

    const int availableScenarioScans=
            markers.size()-1-epochDetection.markerIndex;
    for(int scanIndex=epochDetection.markerIndex;
            scanIndex<markers.size()-1;scanIndex++)
    {
        const QDateTime scanStart=markers.at(scanIndex)->getTime();
        const QDateTime scanEnd=markers.at(scanIndex+1)->getTime();
        const double scanDuration=scanStart.msecsTo(scanEnd)/1000.0;
        if(scanDuration<=0.0)
            continue;

        const double scanStartElapsed=
                scenarioEpoch.msecsTo(scanStart)/1000.0;
        if(scanStartElapsed>scenarioEndSeconds)
            break;

        const QVector<ScenarioOpportunity> opportunities=
                scenarioOpportunitiesForScan(
                    targets,analyser->getActiveMap(),scanStartElapsed,
                    scanDuration);
        foreach(const ScenarioOpportunity& opportunity,opportunities)
        {
            statistics[opportunity.targetIndex].expected++;
            basePoints.append(opportunity.position);
        }

        const QVector<const NRadarPlot*>& scanPlots=
                plotsByScan.at(scanIndex);
        const AssociationResult association=associateScan(
                    opportunities,scanPlots,associationMethod);
        const QVector<int>& matches=association.plotForOpportunity;
        for(int opportunityIndex=0;
                opportunityIndex<matches.size();opportunityIndex++)
        {
            const int matchedPlotIndex=matches.at(opportunityIndex);
            const ScenarioOpportunity& opportunity=
                    opportunities.at(opportunityIndex);
            if(ScenarioDiagnosticLogging &&
                    targets.at(opportunity.targetIndex).modeA==
                    ScenarioDiagnosticModeA)
            {
                ScenarioDiagnosticBasePlot diagnosticBasePlot;
                diagnosticBasePlot.opportunity=opportunity;
                diagnosticBasePlot.scanIndex=scanIndex;
                diagnosticBasePlot.scanStart=scanStart;
                diagnosticBasePlot.scanEnd=scanEnd;
                diagnosticBasePlot.matchedPlot=matchedPlotIndex>=0 ?
                            scanPlots.at(matchedPlotIndex) : 0;
                diagnosticBasePlots.append(diagnosticBasePlot);
            }

            if(matchedPlotIndex<0)
                continue;

            const NRadarPlot *matchedPlot=scanPlots.at(matchedPlotIndex);
            statistics[opportunity.targetIndex].detected++;
            if(matchedPlot->hasBoardNumber() &&
                    matchedPlot->getBoardNumber()==
                    targets.at(opportunity.targetIndex).modeA)
                statistics[opportunity.targetIndex].correctADetected++;
            matchLines.append(QLineF(opportunity.position,
                    matchedPlot->getXYCoord()));
        }

        if(scanIndex%2==0)
            analyser->setTaskProgress(
                    45+int(qint64(scanIndex-
                                  epochDetection.markerIndex+1)*55/
                           availableScenarioScans));
    }
    analyser->setTaskProgress(0,false);

    delete m_overlay.data();
    m_overlay=new ScenarioResolutionOverlay(basePoints,matchLines);
    analyser->getRadarScene()->addItem(m_overlay.data());

    QString diagnosticPath;
    if(ScenarioDiagnosticLogging)
    {
        diagnosticPath=writeScenarioAssociationDiagnostics(
                    path,ScenarioDiagnosticModeA,scenarioEpoch,
                    analyser->getActiveMap(),markers,plots,allData,targets,
                    diagnosticBasePlots);
        if(!diagnosticPath.isEmpty())
            qDebug().noquote() << tr("Mode A %1 diagnostic log: %2")
                    .arg(modeAText(ScenarioDiagnosticModeA))
                    .arg(diagnosticPath);
    }
    showResults(path,scenarioName,scenarioEpoch,targets,statistics,
                epochDetection,associationMethod);
    emit finished(true);
    return true;
}

#include <QtCore>

#include <asterix/asterix.h>
#include <nradarmap.h>
#include <radardata/nasterixconverter.h>
#include <radardata/nradarmarker.h>
#include <radardata/nradarplot.h>
#include <radardata/nradartrackplot.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>

namespace
{

const qint64 TrackEndInactivityMs=30000;
const qint64 NorthDuplicateWindowMs=1000;
const double TrackPositionToleranceMeters=5000.0;
const double TrackMinimumPlausibleSpeedMps=350.0;
const double TrackMaximumPlausibleSpeedMps=1200.0;

struct TrackInstance
{
    TrackInstance():trackKey(0),normalPoints(0),predictedPoints(0),
        psrPoints(0),ssrPoints(0),combinedPoints(0),modeSPoints(0),
        explicitEnd(false) {}

    quint64 trackKey;
    int normalPoints;
    int predictedPoints;
    int psrPoints;
    int ssrPoints;
    int combinedPoints;
    int modeSPoints;
    bool explicitEnd;
    QDateTime firstPointTime;
    QDateTime lastPointTime;
    QDateTime explicitEndTime;
};

struct Recording
{
    QList<QSharedPointer<NRadarAbstractPlot> > data;
    QMap<int,qint64> categoryCounts;
    QByteArray userHeader;
    qint64 recordCount=0;
    qint64 decodeErrors=0;
    QDateTime firstRecordTime;
    QDateTime lastRecordTime;
};

QString utcText(const QDateTime& time)
{
    return time.toUTC().toString("yyyy-MM-dd HH:mm:ss.zzz'Z'");
}

QDateTime parseDateTime(QString value)
{
    QDateTime result=QDateTime::fromString(value,Qt::ISODateWithMs);
    if(!result.isValid())
        result=QDateTime::fromString(value,Qt::ISODate);
    if(result.isValid() && result.timeSpec()==Qt::LocalTime &&
            !value.endsWith('Z') && !value.contains('+'))
        result.setTimeSpec(Qt::UTC);
    return result.toUTC();
}

bool hasConflictingAircraftAddress(const NRadarTrackPlot *previous,
                                   const NRadarTrackPlot *current)
{
    const QVariant previousValue=previous->getOption(NRadarPlot::AircraftAddress);
    const QVariant currentValue=current->getOption(NRadarPlot::AircraftAddress);
    if(previousValue.isNull() || currentValue.isNull())
        return false;

    const uint previousAddress=previousValue.toUInt();
    const uint currentAddress=currentValue.toUInt();
    return previousAddress && currentAddress && previousAddress!=currentAddress;
}

bool isContinuousTrack(const NRadarTrackPlot *previous,
                       const NRadarTrackPlot *current)
{
    const qint64 gapMs=previous->getTime().msecsTo(current->getTime());
    if(gapMs<0 || gapMs>TrackEndInactivityMs)
        return false;
    if((previous->getSource()==NRadarPlot::ADSB)!=
            (current->getSource()==NRadarPlot::ADSB))
        return false;
    if(hasConflictingAircraftAddress(previous,current))
        return false;

    const double reportedSpeedMps=qMax(previous->getSpeed(),current->getSpeed())/3.6;
    const double plausibleSpeedMps=qBound(TrackMinimumPlausibleSpeedMps,
                                          reportedSpeedMps*3.0,
                                          TrackMaximumPlausibleSpeedMps);
    const double allowedDistance=TrackPositionToleranceMeters+
            plausibleSpeedMps*(gapMs/1000.0);
    return QLineF(previous->getXYCoord(),current->getXYCoord()).length()<=allowedDistance;
}

int scanIndexForTime(const QVector<QDateTime>& markers,const QDateTime& time)
{
    if(markers.size()<2 || time<markers.first() || time>=markers.last())
        return -1;

    int first=0;
    int last=markers.size()-1;
    while(first+1<last)
    {
        const int middle=(first+last)/2;
        if(markers.at(middle)<=time)
            first=middle;
        else
            last=middle;
    }
    return first;
}

QVector<QDateTime> physicalNorthMarkers(QVector<QDateTime> markers)
{
    std::sort(markers.begin(),markers.end());
    QVector<QDateTime> result;
    result.reserve(markers.size());
    foreach(const QDateTime& marker,markers)
        if(result.isEmpty() ||
                result.last().msecsTo(marker)>=NorthDuplicateWindowMs)
            result.append(marker);
    return result;
}

bool decodePacket(const QByteArray& bytes,const QDateTime& recordTime,
                  NAsterixConverter& converter,Recording& recording)
{
    if(bytes.isEmpty())
        return false;

    const quint8 *buffer=reinterpret_cast<const quint8*>(bytes.constData());
    int error=0;
    QSharedPointer<NRadarAbstractPlot> plot;
    switch(buffer[0])
    {
    case 34:
    {
        Asterix_34 packet;
        if(packet.read(buffer,bytes.size(),&error) && !error)
            plot=converter.convert(&packet,-1,recordTime.date());
        break;
    }
    case 48:
    {
        Asterix_48 packet;
        if(packet.read(buffer,bytes.size(),&error) && !error)
            plot=converter.convert(&packet,-1,recordTime.date());
        break;
    }
    case 62:
    {
        Asterix_62 packet;
        if(packet.read(buffer,bytes.size(),&error) && !error)
            plot=converter.convert(&packet,-1,recordTime.date());
        break;
    }
    case 21:
    {
        Asterix_21_13 packet13;
        Asterix_21_023 packet023;
        int error13=0;
        int error023=0;
        if(packet13.read(buffer,bytes.size(),&error13) && !error13)
            plot=converter.convert(&packet13,-1,recordTime.date());
        else if(packet023.read(buffer,bytes.size(),&error023) && !error023)
            plot=converter.convert(&packet023,-1,recordTime.date());
        error=error13 && error023;
        break;
    }
    default:
        break;
    }

    recording.categoryCounts[buffer[0]]++;
    if(error)
        recording.decodeErrors++;
    if(!plot.isNull())
        recording.data.append(plot);
    return true;
}

bool readRdb(const QString& path,NAsterixConverter& converter,
             Recording& recording)
{
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return false;

    const QByteArray mark=file.read(4);
    if(mark.size()!=4 || mark.left(3)!="rdb" || mark.at(3)!='2')
        return false;

    char headerSize=0;
    if(!file.getChar(&headerSize))
        return false;
    recording.userHeader=file.read(quint8(headerSize));

    while(!file.atEnd())
    {
        const QByteArray blockHeader=file.read(16);
        if(blockHeader.isEmpty())
            break;
        if(blockHeader.size()!=16 || quint8(blockHeader.at(0))!=0xff ||
                quint8(blockHeader.at(1))!=0xff)
            return false;

        const uchar *header=
                reinterpret_cast<const uchar*>(blockHeader.constData());
        const int blockLength=qFromLittleEndian<int>(header+2);
        if(blockLength<24)
            return false;

        const QByteArray compressed=file.read(blockLength-24);
        if(compressed.size()!=blockLength-24 || file.read(8).size()!=8)
            return false;
        const QByteArray block=qUncompress(compressed);
        if(block.isEmpty() && !compressed.isEmpty())
            return false;

        int position=0;
        while(position<block.size())
        {
            if(position+12>block.size())
                return false;
            const uchar *record=reinterpret_cast<const uchar*>(
                        block.constData()+position);
            const int recordLength=qFromLittleEndian<int>(record);
            const qint64 milliseconds=qFromLittleEndian<qint64>(record+4);
            if(recordLength<12 || position+recordLength>block.size())
                return false;

            const QDateTime recordTime=
                    QDateTime::fromMSecsSinceEpoch(milliseconds).toUTC();
            if(!recording.firstRecordTime.isValid())
                recording.firstRecordTime=recordTime;
            recording.lastRecordTime=recordTime;
            decodePacket(QByteArray(block.constData()+position+12,
                                    recordLength-12),
                         recordTime,converter,recording);
            recording.recordCount++;
            position+=recordLength;
        }
    }
    return true;
}

QVector<TrackInstance> buildTrackInstances(const Recording& recording)
{
    QMap<quint64,QList<const NRadarTrackPlot*> > tracks;
    foreach(const QSharedPointer<NRadarAbstractPlot>& item,recording.data)
    {
        if(item->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;
        const NRadarTrackPlot *track=
                static_cast<const NRadarTrackPlot*>(item.data());
        const quint64 key=(quint64(track->getRadarId())<<32)|track->getTrackId();
        tracks[key].append(track);
    }

    QVector<TrackInstance> instances;
    QMapIterator<quint64,QList<const NRadarTrackPlot*> > group(tracks);
    while(group.hasNext())
    {
        group.next();
        QList<const NRadarTrackPlot*> samples=group.value();
        std::stable_sort(samples.begin(),samples.end(),
                         [](const NRadarTrackPlot *left,
                            const NRadarTrackPlot *right)
        {
            return left->getTime()<right->getTime();
        });

        const NRadarTrackPlot *previous=0;
        int instanceIndex=-1;
        foreach(const NRadarTrackPlot *sample,samples)
        {
            if(instanceIndex<0 ||
                    (previous && !isContinuousTrack(previous,sample)))
            {
                TrackInstance instance;
                instance.trackKey=group.key();
                instances.append(instance);
                instanceIndex=instances.size()-1;
            }

            TrackInstance& instance=instances[instanceIndex];
            if(sample->getTrackPlotType()==NRadarTrackPlot::EndPoint)
            {
                instance.explicitEnd=true;
                instance.explicitEndTime=sample->getTime();
            }
            else
            {
                if(!instance.firstPointTime.isValid() ||
                        sample->getTime()<instance.firstPointTime)
                    instance.firstPointTime=sample->getTime();
                if(!instance.lastPointTime.isValid() ||
                        instance.lastPointTime<sample->getTime())
                    instance.lastPointTime=sample->getTime();

                if(sample->getTrackPlotType()==NRadarTrackPlot::PredictedPoint)
                    instance.predictedPoints++;
                else
                    instance.normalPoints++;
                switch(sample->getSource())
                {
                case NRadarPlot::PSR: instance.psrPoints++; break;
                case NRadarPlot::SSR: instance.ssrPoints++; break;
                case NRadarPlot::Combined: instance.combinedPoints++; break;
                default: break;
                }
                if(sample->getSSRType()==NRadarPlot::ModeS)
                    instance.modeSPoints++;
            }

            previous=sample;
            if(sample->getTrackPlotType()==NRadarTrackPlot::EndPoint)
            {
                previous=0;
                instanceIndex=-1;
            }
        }
    }
    return instances;
}

QMap<quint64,QDateTime> latestPointsByTrackKey(
        const QVector<TrackInstance>& instances)
{
    QMap<quint64,QDateTime> latest;
    foreach(const TrackInstance& instance,instances)
        if(instance.lastPointTime.isValid() &&
                (!latest.contains(instance.trackKey) ||
                 latest.value(instance.trackKey)<instance.lastPointTime))
            latest[instance.trackKey]=instance.lastPointTime;
    return latest;
}

bool isEnded(const TrackInstance& instance,
             const QMap<quint64,QDateTime>& latest,
             const QDateTime& availableDataEnd)
{
    return instance.explicitEnd ||
            instance.lastPointTime<latest.value(instance.trackKey) ||
            (instance.lastPointTime.isValid() &&
             instance.lastPointTime.msecsTo(availableDataEnd)>
             TrackEndInactivityMs);
}

void printMap(const char *prefix,const QMap<int,qint64>& values)
{
    QMapIterator<int,qint64> value(values);
    while(value.hasNext())
    {
        value.next();
        std::cout << prefix << value.key() << '=' << value.value() << '\n';
    }
}

void printMarkerSummary(int radarId,const QVector<QDateTime>& rawMarkers)
{
    QVector<QDateTime> sorted=rawMarkers;
    std::sort(sorted.begin(),sorted.end());
    QVector<QDateTime> exactUnique=sorted;
    exactUnique.erase(std::unique(exactUnique.begin(),exactUnique.end()),
                      exactUnique.end());
    const QVector<QDateTime> physical=physicalNorthMarkers(exactUnique);

    QMap<qint64,int> roundedGaps;
    qint64 minimumGap=std::numeric_limits<qint64>::max();
    qint64 maximumGap=0;
    for(int i=1;i<exactUnique.size();i++)
    {
        const qint64 gap=exactUnique.at(i-1).msecsTo(exactUnique.at(i));
        roundedGaps[qRound(gap/10.0)*10]++;
        minimumGap=qMin(minimumGap,gap);
        maximumGap=qMax(maximumGap,gap);
    }

    std::cout << "north_radar_" << radarId
              << ": raw=" << sorted.size()
              << " exact_unique=" << exactUnique.size()
              << " exact_duplicates=" << sorted.size()-exactUnique.size()
              << " physical=" << physical.size()
              << " near_duplicates=" << exactUnique.size()-physical.size();
    if(!physical.isEmpty())
        std::cout << " range=" << qPrintable(utcText(physical.first()))
                  << " .. " << qPrintable(utcText(physical.last()));
    if(exactUnique.size()>1)
        std::cout << " min_raw_unique_gap_ms=" << minimumGap
                  << " max_raw_unique_gap_ms=" << maximumGap;
    std::cout << '\n';

    QList<QPair<int,qint64> > commonGaps;
    QMapIterator<qint64,int> gap(roundedGaps);
    while(gap.hasNext())
    {
        gap.next();
        commonGaps.append(qMakePair(gap.value(),gap.key()));
    }
    std::sort(commonGaps.begin(),commonGaps.end(),
              std::greater<QPair<int,qint64> >());
    std::cout << "north_radar_" << radarId << "_common_gaps_ms=";
    for(int i=0;i<qMin(8,commonGaps.size());i++)
        std::cout << (i ? "," : "") << commonGaps.at(i).second
                  << '(' << commonGaps.at(i).first << ')';
    std::cout << '\n';
}

void printFalseSummary(const char *sourceName,bool ssr,int maximumPoints,
                       const QVector<TrackInstance>& instances,
                       const QMap<quint64,QDateTime>& latest,
                       const QDateTime& availableDataEnd,
                       const QVector<QDateTime>& markers)
{
    QVector<int> counts(qMax(0,markers.size()-1),0);
    int shortInstances=0;
    int endedShortInstances=0;
    int outsideScanRange=0;
    foreach(const TrackInstance& instance,instances)
    {
        const int points=ssr ? instance.ssrPoints+instance.combinedPoints :
                               instance.psrPoints;
        if(points<=0 || points>maximumPoints)
            continue;
        shortInstances++;
        if(!isEnded(instance,latest,availableDataEnd))
            continue;
        endedShortInstances++;

        const QDateTime endTime=instance.explicitEnd ?
                    instance.explicitEndTime : instance.lastPointTime;
        const int scanIndex=scanIndexForTime(markers,endTime);
        if(scanIndex>=0)
            counts[scanIndex]++;
        else
            outsideScanRange++;
    }

    int total=0;
    int nonzeroScans=0;
    int maximumPerScan=0;
    foreach(int count,counts)
    {
        total+=count;
        if(count) nonzeroScans++;
        maximumPerScan=qMax(maximumPerScan,count);
    }
    std::cout << sourceName << " <= " << maximumPoints
              << ": short_instances=" << shortInstances
              << " ended_short_instances=" << endedShortInstances
              << " total_in_complete_scans=" << total
              << " nonzero_scans=" << nonzeroScans
              << " maximum_per_scan=" << maximumPerScan
              << " outside_complete_scans=" << outsideScanRange << '\n';
}

QList<int> parseThresholds(const QString& text)
{
    QList<int> result;
    foreach(const QString& part,text.split(',',Qt::SkipEmptyParts))
    {
        bool ok=false;
        const int value=part.trimmed().toInt(&ok);
        if(ok && value>0 && !result.contains(value))
            result.append(value);
    }
    std::sort(result.begin(),result.end());
    return result;
}

}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName("rdb_false_tracks_check");

    QCommandLineParser parser;
    parser.setApplicationDescription(
                "Parse an Analyser RDB and audit North markers/false tracks.");
    parser.addHelpOption();
    parser.addPositionalArgument("rdb","RDB recording to inspect.");
    parser.addOption(QCommandLineOption("thresholds",
                                        "Comma-separated point limits.",
                                        "limits","3,10"));
    parser.addOption(QCommandLineOption("begin",
                                        "Analysis interval start (ISO date/time).",
                                        "datetime"));
    parser.addOption(QCommandLineOption("end",
                                        "Analysis interval end (ISO date/time).",
                                        "datetime"));
    parser.process(app);

    if(parser.positionalArguments().size()!=1)
        parser.showHelp(2);
    const QList<int> thresholds=parseThresholds(parser.value("thresholds"));
    if(thresholds.isEmpty())
    {
        std::cerr << "No valid positive thresholds were supplied.\n";
        return 2;
    }

    QScopedPointer<NRadarMap> map(NRadarMap::createMap(60,30,650000));
    NAsterixConverter converter(map.data());
    Recording recording;
    if(!readRdb(parser.positionalArguments().first(),converter,recording))
    {
        std::cerr << "Failed to parse the RDB file.\n";
        return 1;
    }

    QDateTime begin=recording.firstRecordTime;
    QDateTime end=recording.lastRecordTime;
    if(parser.isSet("begin")) begin=parseDateTime(parser.value("begin"));
    if(parser.isSet("end")) end=parseDateTime(parser.value("end"));
    if(!begin.isValid() || !end.isValid() || begin>=end)
    {
        std::cerr << "Invalid Begin/End interval.\n";
        return 2;
    }

    QMap<int,qint64> radarCounts;
    QMap<int,qint64> sourceCounts;
    QMap<int,qint64> trackTypeCounts;
    QMap<int,QVector<QDateTime> > allNorthMarkers;
    foreach(const QSharedPointer<NRadarAbstractPlot>& item,recording.data)
    {
        radarCounts[item->getRadarId()]++;
        if(item->getType()==NRadarAbstractPlot::TypeMarker)
        {
            const NRadarMarker *marker=
                    static_cast<const NRadarMarker*>(item.data());
            if(marker->isNorthMarker())
                allNorthMarkers[item->getRadarId()].append(marker->getTime());
        }
        else if(item->getType()==NRadarAbstractPlot::TypePlot ||
                item->getType()==NRadarAbstractPlot::TypeTrack)
        {
            const NRadarPlot *plot=static_cast<const NRadarPlot*>(item.data());
            sourceCounts[plot->getSource()]++;
            if(item->getType()==NRadarAbstractPlot::TypeTrack)
                trackTypeCounts[static_cast<const NRadarTrackPlot*>(item.data())
                        ->getTrackPlotType()]++;
        }
    }

    std::cout << "header=" << recording.userHeader.constData() << '\n'
              << "record_range=" << qPrintable(utcText(recording.firstRecordTime))
              << " .. " << qPrintable(utcText(recording.lastRecordTime)) << '\n'
              << "analysis_range=" << qPrintable(utcText(begin))
              << " .. " << qPrintable(utcText(end)) << '\n'
              << "rdb_records=" << recording.recordCount
              << " decoded_plots=" << recording.data.size()
              << " decode_errors=" << recording.decodeErrors << '\n';
    printMap("category_",recording.categoryCounts);
    printMap("radar_",radarCounts);
    printMap("source_",sourceCounts);
    printMap("track_type_",trackTypeCounts);

    QMapIterator<int,QVector<QDateTime> > markerGroup(allNorthMarkers);
    while(markerGroup.hasNext())
    {
        markerGroup.next();
        printMarkerSummary(markerGroup.key(),markerGroup.value());
    }

    QVector<QDateTime> psrMarkers;
    QVector<QDateTime> ssrMarkers;
    foreach(const QDateTime& time,allNorthMarkers.value(51))
        if(time>=begin && time<=end) psrMarkers.append(time);
    foreach(const QDateTime& time,allNorthMarkers.value(50))
        if(time>=begin && time<=end) ssrMarkers.append(time);
    psrMarkers=physicalNorthMarkers(psrMarkers);
    ssrMarkers=physicalNorthMarkers(ssrMarkers);

    const QVector<TrackInstance> instances=buildTrackInstances(recording);
    const QMap<quint64,QDateTime> latest=latestPointsByTrackKey(instances);
    int endedInstances=0;
    foreach(const TrackInstance& instance,instances)
        if(isEnded(instance,latest,recording.lastRecordTime))
            endedInstances++;
    std::cout << "track_instances=" << instances.size()
              << " ended_instances=" << endedInstances << '\n'
              << "psr_complete_scans=" << qMax(0,psrMarkers.size()-1) << '\n'
              << "ssr_complete_scans=" << qMax(0,ssrMarkers.size()-1) << '\n';

    foreach(int threshold,thresholds)
    {
        printFalseSummary("PSR",false,threshold,instances,latest,
                          recording.lastRecordTime,psrMarkers);
        printFalseSummary("SSR",true,threshold,instances,latest,
                          recording.lastRecordTime,ssrMarkers);
    }
    return 0;
}

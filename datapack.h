#ifndef DATAPACK_H
#define DATAPACK_H

#include <QtCore>
#include <libradardata/nradarabstractplot.h>
#include "prp3trackerevidence.h"

struct Prp3PlotRecord;

class DataPack
{
public:

    typedef QList<NRadarAbstractPlot*> PlotList;

    DataPack();
    ~DataPack();

    void clear();

    const PlotList& getData() const;
    QSharedPointer<const Prp3PlotRecord> getPrp3Record(const NRadarAbstractPlot *plot) const;
    bool hasPrp3Data() const;
    QVector<Prp3TrackerUse> trackerUses(const NRadarAbstractPlot *plot) const;
    const QSharedPointer<Prp3TrackerRecording> &trackerRecording() const { return m_trackerRecording; }

    QPointF getCenter() const;
    QDateTime getBeginDate() const;
    QDateTime getEndDate() const;

protected:
    PlotList data;
    QList<QSharedPointer<NRadarAbstractPlot>> savedData;
    QHash<const NRadarAbstractPlot *, QSharedPointer<Prp3PlotRecord>> prp3Data;
    QHash<const NRadarAbstractPlot *, QVector<Prp3TrackerUse>> m_trackerUses;
    QSharedPointer<Prp3TrackerRecording> m_trackerRecording;

    QDateTime begin;
    QDateTime end;

    QPointF center;

    friend class DataSourceDlg;
};

#endif // DATAPACK_H

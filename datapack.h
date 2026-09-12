#ifndef DATAPACK_H
#define DATAPACK_H

#include <QtCore>
#include <libradardata/nradarabstractplot.h>

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

    QPointF getCenter() const;
    QDateTime getBeginDate() const;
    QDateTime getEndDate() const;

protected:
    PlotList data;
    QList<QSharedPointer<NRadarAbstractPlot>> savedData;
    QHash<const NRadarAbstractPlot *, QSharedPointer<Prp3PlotRecord>> prp3Data;

    QDateTime begin;
    QDateTime end;

    QPointF center;

    friend class DataSourceDlg;
};

#endif // DATAPACK_H

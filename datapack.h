#ifndef DATAPACK_H
#define DATAPACK_H

#include <QtCore>
#include <radardata/nradarabstractplot.h>

class DataPack
{
public:

    typedef QList<NRadarAbstractPlot*> PlotList;

    DataPack();
    ~DataPack();

    void clear();

    const PlotList& getData() const;

    QPointF getCenter() const;
    QDateTime getBeginDate() const;
    QDateTime getEndDate() const;

protected:
    PlotList data;
    QList<QSharedPointer<NRadarAbstractPlot>> savedData;

    QDateTime begin;
    QDateTime end;

    QPointF center;

    friend class DataSourceDlg;
};

#endif // DATAPACK_H

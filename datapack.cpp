#include "datapack.h"
#include "prp3record.h"

DataPack::DataPack()
{
}

DataPack::~DataPack()
{
    clear();
}

void DataPack::clear()
{
    data.clear();
    prp3Data.clear();
    savedData.clear();
    begin = end = QDateTime();
    center = QPointF();
}

const DataPack::PlotList& DataPack::getData() const
{ return data; }

QSharedPointer<const Prp3PlotRecord> DataPack::getPrp3Record(const NRadarAbstractPlot *plot) const
{ return prp3Data.value(plot); }

bool DataPack::hasPrp3Data() const
{ return !prp3Data.isEmpty(); }

QPointF DataPack::getCenter() const
{ return center; }

QDateTime DataPack::getBeginDate() const
{ return begin; }

QDateTime DataPack::getEndDate() const
{ return end; }

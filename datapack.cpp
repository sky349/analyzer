#include "datapack.h"

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
    begin=end=QDateTime();
    center=QPointF();
}

const DataPack::PlotList& DataPack::getData() const
{ return data; }

QPointF DataPack::getCenter() const
{ return center; }

QDateTime DataPack::getBeginDate() const
{ return begin; }

QDateTime DataPack::getEndDate() const
{ return end; }

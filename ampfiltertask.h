#pragma once

//main interface header, read it first!
#include "ianalyser.h"
#include <radarview/nradarobjectitem.h>

class AmpFilterTask:public AnalyserTask
{
public:
    //IAnalyser holds all the data and provides QGraphicsScene for drawing
    AmpFilterTask(IAnalyser *analyser);

    TaskType getType() const {return SingleStage; }
    QString getName(bool firstStage) const;

    bool execute(bool firstStage);

private:
    QElapsedTimer lastWorkingUpdate;
    quint16 *amplLimits;
    int amplMapSize;
    double scale=1.;
};


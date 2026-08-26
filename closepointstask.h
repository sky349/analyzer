#pragma once

#include <QPointer>

#include "ianalyser.h"

class QGraphicsObject;

class CloseTrackPointsTask:public AnalyserTask
{
public:
    explicit CloseTrackPointsTask(IAnalyser *analyser);
    ~CloseTrackPointsTask();

    TaskType getType() const { return TwoStages; }
    QString getName(bool firstStage) const;
    bool execute(bool firstStage);

private:
    double rangeGateMeters;
    double azimuthGateDegrees;
    int minimumTrackLength;
    QPointer<QGraphicsObject> overlay;
};

#pragma once

#include <QPointer>

#include "ianalyser.h"

class QGraphicsObject;

class ClosePSRPlotsTask:public AnalyserTask
{
public:
    explicit ClosePSRPlotsTask(IAnalyser *analyser);
    ~ClosePSRPlotsTask();

    TaskType getType() const { return SingleStage; }
    QString getName(bool firstStage) const;
    bool execute(bool firstStage);

private:
    double rangeGateMeters;
    double azimuthGateDegrees;
    QPointer<QGraphicsObject> overlay;
};

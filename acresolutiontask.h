#pragma once

#include "ianalyser.h"

class ACResolutionTask:public AnalyserTask
{
public:
    explicit ACResolutionTask(IAnalyser *analyser);

    TaskType getType() const { return SingleStage; }
    QString getName(bool firstStage) const;
    bool execute(bool firstStage);
};

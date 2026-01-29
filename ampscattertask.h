#pragma once

#include "ianalyser.h"

class ampScatterTask:public AnalyserTask
{
public:
    ampScatterTask(IAnalyser *analyser);

    TaskType getType() const {return SingleStage; }
    QString getName(bool firstStage) const;

    bool execute(bool firstStage);
};

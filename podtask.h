#ifndef PODTASK_H
#define PODTASK_H

#include "ianalyser.h"

class PoDTask:public AnalyserTask
{
Q_OBJECT

public:
    PoDTask(IAnalyser *analyser);

    TaskType getType() const {return SingleStage; }
    QString getName(bool) const;
    bool execute(bool);

protected:
    QTime lastWorkingUpdate;
};

#endif // PODTASK_H

#pragma once

#include <QPointer>

#include "ianalyser.h"

class QGraphicsObject;

class ScenarioResolutionTask:public AnalyserTask
{
public:
    explicit ScenarioResolutionTask(IAnalyser *analyser);
    ~ScenarioResolutionTask();

    TaskType getType() const { return SingleStage; }
    QString getName(bool firstStage) const;
    bool execute(bool firstStage);

private:
    QPointer<QGraphicsObject> m_overlay;
};

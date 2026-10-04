#pragma once

#include "ianalyser.h"

#include <functional>
#include <utility>

// Checkable toggle: PRP3 PSR plots take the motion-class (MTI) palette instead of the restored-Doppler palette. The
// host owns the colouring so plots recreated by later filter changes keep the selected palette.
class MtiLabelsTask : public AnalyserTask
{
public:
    MtiLabelsTask(IAnalyser *analyser, std::function<void(bool)> apply)
        : AnalyserTask(analyser), m_apply(std::move(apply))
    {
    }

    TaskType getType() const override { return TwoStages; }
    QString getName(bool) const override { return tr("MTI labels"); }
    bool execute(bool on) override
    {
        m_apply(on);
        return true;
    }

private:
    std::function<void(bool)> m_apply;
};

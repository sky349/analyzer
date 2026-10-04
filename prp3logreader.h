#pragma once

#include "prp3record.h"
#include "prp3trackerevidence.h"

#include <QDateTime>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

#include <functional>

class Prp3LogReader
{
public:
    enum class Status { SUCCESS, CANCELLED, ERROR };

    struct Result
    {
        QVector<QSharedPointer<Prp3PlotRecord>> plots;
        int eventCount = 0;
        QDateTime begin;
        QDateTime end;
        QSharedPointer<Prp3TrackerRecording> tracker = QSharedPointer<Prp3TrackerRecording>::create();
    };

    using Progress = std::function<bool(qint64, qint64)>;

    Status read(const QStringList &filePaths, Result &result, QString &error,
                const Progress &progress = Progress()) const;
};

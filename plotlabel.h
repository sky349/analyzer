#pragma once

#include <QTreeWidget>
#include <QVector>

class PlotLabel : public QTreeWidget
{
public:
    explicit PlotLabel(const QTreeWidget *styleSource, QWidget *parent = nullptr);

    void setEntries(const QVector<QString> &entries);

private:
    void applyTreeStyle(const QTreeWidget *styleSource);
};

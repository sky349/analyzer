#ifndef PLOTLABEL_H
#define PLOTLABEL_H

#include <QPair>
#include <QTreeWidget>
#include <QVector>

class PlotLabel : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PlotLabel(const QTreeWidget *styleSource, QWidget *parent = 0);

    void setEntries(const QVector<QPair<QString, QString> > &entries);

private:
    void applyTreeStyle(const QTreeWidget *styleSource);
};

#endif // PLOTLABEL_H

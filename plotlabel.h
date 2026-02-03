#ifndef PLOTLABEL_H
#define PLOTLABEL_H

#include <QFrame>
#include <QPair>
#include <QVector>

class QTableWidget;
class QTreeWidget;

class PlotLabel : public QFrame
{
    Q_OBJECT

public:
    explicit PlotLabel(const QTreeWidget *styleSource, QWidget *parent = 0);

    void setEntries(const QVector<QPair<QString, QString> > &entries);

private:
    void applyTreeStyle(const QTreeWidget *styleSource);

    QTableWidget *m_table;
};

#endif // PLOTLABEL_H

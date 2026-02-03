#include "plotlabel.h"

#include <QHeaderView>
#include <QPalette>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

PlotLabel::PlotLabel(const QTreeWidget *styleSource, QWidget *parent)
    : QFrame(parent),
      m_table(0)
{
    setFrameShape(QFrame::Box);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(255, 255, 255, 220));
    setPalette(pal);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_table = new QTableWidget(3, 2, this);
    applyTreeStyle(styleSource);

    m_table->horizontalHeader()->setVisible(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);

    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    layout->addWidget(m_table);

    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_table->setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void PlotLabel::applyTreeStyle(const QTreeWidget *styleSource)
{
    if(!styleSource || !m_table)
        return;

    m_table->setStyle(styleSource->style());
    m_table->setFont(styleSource->font());
    m_table->setPalette(styleSource->palette());
    m_table->setStyleSheet(styleSource->styleSheet());
    m_table->setFrameShape(styleSource->frameShape());
    m_table->setFrameShadow(styleSource->frameShadow());
    m_table->setLineWidth(styleSource->lineWidth());
    m_table->setMidLineWidth(styleSource->midLineWidth());
    m_table->setAlternatingRowColors(styleSource->alternatingRowColors());
}

void PlotLabel::setEntries(const QVector<QPair<QString, QString> > &entries)
{
    if(!m_table)
        return;

    m_table->setRowCount(entries.size());
    m_table->setColumnCount(2);

    for(int i = 0; i < entries.size(); ++i)
    {
        m_table->setItem(i, 0, new QTableWidgetItem(entries[i].first));
        m_table->setItem(i, 1, new QTableWidgetItem(entries[i].second));
    }

    m_table->resizeColumnsToContents();
    m_table->resizeRowsToContents();
}

#pragma once

#include "prp3trackerevidence.h"

#include <QColor>
#include <QDockWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTableWidget;
class QTreeWidget;
class TrackerEvidenceChart;

class TrackerEvidencePanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit TrackerEvidencePanel(QWidget *parent);
    void setRecording(QSharedPointer<const Prp3TrackerRecording> recording);
    void inspect(QVector<Prp3TrackerUse> uses);
    bool accepts(const QVector<Prp3TrackerUse> &uses) const;
    QColor colour(const QVector<Prp3TrackerUse> &uses) const;
    bool coloursEnabled() const;
    QString summary(const QVector<Prp3TrackerUse> &uses) const;

signals:
    void displayChanged();

private:
    Prp3EvidenceScope scope() const;
    QSharedPointer<const Prp3TrackerSample> selected(const QVector<Prp3TrackerUse> &uses) const;
    void updateInspection();
    void showSample(const QSharedPointer<const Prp3TrackerSample> &sample);
    QComboBox *m_metric, *m_scope, *m_table, *m_availability, *m_context;
    QCheckBox *m_colours, *m_range;
    QDoubleSpinBox *m_minimum, *m_maximum;
    QLabel *m_summary;
    QTreeWidget *m_details;
    QTableWidget *m_history;
    TrackerEvidenceChart *m_chart;
    QSharedPointer<const Prp3TrackerRecording> m_recording;
    QVector<Prp3TrackerUse> m_uses;
};

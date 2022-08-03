#ifndef DATAPATH_H
#define DATAPATH_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>

#include "qt/experimentcontrol_model.h"

class DataPage : public QWidget {
    Q_OBJECT
public:
    explicit DataPage(QWidget *parent = nullptr);
    void setExperimentControlModel(ExperimentControlModel *model);

public:
    ExperimentControlModel *model;

    QPushButton *refresh_button;
    QTableWidget *table;

    void refreshContent();
};

#endif // SAMPLEPAGE_H

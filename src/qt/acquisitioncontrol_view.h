#ifndef ACQUISITIONCONTROL_VIEW_H
#define ACQUISITIONCONTROL_VIEW_H

#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include "qt/experimentcontrol_model.h"

class AcquisitionControlView : public QWidget {
    Q_OBJECT
public:
    explicit AcquisitionControlView(QWidget *parent = nullptr);
    void setModel(ExperimentControlModel *model);

    void setControlButtonEnabled(bool enabled);

    void setChannel(QString channel_name);
    void setState(QString state);
    void setMessage(QString message) { messageLabel->setText(message); }

    void toggleLiveView();

signals:
    void liveViewStarted();

public:
    ExperimentControlModel *model;

    QPushButton *buttonLiveOrStop;
    QPushButton *buttonSnap;
    QPushButton *buttonStart;
    QPushButton *buttonStop;

    QGridLayout *channelLayout;
    QMap<QString, QCheckBox *> channelCheckboxMap;
    QMap<QString, QPushButton *> channelButtonMap;
    QMap<QString, QSpinBox *> channelExposureInputMap;
    QMap<QString, QSpinBox *> channelIlluminationInputMap;

    QLabel *stateLabel;
    QLabel *messageLabel;
};

#endif // TASKCONTROLVIEW_H

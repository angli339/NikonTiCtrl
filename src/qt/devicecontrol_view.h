#ifndef DEVICECONTROLVIEW_H
#define DEVICECONTROLVIEW_H

#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QWidget>

#include "qt/devicecontrol_model.h"

class ButtonGroup : public QWidget {
    Q_OBJECT
public:
    explicit ButtonGroup(QStringList valueList, QWidget *parent = nullptr);

    void setEnabled(bool enabled);
    void setValue(QString value);
    void setButtonLabels(QMap<QString, QString> valueLabelMap);

signals:
    void valueChanged(QString value);

public:
    QStringList valueList;
    QMap<QString, QPushButton *> valueButtonMap;
    QMap<QString, QString> valueLabelMap;
};

class XYStageView : public QWidget {
    Q_OBJECT
public:
    explicit XYStageView(QWidget *parent = nullptr);

    void setValue(QString value);

public:
    QLabel *labelPosX;
    QLabel *labelPosY;
};

class ZStageView : public QWidget {
    Q_OBJECT
public:
    explicit ZStageView(QWidget *parent = nullptr);

    void setPositionZ(QString value);
    void setEnabled(bool enabled)
    {
        buttonGroupPFSState->setEnabled(enabled);
    }
    void setPFSStatus(QString status);
    void setPFSOffset(QString offset);
    void setPFSState(QString state);

public:
    QLabel *labelPosZ;
    QLabel *labelPFSStatus;
    QLabel *labelPFSOffset;
    ButtonGroup *buttonGroupPFSState;
};

class DeviceControlView : public QWidget {
    Q_OBJECT
public:
    explicit DeviceControlView(QWidget *parent = nullptr);
    void setModel(DeviceControlModel *model);

    void setPropertyValueLabels(QMap<QString, QMap<QString, QString>> labels);
    void setPropertyValue(QString propertyPath, QString value);

    void setNikonTiEnabled(bool enabled);
    void setPriorProScanEnabled(bool enabled);

signals:
    void propertyValueChanged(QString propertyPath, QString value);

public:
    QString deviceNameNikonTi = "NikonTi";
    QString deviceNamePriorProScan = "PriorProScan";

    QString xyPosProperty = "/PriorProScan/XYPosition";
    QString zPosProperty = "/NikonTi/ZDrivePosition";
    QString pfsStatusProperty = "/NikonTi/PFSStatus";
    QString pfsOffsetProperty = "/NikonTi/PFSOffset";
    QString pfsStateProperty = "/NikonTi/PFSState";
    QString lightPathProperty = "/NikonTi/LightPath";
    QString objectiveProperty = "/NikonTi/NosePiece";
    QString filterBlockProperty = "/NikonTi/FilterBlock1";
    QString emFilterProperty = "/PriorProScan/FilterWheel3";
    QString exFilterProperty = "/PriorProScan/FilterWheel1";
    QString diaShutterProperty = "/NikonTi/DiaShutter";
    QString epiShutterProperty = "/PriorProScan/LumenShutter";

    DeviceControlModel *model;

    XYStageView *xyStageView;
    ZStageView *zStageView;
    QMap<QString, ButtonGroup *> propertyButtonGroupMap;
};

#endif // DEVICECONTROLVIEW_H

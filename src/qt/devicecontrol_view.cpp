#include "qt/devicecontrol_view.h"

#include <QGridLayout>
#include <QPushButton>

ButtonGroup::ButtonGroup(QStringList valueList, QWidget *parent)
    : QWidget(parent)
{
    this->valueList = valueList;

    QGridLayout *layout = new QGridLayout(this);
    layout->setAlignment(Qt::AlignLeft);
    layout->setSpacing(2);
    layout->setContentsMargins(5, 0, 0, 0);

    for (int i = 0; i < valueList.size(); i++) {
        QString value = valueList[i];

        QPushButton *button = new QPushButton(value);
        button->setFixedSize(QSize(35, 35));
        valueButtonMap[value] = button;

        QFont font = button->font();
        font.setPixelSize(11);
        button->setFont(font);

        layout->addWidget(button, i / 6, i % 6);

        connect(button, &QPushButton::clicked,
                [this, value] { emit valueChanged(value); });
    }
}

void ButtonGroup::setEnabled(bool enabled)
{
    for (auto &button : valueButtonMap) {
        button->setEnabled(enabled);
    }
}

void ButtonGroup::setValue(QString newValue)
{
    for (int i = 0; i < valueList.size(); i++) {
        QPushButton *button = valueButtonMap[valueList[i]];
        if (valueList[i] == newValue) {
            button->setDown(true);
            QFont font = button->font();
            font.setBold(true);
            button->setFont(font);
        } else {
            button->setDown(false);
            QFont font = button->font();
            font.setBold(false);
            button->setFont(font);
        }
    }
}

void ButtonGroup::setButtonLabels(QMap<QString, QString> valueLabelMap)
{
    this->valueLabelMap = valueLabelMap;
    foreach (const QString &value, valueList) {
        QPushButton *button = valueButtonMap[value];
        button->setText(valueLabelMap[value]);
    }
}

XYStageView::XYStageView(QWidget *parent) : QWidget(parent)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(5, 0, 0, 0);
    layout->setAlignment(Qt::AlignLeft);

    QLabel *labelPosXName = new QLabel("X", this);
    QLabel *labelPosYName = new QLabel("Y", this);
    labelPosX = new QLabel(this);
    labelPosY = new QLabel(this);

    for (const auto &label :
         {labelPosXName, labelPosYName, labelPosX, labelPosY}) {
        QFont font = label->font();
        font.setPixelSize(11);
        label->setFont(font);
    }

    layout->addWidget(labelPosXName, 0, 0);
    layout->addWidget(labelPosX, 0, 1);
    layout->addWidget(labelPosYName, 1, 0);
    layout->addWidget(labelPosY, 1, 1);

    setValue("");
}

void XYStageView::setValue(QString value)
{
    if (value == "") {
        labelPosX->setText("---");
        labelPosY->setText("---");
        return;
    }

    QStringList list = value.split(",");
    if (list.length() != 2) {
        labelPosX->setText("<Invalid Value>");
        labelPosY->setText("<Invalid Value>");
        return;
    }
    labelPosX->setText(list[0] + " um");
    labelPosY->setText(list[1] + " um");
}

ZStageView::ZStageView(QWidget *parent) : QWidget(parent)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(5, 0, 0, 0);
    layout->setAlignment(Qt::AlignLeft);

    QLabel *labelPosZName = new QLabel("Z");
    QLabel *labelPFSStatusName = new QLabel("PFS Status");
    QLabel *labelPFSOffsetName = new QLabel("PFS Offset");
    QLabel *labelPFSButtonName = new QLabel("PFS");
    labelPosZ = new QLabel("---");
    labelPFSStatus = new QLabel("---");
    labelPFSOffset = new QLabel("---");

    for (const auto &label :
         {labelPosZName, labelPosZName, labelPFSOffsetName, labelPosZ,
          labelPFSStatus, labelPFSOffset, labelPFSButtonName})
    {
        QFont font = label->font();
        font.setPixelSize(11);
        label->setFont(font);
    }

    buttonGroupPFSState = new ButtonGroup({"On", "Off"});

    layout->addWidget(labelPosZName, 0, 0);
    layout->addWidget(labelPosZ, 0, 1);
    layout->addWidget(labelPFSStatusName, 1, 0);
    layout->addWidget(labelPFSStatus, 1, 1);
    layout->addWidget(labelPFSOffsetName, 2, 0);
    layout->addWidget(labelPFSOffset, 2, 1);
    layout->addWidget(labelPFSButtonName, 3, 0);
    layout->addWidget(buttonGroupPFSState, 3, 1);
}

void ZStageView::setPositionZ(QString value)
{
    if (value == "") {
        labelPosZ->setText("---");
    } else {
        labelPosZ->setText(value + " um");
    }
}

void ZStageView::setPFSStatus(QString status)
{
    if (status == "") {
        labelPFSStatus->setText("---");
    } else {
        labelPFSStatus->setText(status);
    }
}

void ZStageView::setPFSOffset(QString offset)
{
    if (offset == "") {
        labelPFSOffset->setText("---");
    } else {
        labelPFSOffset->setText(offset);
    }
}

void ZStageView::setPFSState(QString state)
{
    buttonGroupPFSState->setValue(state);
}

DeviceControlView::DeviceControlView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel *xyStageTitle = new QLabel("XY Stage");
    QLabel *zStageTitle = new QLabel("Z Focus");
    QLabel *objectiveTitle = new QLabel("Objective");
    QLabel *emFilterTitle = new QLabel("Emission Filter");
    QLabel *filterBlockTitle = new QLabel("Filter Block");
    QLabel *exFilterTitle = new QLabel("Excitation Filter");
    QLabel *lightPathTitle = new QLabel("Light Path");
    QLabel *diaShutterTitle = new QLabel("Dia Shutter");
    QLabel *epiShutterTitle = new QLabel("Epi Shutter");

    for (const auto &label : {xyStageTitle, zStageTitle, objectiveTitle,
                              emFilterTitle, filterBlockTitle, exFilterTitle,
                              lightPathTitle, diaShutterTitle, epiShutterTitle})
    {
        label->setFixedHeight(20);

        QFont font = label->font();
        font.setPixelSize(11);
        label->setFont(font);
    }

    QMap<QString, QStringList> property_values = {
        {"/NikonTi/LightPath", {"1", "2", "3", "4"}},
        {"/NikonTi/NosePiece", {"1", "2", "3", "4", "5", "6"}},
        {"/NikonTi/FilterBlock1", {"1", "2", "3", "4", "5", "6"}},
        {"/PriorProScan/FilterWheel1", {"1", "2", "3", "4", "5", "6"}},
        {"/PriorProScan/FilterWheel3",
         {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}},
        {"/NikonTi/DiaShutter", {"On", "Off"}},
        {"/PriorProScan/LumenShutter", {"On", "Off"}},
    };

    for (const auto &propertyPath : {
             objectiveProperty,
             filterBlockProperty,
             emFilterProperty,
             exFilterProperty,
             lightPathProperty,
             diaShutterProperty,
             epiShutterProperty,
         })
    {
        auto valueList = property_values[propertyPath];
        ButtonGroup *button_group = new ButtonGroup(valueList);
        propertyButtonGroupMap[propertyPath] = button_group;
        connect(button_group, &ButtonGroup::valueChanged,
                [this, propertyPath](QString value) {
                    emit propertyValueChanged(propertyPath, value);
                });
    }

    xyStageView = new XYStageView();
    zStageView = new ZStageView();

    layout->addWidget(xyStageTitle);
    layout->addWidget(xyStageView);

    layout->addWidget(zStageTitle);
    layout->addWidget(zStageView);

    connect(zStageView->buttonGroupPFSState, &ButtonGroup::valueChanged,
            [this](QString value) {
                emit propertyValueChanged(pfsStateProperty, value);
            });

    layout->addWidget(objectiveTitle);
    layout->addWidget(propertyButtonGroupMap[objectiveProperty]);

    layout->addWidget(exFilterTitle);
    layout->addWidget(propertyButtonGroupMap[exFilterProperty]);

    layout->addWidget(filterBlockTitle);
    layout->addWidget(propertyButtonGroupMap[filterBlockProperty]);

    layout->addWidget(emFilterTitle);
    layout->addWidget(propertyButtonGroupMap[emFilterProperty]);

    layout->addWidget(lightPathTitle);
    layout->addWidget(propertyButtonGroupMap[lightPathProperty]);

    layout->addWidget(diaShutterTitle);
    layout->addWidget(propertyButtonGroupMap[diaShutterProperty]);

    layout->addWidget(epiShutterTitle);
    layout->addWidget(propertyButtonGroupMap[epiShutterProperty]);

    setNikonTiEnabled(false);
    setPriorProScanEnabled(false);
}

void DeviceControlView::setModel(DeviceControlModel *model)
{
    this->model = model;
    connect(model, &DeviceControlModel::propertyValueChanged, this,
            &DeviceControlView::setPropertyValue);
    connect(this, &DeviceControlView::propertyValueChanged, model,
            &DeviceControlModel::setPropertyValue);

    connect(model, &DeviceControlModel::deviceConnected,
            [this](QString devName) {
                if (devName == deviceNameNikonTi) {
                    setNikonTiEnabled(true);
                } else if (devName == deviceNamePriorProScan) {
                    setPriorProScanEnabled(true);
                }
            });

    connect(model, &DeviceControlModel::deviceDisconnected,
            [this](QString devName) {
                if (devName == deviceNameNikonTi) {
                    setNikonTiEnabled(false);
                } else if (devName == deviceNamePriorProScan) {
                    setPriorProScanEnabled(false);
                }
            });
}

void DeviceControlView::setPropertyValueLabels(
    QMap<QString, QMap<QString, QString>> labels)
{
    for (auto it = labels.constBegin(); it != labels.constEnd(); it++) {
        auto &propertyPath = it.key();
        auto &valueLabels = it.value();
        if (propertyButtonGroupMap.contains(propertyPath)) {
            propertyButtonGroupMap[propertyPath]->setButtonLabels(valueLabels);
        }
    }
}

void DeviceControlView::setPropertyValue(QString propertyPath, QString value)
{
    if (propertyPath == xyPosProperty) {
        xyStageView->setValue(value);
    } else if (propertyPath == zPosProperty) {
        zStageView->setPositionZ(value);
    } else if (propertyPath == pfsStatusProperty) {
        zStageView->setPFSStatus(value);
    } else if (propertyPath == pfsOffsetProperty) {
        zStageView->setPFSOffset(value);
    } else if (propertyPath == pfsStateProperty) {
        zStageView->setPFSState(value);
    } else {
        if (propertyButtonGroupMap.contains(propertyPath)) {
            propertyButtonGroupMap[propertyPath]->setValue(value);
        }
    }
}

void DeviceControlView::setNikonTiEnabled(bool enabled)
{
    zStageView->setEnabled(enabled);
    propertyButtonGroupMap[lightPathProperty]->setEnabled(enabled);
    propertyButtonGroupMap[objectiveProperty]->setEnabled(enabled);
    propertyButtonGroupMap[filterBlockProperty]->setEnabled(enabled);
    propertyButtonGroupMap[diaShutterProperty]->setEnabled(enabled);
}

void DeviceControlView::setPriorProScanEnabled(bool enabled)
{
    propertyButtonGroupMap[emFilterProperty]->setEnabled(enabled);
    propertyButtonGroupMap[exFilterProperty]->setEnabled(enabled);
    propertyButtonGroupMap[epiShutterProperty]->setEnabled(enabled);
}
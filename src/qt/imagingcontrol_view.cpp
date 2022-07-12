#include "qt/imagingcontrol_view.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

ImagingControlView::ImagingControlView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *containerLayout = new QVBoxLayout(this);
    containerLayout->setSpacing(0);
    containerLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    //
    // Status bar
    //
    QHBoxLayout *statusBarLayout = new QHBoxLayout;
    statusBarLayout->setSpacing(0);
    statusBarLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    statusBarLayout->setContentsMargins(0, 0, 0, 0);

    stateLabel = new QLabel("---");
    stateLabel->setFixedWidth(60);
    messageLabel = new QLabel("---");
    messageLabel->setFixedWidth(400);

    statusBarLayout->addWidget(stateLabel);
    statusBarLayout->addWidget(messageLabel);

    //
    // Main
    //
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setSpacing(3);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setContentsMargins(0, 0, 0, 0);

    containerLayout->addLayout(layout);
    containerLayout->addLayout(statusBarLayout);

    //
    // Control buttons: Live, Start, Stop
    //
    QVBoxLayout *controlButtonLayout = new QVBoxLayout;
    controlButtonLayout->setSpacing(3);
    controlButtonLayout->setAlignment(Qt::AlignTop);
    controlButtonLayout->setContentsMargins(0, 0, 0, 0);

    buttonLiveOrStop = new QPushButton("Live");
    buttonSnap = new QPushButton("Snap");
    buttonStart = new QPushButton("Start");
    buttonStop = new QPushButton("Stop");

    for (auto &button : {buttonLiveOrStop, buttonSnap, buttonStart, buttonStop})
    {
        button->setFixedSize(70, 30);

        QFont font = button->font();
        font.setPixelSize(12);
        button->setFont(font);

        controlButtonLayout->addWidget(button);
    }

    //
    // Channels
    //
    channelLayout = new QGridLayout;
    channelLayout->setSpacing(5);
    channelLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    channelLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *labelChannel = new QLabel("Channel");
    QLabel *exposureChannel = new QLabel("Exposure (ms)");
    QLabel *illuminationChannel = new QLabel("Illumination (%)");

    channelLayout->addWidget(labelChannel, 1, 0);
    channelLayout->addWidget(exposureChannel, 2, 0);
    channelLayout->addWidget(illuminationChannel, 3, 0);

    layout->addLayout(controlButtonLayout);
    layout->addLayout(channelLayout);

    setControlButtonEnabled(false);
}

void ImagingControlView::setModel(ExperimentControlModel *model)
{
    this->model = model;

    QStringList channel_names =
        model->ChannelControlModel()->ListChannelNames();
    for (int i = 0; i < channel_names.size(); i++) {
        QString channel_name = channel_names[i];
        Channel channel =
            model->ChannelControlModel()->GetChannel(channel_name);
        bool channel_enabled =
            model->ChannelControlModel()->IsChannelEnabled(channel_name);

        QCheckBox *checkbox = new QCheckBox;
        QPushButton *button = new QPushButton(channel_name);
        QSpinBox *exposureInput = new QSpinBox;

        if (channel_enabled) {
            checkbox->setCheckState(Qt::Checked);
        } else {
            checkbox->setCheckState(Qt::Unchecked);
        }

        button->setFixedSize(55, 35);

        exposureInput->setRange(1, 10000);
        exposureInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
        exposureInput->setValue(channel.exposure_ms);

        channelCheckboxMap[channel_name] = checkbox;
        channelButtonMap[channel_name] = button;
        channelExposureInputMap[channel_name] = exposureInput;

        channelLayout->addWidget(checkbox, 0, i + 1);
        channelLayout->addWidget(button, 1, i + 1);
        channelLayout->addWidget(exposureInput, 2, i + 1);

        // TODO: hide illumination input of BF
        {
            QSpinBox *illuminationInput = new QSpinBox;
            illuminationInput->setRange(1, 100);
            illuminationInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
            channelIlluminationInputMap[channel_name] = illuminationInput;
            channelLayout->addWidget(illuminationInput, 3, i + 1);
            illuminationInput->setValue(channel.illumination_intensity);
            connect(
                illuminationInput, qOverload<int>(&QSpinBox::valueChanged),
                [this, channel_name](int value) {
                    this->model->ChannelControlModel()->SetChannelIllumination(
                        channel_name, value);
                });
        }

        connect(button, &QPushButton::clicked, [this, channel_name] {
            this->model->ChannelControlModel()->SwitchChannel(channel_name);
        });
        connect(checkbox, &QCheckBox::stateChanged,
                [this, channel_name](int state) {
                    this->model->ChannelControlModel()->SetChannelEnabled(
                        channel_name, state != 0);
                });
        connect(exposureInput, qOverload<int>(&QSpinBox::valueChanged),
                [this, channel_name](int value) {
                    this->model->ChannelControlModel()->SetChannelExposure(
                        channel_name, value);
                });
    }

    connect(model, &ExperimentControlModel::stateChanged, this,
            &ImagingControlView::setState);
    connect(buttonLiveOrStop, &QPushButton::clicked, this,
            &ImagingControlView::toggleLiveView);
    connect(buttonStart, &QPushButton::clicked, model,
            &ExperimentControlModel::StartMultiChannelTask);
    connect(model, &ExperimentControlModel::channelChanged, this,
            &ImagingControlView::setChannel);
    connect(model, &ExperimentControlModel::messageReceived, this,
            &ImagingControlView::setMessage);
}

void ImagingControlView::setChannel(QString channel_name)
{
    for (auto it = channelButtonMap.begin(); it != channelButtonMap.end(); it++)
    {
        auto button = it.value();
        if (it.key() == channel_name) {
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

void ImagingControlView::setState(QString state)
{
    stateLabel->setText(state);
    if (state == "Ready") {
        setControlButtonEnabled(true);
        buttonLiveOrStop->setText("Live");
    } else if (state == "Not Ready") {
        setControlButtonEnabled(false);
        buttonLiveOrStop->setText("Live");
    } else if (state == "Live") {
        buttonLiveOrStop->setEnabled(true);
        buttonLiveOrStop->setText("Stop");
        buttonSnap->setEnabled(false);
        buttonStart->setEnabled(false);
        buttonStop->setEnabled(false);
        LOG_DEBUG("emit liveViewStarted");
        emit liveViewStarted();
        LOG_DEBUG("liveViewStarted emitted");
    } else if (state == "Running") {
        buttonLiveOrStop->setEnabled(false);
        buttonLiveOrStop->setText("Live");
        buttonSnap->setEnabled(false);
        buttonStart->setEnabled(false);
        buttonStop->setEnabled(true);
    } else if (state == "Error") {
        // if (!model->isBusy()) {
        //     setControlButtonEnabled(true);
        //     buttonLiveOrStop->setText("Live");
        // }
    }
}

void ImagingControlView::toggleLiveView()
{
    if (buttonLiveOrStop->text() == "Live") {
        model->StartLiveView();
    } else if (buttonLiveOrStop->text() == "Stop") {
        model->StopLiveView();
    }
}

void ImagingControlView::setControlButtonEnabled(bool enabled)
{
    for (auto &button : {buttonLiveOrStop, buttonSnap, buttonStart, buttonStop})
    {
        button->setEnabled(enabled);
    }
}

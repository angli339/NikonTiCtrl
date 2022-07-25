#include "qt/ndimage_view.h"

#include <stdexcept>

#include "image/imageutils.h"

NDImageView::NDImageView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(5);
    layout->setContentsMargins(0, 0, 0, 0);

    imGridLayout = new QGridLayout;
    imGridLayout->setSpacing(5);
    imGridLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *zSliderLayout = new QHBoxLayout;
    zSlider = new QScrollBar(Qt::Horizontal);
    zSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    zSlider->setPageStep(1);
    zSlider->setSingleStep(1);
    zLabelCurrent = new QLabel;
    zLabelTotal = new QLabel;
    zSliderLayout->addWidget(new QLabel("Z"));
    zSliderLayout->addWidget(zSlider);
    zSliderLayout->addWidget(zLabelCurrent);
    zSliderLayout->addWidget(new QLabel("|"));
    zSliderLayout->addWidget(zLabelTotal);

    QHBoxLayout *tSliderLayout = new QHBoxLayout;
    tSlider = new QScrollBar(Qt::Horizontal);
    tSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    tSlider->setPageStep(1);
    tSlider->setSingleStep(1);
    tLabelCurrent = new QLabel;
    tLabelTotal = new QLabel;
    tSliderLayout->addWidget(new QLabel("T"));
    tSliderLayout->addWidget(tSlider);
    tSliderLayout->addWidget(tLabelCurrent);
    tSliderLayout->addWidget(new QLabel("|"));
    tSliderLayout->addWidget(tLabelTotal);

    layout->addLayout(imGridLayout);
    layout->addLayout(zSliderLayout);
    layout->addLayout(tSliderLayout);

    setNDimZ(0);
    setNDimT(0);
    setIndexZ(0);
    setIndexT(0);
    setNChannels(1);
}

void NDImageView::clear()
{
    for (auto &widget : channelViewList) {
        imGridLayout->removeWidget(widget);
        delete widget;
    }
    channelViewList.clear();
}

void NDImageView::setNChannels(int n_channel)
{
    if (n_channel < 1) {
        throw std::invalid_argument("invalid channel number");
    }
    if (n_channel > 6) {
        throw std::invalid_argument("too many channel number");
    }

    if (n_channel == channelViewList.size()) {
        return;
    }

    for (const auto &widget : channelViewList) {
        imGridLayout->removeWidget(widget);
    }
    
    if (channelViewList.size() < n_channel) {
        int n_diff = n_channel - channelViewList.size();
        for (int i = 0; i < n_diff; i++) {
            NDImageChannelView *channelView = new NDImageChannelView;
            channelView->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);
            channelViewList.push_back(channelView);
        }
    } else if (channelViewList.size() > n_channel) {
        int n_diff = channelViewList.size() - n_channel;
        for (int i = 0; i < n_diff; i++) {
            delete channelViewList.back();
            channelViewList.pop_back();
        }
    }

    switch (n_channel) {
    case 1:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        break;
    case 2:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        imGridLayout->addWidget(channelViewList[1], 0, 1);
        break;
    case 3:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        imGridLayout->addWidget(channelViewList[1], 0, 1);
        imGridLayout->addWidget(channelViewList[2], 1, 0);
        break;
    case 4:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        imGridLayout->addWidget(channelViewList[1], 0, 1);
        imGridLayout->addWidget(channelViewList[2], 1, 0);
        imGridLayout->addWidget(channelViewList[3], 1, 1);
        break;
    case 5:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        imGridLayout->addWidget(channelViewList[1], 0, 1);
        imGridLayout->addWidget(channelViewList[2], 0, 2);
        imGridLayout->addWidget(channelViewList[3], 1, 0);
        imGridLayout->addWidget(channelViewList[4], 1, 1);
        break;
    case 6:
        imGridLayout->addWidget(channelViewList[0], 0, 0);
        imGridLayout->addWidget(channelViewList[1], 0, 1);
        imGridLayout->addWidget(channelViewList[2], 0, 2);
        imGridLayout->addWidget(channelViewList[3], 1, 0);
        imGridLayout->addWidget(channelViewList[4], 1, 1);
        imGridLayout->addWidget(channelViewList[5], 1, 2);
        break;
    }
}

void NDImageView::setNDimZ(int n_z)
{
    zLabelTotal->setText(QString::number(n_z));
    if (n_z > 0) {
        zSlider->setMaximum(n_z - 1);
    } else {
        zSlider->setMaximum(0);
    }
}

void NDImageView::setNDimT(int n_t)
{
    tLabelTotal->setText(QString::number(n_t));
    if (n_t > 0) {
        tSlider->setMaximum(n_t - 1);
    } else {
        tSlider->setMaximum(0);
    }
}

void NDImageView::setIndexZ(int i_z)
{
    zLabelCurrent->setText(QString::number(i_z));
    zSlider->setValue(i_z);
}

void NDImageView::setIndexT(int i_t)
{
    tLabelCurrent->setText(QString::number(i_t));
    tSlider->setValue(i_t);
}

void NDImageView::setChannelName(int i_channel, QString channelName)
{
    channelViewList[i_channel]->controlBar->setChannelName(channelName);
}

void NDImageView::setFrameData(int i_channel, ImageData frame)
{
    channelViewList[i_channel]->glImageView->setImageFrame(frame);
    channelViewList[i_channel]->glImageView->update();

    auto hist = im::Hist(frame);
    channelViewList[i_channel]->controlBar->histView->setData(hist);
}

NDImageChannelView::NDImageChannelView(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    glImageView = new GLImageView;
    controlBar = new GLImageControlBar;

    glImageView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(glImageView);
    layout->addWidget(controlBar);

    connect(controlBar->histView, &ImageHistView::vminChanged, glImageView,
            &GLImageView::setVmin);

    connect(controlBar->histView, &ImageHistView::vmaxChanged, glImageView,
            &GLImageView::setVmax);
}

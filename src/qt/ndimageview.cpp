#include "qt/ndimageview.h"

#include <stdexcept>

#include "data/imageutils.h"

NDImageView::NDImageView(QWidget *parent) : QWidget(parent)
{
    setNumChannels(1);
}

void NDImageView::setNumChannels(int n_channel)
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

    if (layout) {
        for (const auto &widget : channelViewList) {
            layout->removeWidget(widget);
        }
        delete layout;
    }
    layout = new QGridLayout;
    setLayout(layout);
    layout->setSpacing(5);
    layout->setContentsMargins(0, 0, 0, 0);

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
        layout->addWidget(channelViewList[0], 0, 0);
        break;
    case 2:
        layout->addWidget(channelViewList[0], 0, 0);
        layout->addWidget(channelViewList[1], 0, 1);
        break;
    case 3:
        layout->addWidget(channelViewList[0], 0, 0);
        layout->addWidget(channelViewList[1], 0, 1);
        layout->addWidget(channelViewList[2], 1, 0);
        break;
    case 4:
        layout->addWidget(channelViewList[0], 0, 0);
        layout->addWidget(channelViewList[1], 0, 1);
        layout->addWidget(channelViewList[2], 1, 0);
        layout->addWidget(channelViewList[3], 1, 1);
        break;
    case 5:
        layout->addWidget(channelViewList[0], 0, 0);
        layout->addWidget(channelViewList[1], 0, 1);
        layout->addWidget(channelViewList[2], 0, 2);
        layout->addWidget(channelViewList[3], 1, 0);
        layout->addWidget(channelViewList[4], 1, 1);
        break;
    case 6:
        layout->addWidget(channelViewList[0], 0, 0);
        layout->addWidget(channelViewList[1], 0, 1);
        layout->addWidget(channelViewList[2], 0, 2);
        layout->addWidget(channelViewList[3], 1, 0);
        layout->addWidget(channelViewList[4], 1, 1);
        layout->addWidget(channelViewList[5], 1, 2);
        break;
    }
}

void NDImageView::setFrameData(int i_channel, ImageData frame)
{
    channelViewList[i_channel]->glImageView->setImageFrame(frame);
    channelViewList[i_channel]->glImageView->update();

    auto hist = im::Hist(frame);
    channelViewList[i_channel]->controlBar->histView->setData(hist);
}

void NDImageView::setChannelName(int i_channel, QString channelName)
{
    channelViewList[i_channel]->controlBar->setChannelName(channelName);
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

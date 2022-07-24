#include "image/imageutils.h"

#include <fmt/format.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace im {

std::vector<double> Hist(ImageData im)
{
    cv::Mat im_mat = im.AsMat();
    cv::Mat cvHist;
    int histChannels[] = {0};
    int histBins[] = {256};
    float range[] = {0, 65536};
    const float *histRange = {range};
    cv::calcHist(&im_mat, 1, histChannels, cv::Mat(), cvHist, 1, histBins,
                 &histRange, true, false);
    cv::normalize(cvHist, cvHist, 1.0, 0.0, cv::NORM_MINMAX);

    std::vector<double> hist;
    hist.reserve(histBins[0]);
    for (int i = 0; i < histBins[0]; i++) {
        hist.push_back(cvHist.at<float>(i));
    }
    return hist;
}

} // namespace im
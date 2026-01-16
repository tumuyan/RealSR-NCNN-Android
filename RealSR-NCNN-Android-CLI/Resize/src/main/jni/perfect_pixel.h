#ifndef PERFECT_PIXEL_H
#define PERFECT_PIXEL_H

#include <opencv2/opencv.hpp>
#include <vector>

// Mirroring the Python arguments where applicable
// scale > 0: shrink to grid then upscale by scale (nearest neighbor)
// scale <= 0: shrink to grid then upscale to ORIGINAL input size (nearest neighbor)
int process_perfect_pixel(const cv::Mat& input, cv::Mat& output, int scale);

#endif // PERFECT_PIXEL_H

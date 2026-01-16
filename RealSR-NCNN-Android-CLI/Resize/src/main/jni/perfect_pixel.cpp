#include "perfect_pixel.h"
#define _USE_MATH_DEFINES
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// --- Helper Functions ---

static cv::Mat compute_fft_magnitude(const cv::Mat& gray_image) {
    cv::Mat padded;
    int m = cv::getOptimalDFTSize(gray_image.rows);
    int n = cv::getOptimalDFTSize(gray_image.cols);
    cv::copyMakeBorder(gray_image, padded, 0, m - gray_image.rows, 0, n - gray_image.cols, cv::BORDER_CONSTANT, cv::Scalar::all(0));

    cv::Mat planes[] = {cv::Mat_<float>(padded), cv::Mat::zeros(padded.size(), CV_32F)};
    cv::Mat complexI;
    cv::merge(planes, 2, complexI);

    cv::dft(complexI, complexI);

    cv::split(complexI, planes);
    cv::magnitude(planes[0], planes[1], planes[0]);
    cv::Mat mag = planes[0];

    mag += cv::Scalar::all(1);
    cv::log(mag, mag);

    // Crop the spectrum, if it has an odd number of rows or columns
    mag = mag(cv::Rect(0, 0, mag.cols & -2, mag.rows & -2));

    // Rearrange the quadrants of Fourier image  so that the origin is at the image center
    int cx = mag.cols / 2;
    int cy = mag.rows / 2;

    cv::Mat q0(mag, cv::Rect(0, 0, cx, cy));
    cv::Mat q1(mag, cv::Rect(cx, 0, cx, cy));
    cv::Mat q2(mag, cv::Rect(0, cy, cx, cy));
    cv::Mat q3(mag, cv::Rect(cx, cy, cx, cy));

    cv::Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);

    cv::normalize(mag, mag, 0, 1, cv::NORM_MINMAX);
    return mag;
}

static std::vector<float> smooth_1d(const std::vector<float>& v, int k = 17) {
    if (k < 3) return v;
    if (k % 2 == 0) k += 1;

    float sigma = k / 6.0f;
    int half_k = k / 2;
    std::vector<float> kernel(k);
    float sum = 0.0f;
    for (int i = 0; i < k; ++i) {
        float x = (float)(i - half_k);
        kernel[i] = std::exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < k; ++i) kernel[i] /= (sum + 1e-8f);

    std::vector<float> res(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        float val = 0.0f;
        for (int j = 0; j < k; ++j) {
            int idx = (int)i - half_k + j;
            if (idx >= 0 && idx < (int)v.size()) {
                val += v[idx] * kernel[j];
            }
        }
        res[i] = val;
    }
    return res;
}

struct PeakCandidate {
    int index;
    float climb;
    float fall;
    float score;
};

static float detect_peak(const std::vector<float>& proj, int peak_width = 6, float rel_thr = 0.35f, int min_dist = 6) {
    int center = (int)proj.size() / 2;
    float mx = 0.0f;
    for (float val : proj) if (val > mx) mx = val;

    if (mx < 1e-6f) return 0.0f; // Indicate fail with 0 instead of None

    float thr = mx * rel_thr;
    std::vector<PeakCandidate> candidates;

    for (int i = 1; i < (int)proj.size() - 1; ++i) {
        bool is_peak = true;
        for (int j = 1; j < peak_width; ++j) {
            if (i - j < 0 || i + j >= (int)proj.size()) continue;
            if (proj[i - j + 1] < proj[i - j] || proj[i + j - 1] < proj[i + j]) {
                is_peak = false;
                break;
            }
        }

        if (is_peak && proj[i] >= thr) {
            float left_climb = 0.0f;
            for (int k = i; k > 0; --k) {
                if (proj[k] > proj[k - 1])
                    left_climb = std::abs(proj[i] - proj[k - 1]);
                else break;
            }

            float right_fall = 0.0f;
            for (int k = i; k < (int)proj.size() - 1; ++k) {
                if (proj[k] > proj[k + 1])
                    right_fall = std::abs(proj[i] - proj[k + 1]);
                else break;
            }

            candidates.push_back({i, left_climb, right_fall, std::max(left_climb, right_fall)});
        }
    }

    if (candidates.empty()) return 0.0f;

    std::vector<PeakCandidate> left, right;
    for (const auto& c : candidates) {
        if (c.index < center - min_dist && c.index > center * 0.25) left.push_back(c);
        if (c.index > center + min_dist && c.index < center * 1.75) right.push_back(c);
    }

    std::sort(left.begin(), left.end(), [](const PeakCandidate& a, const PeakCandidate& b) { return a.score > b.score; });
    std::sort(right.begin(), right.end(), [](const PeakCandidate& a, const PeakCandidate& b) { return a.score > b.score; });

    if (left.empty() || right.empty()) return 0.0f;

    int peak_left = left[0].index;
    int peak_right = right[0].index;

    return std::abs(peak_right - peak_left) / 2.0f;
}

static bool estimate_grid_fft(const cv::Mat& gray, int& out_w, int& out_h, int peak_width = 6) {
    int H = gray.rows;
    int W = gray.cols;

    cv::Mat mag_float;
    gray.convertTo(mag_float, CV_32F);
    cv::Mat mag = compute_fft_magnitude(mag_float);

    int band_row = W / 2;
    int band_col = H / 2;
    
    // row_sum: sum across columns (axis=1), results in (H, 1) -> flatten to (H)
    std::vector<float> row_sum(H, 0.0f);
    // col_sum: sum across rows (axis=0), results in (1, W) -> flatten to (W)
    std::vector<float> col_sum(W, 0.0f);

    int start_col = W / 2 - band_row;
    int end_col = W / 2 + band_row;
    for (int r = 0; r < H; ++r) {
        float sum = 0.0f;
        const float* ptr = mag.ptr<float>(r);
        for (int c = std::max(0, start_col); c < std::min(W, end_col); ++c) {
            sum += ptr[c];
        }
        row_sum[r] = sum;
    }

    int start_row = H / 2 - band_col;
    int end_row = H / 2 + band_col;
    for (int c = 0; c < W; ++c) {
        float sum = 0.0f;
        for (int r = std::max(0, start_row); r < std::min(H, end_row); ++r) {
            sum += mag.at<float>(r, c);
        }
        col_sum[c] = sum;
    }

    cv::normalize(row_sum, row_sum, 0, 1, cv::NORM_MINMAX);
    cv::normalize(col_sum, col_sum, 0, 1, cv::NORM_MINMAX);

    row_sum = smooth_1d(row_sum, 17);
    col_sum = smooth_1d(col_sum, 17);

    float scale_row = detect_peak(row_sum, peak_width);
    float scale_col = detect_peak(col_sum, peak_width);

    if (scale_row <= 0 || scale_col <= 0) return false;

    out_w = (int)std::round(scale_col);
    out_h = (int)std::round(scale_row);
    return true;
}

static bool estimate_grid_gradient(const cv::Mat& gray, int& out_w, int& out_h, float rel_thr = 0.2f) {
    int H = gray.rows;
    int W = gray.cols;

    cv::Mat grad_x, grad_y;
    cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

    std::vector<float> grad_x_sum(W, 0.0f);
    std::vector<float> grad_y_sum(H, 0.0f);

    for (int c = 0; c < W; ++c) {
        float s = 0;
        for (int r = 0; r < H; ++r) s += std::abs(grad_x.at<float>(r, c));
        grad_x_sum[c] = s;
    }
    for (int r = 0; r < H; ++r) {
        float s = 0;
        const float* ptr = grad_y.ptr<float>(r);
        for (int c = 0; c < W; ++c) s += std::abs(ptr[c]);
        grad_y_sum[r] = s;
    }

    float max_x = 0, max_y = 0;
    for (float v : grad_x_sum) max_x = std::max(max_x, v);
    for (float v : grad_y_sum) max_y = std::max(max_y, v);

    float thr_x = rel_thr * max_x;
    float thr_y = rel_thr * max_y;

    std::vector<int> peak_x, peak_y;
    int min_interval = 4;

    for (int i = 1; i < W - 1; ++i) {
        if (grad_x_sum[i] > grad_x_sum[i-1] && grad_x_sum[i] > grad_x_sum[i+1] && grad_x_sum[i] >= thr_x) {
            if (peak_x.empty() || i - peak_x.back() >= min_interval) peak_x.push_back(i);
        }
    }
    for (int i = 1; i < H - 1; ++i) {
        if (grad_y_sum[i] > grad_y_sum[i-1] && grad_y_sum[i] > grad_y_sum[i+1] && grad_y_sum[i] >= thr_y) {
            if (peak_y.empty() || i - peak_y.back() >= min_interval) peak_y.push_back(i);
        }
    }

    if (peak_x.size() < 4 || peak_y.size() < 4) return false;

    std::vector<int> intervals_x, intervals_y;
    for (size_t i = 1; i < peak_x.size(); ++i) intervals_x.push_back(peak_x[i] - peak_x[i-1]);
    for (size_t i = 1; i < peak_y.size(); ++i) intervals_y.push_back(peak_y[i] - peak_y[i-1]);

    std::sort(intervals_x.begin(), intervals_x.end());
    std::sort(intervals_y.begin(), intervals_y.end());

    float med_x = intervals_x[intervals_x.size() / 2];
    float med_y = intervals_y[intervals_y.size() / 2];

    out_w = (int)std::round(W / med_x);
    out_h = (int)std::round(H / med_y);
    return true;
}

static bool detect_grid_scale(const cv::Mat& image, int& out_w, int& out_h, int peak_width=6, float max_ratio=1.5f, float min_size=4.0f) {
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    int H = gray.rows;
    int W = gray.cols;

    int grid_w = 0, grid_h = 0;
    bool fft_ok = estimate_grid_fft(gray, grid_w, grid_h, peak_width);
    
    if (!fft_ok) {
        std::cout << "FFT-based grid estimation failed, fallback to gradient-based method." << std::endl;
        if (!estimate_grid_gradient(gray, grid_w, grid_h)) return false;
    } else {
        float pixel_size_x = (float)W / grid_w;
        float pixel_size_y = (float)H / grid_h;
        float max_pixel_size = 20.0f;
        if (std::min(pixel_size_x, pixel_size_y) < min_size || 
            std::max(pixel_size_x, pixel_size_y) > max_pixel_size ||
            pixel_size_x / pixel_size_y > max_ratio || 
            pixel_size_y / pixel_size_x > max_ratio) {
                std::cout << "Inconsistent grid size detected (FFT-based), fallback to gradient-based method." << std::endl;
                if (!estimate_grid_gradient(gray, grid_w, grid_h)) return false;
        }
    }

    float pixel_size_x = (float)W / grid_w;
    float pixel_size_y = (float)H / grid_h;
    float pixel_size;

    if (pixel_size_x / pixel_size_y > max_ratio || pixel_size_y / pixel_size_x > max_ratio) {
        pixel_size = std::min(pixel_size_x, pixel_size_y);
    } else {
        pixel_size = (pixel_size_x + pixel_size_y) / 2.0f;
    }

    std::cout << "Detected pixel size: " << pixel_size << std::endl;
    out_w = (int)std::round(W / pixel_size);
    out_h = (int)std::round(H / pixel_size);
    return true;
}

static float find_best_grid(float origin, float range_val_min, float range_val_max, const std::vector<float>& grad_mag, float thr=0.0f) {
    float best = std::round(origin);
    std::vector<std::pair<float, int>> peaks;
    float mx = 0;
    for(float v : grad_mag) mx = std::max(mx, v);
    if (mx < 1e-6f) return best;

    float rel_thr = mx * thr;
    int start = (int)(-std::round(range_val_min));
    int end = (int)(std::round(range_val_max));

    for (int i = start; i <= end; ++i) {
        int candidate = (int)std::round(origin + i);
        if (candidate <= 0 || candidate >= (int)grad_mag.size() - 1) continue;
        if (grad_mag[candidate] > grad_mag[candidate - 1] && 
            grad_mag[candidate] > grad_mag[candidate + 1] && 
            grad_mag[candidate] >= rel_thr) {
                peaks.push_back({grad_mag[candidate], candidate});
        }
    }

    if (peaks.empty()) return best;
    std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b){ return a.first > b.first; });
    return (float)peaks[0].second;
}

static void refine_grids(const cv::Mat& image, int grid_x, int grid_y, float refine_intensity, std::vector<float>& out_x, std::vector<float>& out_y) {
    int H = image.rows;
    int W = image.cols;
    float cell_w = (float)W / grid_x;
    float cell_h = (float)H / grid_y;

    cv::Mat gray, grad_x, grad_y;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Sobel(gray, grad_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray, grad_y, CV_32F, 0, 1, 3);

    std::vector<float> grad_x_sum(W, 0.0f);
    std::vector<float> grad_y_sum(H, 0.0f);

    for (int c = 0; c < W; ++c) {
        float s = 0;
        for (int r = 0; r < H; ++r) s += std::abs(grad_x.at<float>(r, c));
        grad_x_sum[c] = s;
    }
    for (int r = 0; r < H; ++r) {
        float s = 0;
        const float* ptr = grad_y.ptr<float>(r);
        for (int c = 0; c < W; ++c) s += std::abs(ptr[c]);
        grad_y_sum[r] = s;
    }

    float x = find_best_grid((float)W / 2, cell_w, cell_w, grad_x_sum);
    while (x < W + cell_w / 2) {
        x = find_best_grid(x, cell_w * refine_intensity, cell_w * refine_intensity, grad_x_sum);
        out_x.push_back(x);
        x += cell_w;
    }
    x = find_best_grid((float)W / 2, cell_w, cell_w, grad_x_sum) - cell_w;
    while (x > -cell_w / 2) {
        x = find_best_grid(x, cell_w * refine_intensity, cell_w * refine_intensity, grad_x_sum);
        out_x.push_back(x);
        x -= cell_w;
    }

    float y = find_best_grid((float)H / 2, cell_h, cell_h, grad_y_sum);
    while (y < H + cell_h / 2) {
        y = find_best_grid(y, cell_h * refine_intensity, cell_h * refine_intensity, grad_y_sum);
        out_y.push_back(y);
        y += cell_h;
    }
    y = find_best_grid((float)H / 2, cell_h, cell_h, grad_y_sum) - cell_h;
    while (y > -cell_h / 2) {
        y = find_best_grid(y, cell_h * refine_intensity, cell_h * refine_intensity, grad_y_sum);
        out_y.push_back(y);
        y -= cell_h;
    }

    std::sort(out_x.begin(), out_x.end());
    std::sort(out_y.begin(), out_y.end());
}

static cv::Mat sample_center(const cv::Mat& image, const std::vector<float>& x_coords, const std::vector<float>& y_coords) {
    int nx = (int)x_coords.size() - 1;
    int ny = (int)y_coords.size() - 1;
    if (nx <= 0 || ny <= 0) return cv::Mat();

    cv::Mat out(ny, nx, image.type());
    
    for (int i = 0; i < ny; ++i) {
        for (int j = 0; j < nx; ++j) {
            float cx = (x_coords[j] + x_coords[j+1]) * 0.5f;
            float cy = (y_coords[i] + y_coords[i+1]) * 0.5f;
            int ix = std::max(0, std::min((int)cx, image.cols - 1));
            int iy = std::max(0, std::min((int)cy, image.rows - 1));
            
            if (image.type() == CV_8UC3)
                out.at<cv::Vec3b>(i, j) = image.at<cv::Vec3b>(iy, ix);
            else if (image.type() == CV_8UC4)
                out.at<cv::Vec4b>(i, j) = image.at<cv::Vec4b>(iy, ix);
        }
    }
    return out;
}

static bool get_perfect_pixel_impl(const cv::Mat& image, cv::Mat& out_image, 
                                   std::string sample_method = "center", // only center is implemented fully in specific logic
                                   int* grid_size = nullptr, float min_size = 4.0f, 
                                   int peak_width = 6, float refine_intensity = 0.25f, bool fix_square = true) {
    int scale_col, scale_row;
    if (grid_size) {
        scale_col = grid_size[0];
        scale_row = grid_size[1];
    } else {
        if (!detect_grid_scale(image, scale_col, scale_row, peak_width, 1.5f, min_size)) {
            std::cerr << "Failed to estimate grid size." << std::endl;
            out_image = image.clone();
            return false;
        }
    }

    std::vector<float> x_coords, y_coords;
    refine_grids(image, scale_col, scale_row, refine_intensity, x_coords, y_coords);

    int refined_size_x = (int)x_coords.size() - 1;
    int refined_size_y = (int)y_coords.size() - 1;

    // Currently only supporting 'center', as sample_majority requires more lengthy K-means impl 
    // and sample_center is usually good enough for typical use cases. 
    // If strict parity with Python is needed, majority sampling can be added.
    out_image = sample_center(image, x_coords, y_coords);

    if (fix_square && std::abs(refined_size_x - refined_size_y) == 1) {
        if (refined_size_x > refined_size_y) {
            if (refined_size_x % 2 == 1) out_image = out_image(cv::Rect(0, 0, out_image.cols - 1, out_image.rows));
            else {
                cv::Mat top = out_image.row(0);
                cv::vconcat(top, out_image, out_image);
            }
        } else {
            if (refined_size_y % 2 == 1) out_image = out_image(cv::Rect(0, 0, out_image.cols, out_image.rows - 1));
            else {
                cv::Mat left = out_image.col(0);
                cv::hconcat(left, out_image, out_image);
            }
        }
    }
    std::cout << "Refined grid size: (" << out_image.cols << ", " << out_image.rows << ")" << std::endl;
    return true;
}


int process_perfect_pixel(const cv::Mat& input, cv::Mat& output, int scale) {
    if (input.empty()) return -1;

    cv::Mat small_image;
    // Defaulting to "center" sampling for now as C++ simple port
    bool success = get_perfect_pixel_impl(input, small_image, "center");
    
    if (!success) {
        // Fallback or just return original if failed?
        // Requirement says: "scale > 0 ... using get_perfect_pixel to shrink ... then nearest alg to magnify"
        // If detection fails, we can't shrink correctly. 
        // Let's assume on failure we just behave like nearest neighbor on original (?) or return error.
        // Returning -1 for handling in main.
        return -1;
    }

    if (scale > 0) {
        // scale > 0: 缩小到网格(n*n)，然后放大scale倍 (a = n*scale)
        // 使用最近邻插值
        cv::resize(small_image, output, cv::Size(), scale, scale, cv::INTER_NEAREST);
    } else {
        // scale <= 0: 缩小到网格(n*n)，然后恢复到原始输入尺寸 (a = m)
        cv::resize(small_image, output, input.size(), 0, 0, cv::INTER_NEAREST);
    }
    
    return 0;
}

// classical resize implemented with ncnn library


#include <cstdio>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>

#define _DEMO_PATH  false
#define _VERBOSE_LOG  true
#define _MODE 2

#if _WIN32
// image decoder and encoder with wic
#include "wic_image.h"
#else // _WIN32
// image decoder and encoder with stb
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_STDIO

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"
#include "de_nearst.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/hal/interface.h>
using namespace cv;

#endif // _WIN32

#include "webp_image.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "avir.h"
#include "lancir.h"
#include <chrono>
#include "utils.hpp"

using namespace std::chrono;

#if _WIN32
#include <wchar.h>
static wchar_t* optarg = NULL;
static int optind = 1;
static wchar_t getopt(int argc, wchar_t* const argv[], const wchar_t* optstring)
{
    if (optind >= argc || argv[optind][0] != L'-')
        return -1;

    wchar_t opt = argv[optind][1];
    const wchar_t* p = wcschr(optstring, opt);
    if (p == NULL)
        return L'?';

    optarg = NULL;

    if (p[1] == L':')
    {
        optind++;
        if (optind >= argc)
            return L'?';

        optarg = argv[optind];
    }

    optind++;

    return opt;
}

static std::vector<int> parse_optarg_int_array(const wchar_t* optarg)
{
    std::vector<int> array;
    array.push_back(_wtoi(optarg));

    const wchar_t* p = wcschr(optarg, L',');
    while (p)
    {
        p++;
        array.push_back(_wtoi(p));
        p = wcschr(p, L',');
    }

    return array;
}
#else // _WIN32

#include <unistd.h> // getopt()
#include <limits>


#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <algorithm>




#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <algorithm>

#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <algorithm> // For std::sort, std::unique, std::min, std::max
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::abs, std::round, std::fmin
#include <cstdio>    // For fprintf
#include <limits>    // For std::numeric_limits



static std::vector<int> parse_optarg_int_array(const char *optarg) {
    std::vector<int> array;
    array.push_back(atoi(optarg));

    const char *p = strchr(optarg, ',');
    while (p) {
        p++;
        array.push_back(atoi(p));
        p = strchr(p, ',');
    }

    return array;
}

#endif // _WIN32

// ncnn
#include "cpu.h"
#include "gpu.h"
#include "platform.h"

#include "filesystem_utils.h"

static void print_usage() {
    fprintf(stderr, "Usage: resize-ncnn -i infile -o outfile [options]...\n\n");
    fprintf(stderr, "  -h                   show this help\n");
    fprintf(stderr, "  -v                   verbose output\n");
    fprintf(stderr, "  -i input-path        input image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -o output-path       output image path (jpg/png/webp) or directory\n");
    fprintf(stderr, "  -s scale             upscale ratio (4, default=4)\n");
    fprintf(stderr, "  -m mode        resize mode (bicubic/bilinear/nearest/avir/avir-lancir/de-nearest/de-nearest2/de-nearest3, default=nearest)\n");
    fprintf(stderr, "  -n not-use-ncnn        bicubic/bilinear not use ncnn\n");
    fprintf(stderr, "  -f format            output image format (jpg/png/webp, default=ext/png)\n");
}

#if _WIN32
const std::wstring& optarg_in (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\input3.jpg");
const std::wstring& optarg_out(L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\output3.jpg");
const std::wstring& optarg_mo (L"A:\\Media\\realsr-ncnn-vulkan-20210210-windows\\models-Real-ESRGAN-anime");
#else
const char *optarg_in = "/sdcard/10086/input3.jpg";
const char *optarg_out = "/sdcard/10086/output3.jpg";
const char *optarg_mo = "nearest";

void pretty_print(ncnn::Mat mat);

#endif


// Function to detect mosaic resolution using template matching of grid patterns
// reference https://github.com/rekaXua/demosaic_project/blob/master/demosaic_project_ESRGAN.py
// It analyzes the input pixel data to find the most likely block size of a mosaic pattern.
// pixelData: Pointer to the raw pixel data (e.g., from stbi_load). Assumed 8-bit per channel.
// width: Width of the image.
// height: Height of the image.
// channel: Number of channels (3 for RGB, 4 for RGBA).
// Returns the detected mosaic block size (e.g., 8 for an 8x8 mosaic).
// Returns a default value (HighRange + 1) if detection is inconclusive.
// Returns -1 if input is invalid or critical error occurs.
int detectMosaicResolution(const unsigned char* pixelData, int width, int height, int channel) {
    // Constants (match Python script)
    const int GBlur_kernel_size = 5; // Gaussian blur kernel size (must be odd)
    const int CannyTr1 = 8;        // Canny lower threshold
    const int CannyTr2 = 30;       // Canny upper threshold
    const int LowRange = 2;        // Minimum mosaic block size to check
    const int HighRange = 25;      // Maximum mosaic block size to check
    const double DetectionTr = 0.29; // Threshold for template matching correlation

    // --- Input Validation ---
    if (!pixelData || width <= 0 || height <= 0 || (channel != 3 && channel != 4)) {
        fprintf(stderr, "Error: Invalid input image data (pixelData=nullptr, width=%d, height=%d, channel=%d).\n", width, height, channel);
        return -1;
    }

    // Validate Gaussian blur kernel size
    if (GBlur_kernel_size % 2 == 0 || GBlur_kernel_size <= 0) {
        fprintf(stderr, "Error: GBlur_kernel_size must be a positive odd number. Got %d.\n", GBlur_kernel_size);
        return -1;
    }

    // Check minimum dimensions required by the pattern generation logic
    // The largest pattern size is calculated with masksize = HighRange + 2
    // Python calculation was 2 + masksize + masksize - 1 + 2
    int max_pattern_size_for_highrange = 2 + (HighRange + 2) + (HighRange + 2) - 1 + 2;
    if (width < max_pattern_size_for_highrange || height < max_pattern_size_for_highrange) {
        fprintf(stderr, "Image too small (%dx%d) for mosaic detection logic based on HighRange constant %d. Needs at least %dx%d.\n",
                width, height, HighRange, max_pattern_size_for_highrange, max_pattern_size_for_highrange);
        // It might still work for smaller mask sizes, but the full range check is not possible.
        // Let's return a default or failure? Python doesn't explicitly handle this.
        // If the loop below skips all patterns, the default HighRange+1 will be returned, which is acceptable.
        // Return -1 if it's ridiculously small, but maybe allow if it's just smaller than max_pattern_size.
        // Let's proceed, as the loops below handle cases where pattern size > image size.
    }


    // --- Pattern Creation ---
    // Create detection patterns (grids of lines) for different mosaic block sizes.
    // Python list size: HighRange+2. Indices 0 to HighRange+1.
    // Python stores patterns at masksize-2 for masksize in [LowRange+2, HighRange+2]. These are indices [LowRange, HighRange].
    // C++ vector needs size HighRange+2 to match Python structure, even if some indices are unused.
    std::vector<cv::Mat> patterns(HighRange + 2);

    // Loop through potential mask sizes (matching Python's range: HighRange+2 down to LowRange+2)
    fprintf(stderr, "Patterns for mask sizes from %d down to %d...\n", HighRange + 2, LowRange + 2);
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2; // Index in patterns vector [LowRange, HighRange]

        // Pattern size calculation from Python
        int maskimg_size = 2 + masksize + masksize - 1 + 2;

        // Check if pattern size is reasonable relative to image size. If pattern > image, matchTemplate will fail or crash.
        if (maskimg_size > width || maskimg_size > height) {
            fprintf(stderr, "Warning: Pattern size %d for masksize %d exceeds image dimensions (%dx%d). Skipping pattern creation and matching.\n", maskimg_size, masksize, width, height);
            // The patterns vector will have an empty Mat at this index. MatchTemplate loop will check for empty.
            continue;
        }

        cv::Mat img = cv::Mat::zeros(maskimg_size, maskimg_size, CV_8UC3); // Create a BGR image
        img.setTo(cv::Scalar(255, 255, 255)); // Set background to white

        // Draw black lines as per Python logic (starting 2 pixels in, spaced by masksize - 1)
        for (int i = 2; i < maskimg_size; i += masksize - 1) {
            // Line endpoints should be within bounds, already checked maskimg_size vs image size
            cv::line(img, cv::Point(i, 0), cv::Point(i, maskimg_size - 1), cv::Scalar(0, 0, 0), 1); // Black vertical line
        }
        for (int j = 2; j < maskimg_size; j += masksize - 1) {
            // Line endpoints should be within bounds
            cv::line(img, cv::Point(0, j), cv::Point(maskimg_size - 1, j), cv::Scalar(0, 0, 0), 1); // Black horizontal line
        }
        patterns[pattern_idx] = img; // Store the created pattern
    }

//    fprintf(stderr, "Pattern creation complete.\n");

    // --- Image Preprocessing for Detection ---
    // Load image from pixel data and convert to a format suitable for processing (BGRA)
    // Create a Mat that shares the pixelData buffer (no memory copy here)
    cv::Mat img_input(height, width, (channel == 4 ? CV_8UC4 : CV_8UC3), (void*)pixelData);
    cv::Mat img_bgra_detection;
    if (channel == 4) {
        cv::cvtColor(img_input, img_bgra_detection, cv::COLOR_RGBA2BGRA);
    } else {
        cv::cvtColor(img_input, img_bgra_detection, cv::COLOR_RGB2BGRA);
    }

    // Create the mask image (Python's 'card') where detected regions will be marked.
    // Initialize as fully transparent BGRA. This mask is used *after* detection in the Python script,
    // but the detection loop in Python draws on it, so we create it here.
    cv::Mat card_mask = cv::Mat::zeros(height, width, CV_8UC4); // Fully transparent BGRA

    // Convert the BGRA image to grayscale for the core detection pipeline
    cv::Mat img_gray;
    cv::cvtColor(img_bgra_detection, img_gray, cv::COLOR_BGRA2GRAY);

    // Apply Canny edge detection to find grid lines
    cv::Canny(img_gray, img_gray, CannyTr1, CannyTr2);
    img_gray = 255 - img_gray; // Invert: edges become dark (0), non-edges bright (255)

    // Apply Gaussian Blur to smooth the edge map slightly
    cv::GaussianBlur(img_gray, img_gray, cv::Size(GBlur_kernel_size, GBlur_kernel_size), 0);

    fprintf(stderr, "Starting template matching...\n");

    // --- Detection: Perform template matching for each pattern ---
    // resolutions vector stores the count of matches for each potential masksize.
    // Python vector size was HighRange+2 initially (indices 0 to HighRange+1), then appended 0 -> size HighRange+3.
    // Python stored counts at indices masksize-1 for masksize in [LowRange+2, HighRange+2]. These are indices [LowRange+1, HighRange+1].
    // Index HighRange+2 is explicitly set to 0 by Python's append. Indices 0 to LowRange are left as initial -1 (or whatever numpy default is).
    // C++ resolutions vector needs size HighRange+3 (indices 0 to HighRange+2) initialized to 0 to match Python structure for extrema calculation.
    std::vector<int> resolutions(HighRange + 3, 0); // Initialize all counts to 0


    // Loop through potential mask sizes (matching Python's detection loop range: HighRange+2 down to LowRange+2)
    for (int masksize = HighRange + 2; masksize >= LowRange + 2; --masksize) {
        int pattern_idx = masksize - 2;      // Index in patterns vector [LowRange, HighRange]
        int resolution_idx = masksize - 1;   // Index in resolutions vector [LowRange+1, HighRange+1]

        // Ensure indices are within bounds (should be if constants and loops are correct, but defensive check)
        if (pattern_idx < 0 || pattern_idx >= patterns.size() ||
            resolution_idx < 0 || resolution_idx >= resolutions.size()) {
            fprintf(stderr, "Error: Index calculation out of bounds during detection. pattern_idx=%d, resolution_idx=%d. Skipping masksize %d.\n",
                    pattern_idx, resolution_idx, masksize);
            continue; // Skip this masksize
        }

        // Check if pattern was successfully created (skipped if too large or error)
        if (patterns[pattern_idx].empty()) {
            continue; // Skip matching if pattern was not created
        }

        cv::Mat templateImg;
        // Pattern is BGR (white=255, black=0). Convert to grayscale for matching (CV_8U).
        cv::cvtColor(patterns[pattern_idx], templateImg, cv::COLOR_BGR2GRAY);

        int w = templateImg.cols;
        int h = templateImg.rows;

        // Ensure template is smaller than or equal to the image dimensions (redundant with pattern creation check, but safe)
        if (w > img_gray.cols || h > img_gray.rows) {
            continue; // Skip match if template is too large
        }

        cv::Mat img_detection_result; // Result of matchTemplate (CV_32F)
        // Match the (inverted, blurred) edge image against the pattern (dark lines on white background).
        // High correlation (near 1) indicates areas in the image where the processed edges match the pattern grid.
        cv::matchTemplate(img_gray, templateImg, img_detection_result, cv::TM_CCOEFF_NORMED);

        // Threshold the result to find locations above the detection threshold (i.e., good matches)
        cv::Mat detection_locations_mask; // This will be CV_8U, 255 for matches, 0 otherwise
        // Thresholding a CV_32F Mat with THRESH_BINARY works, result is CV_8U.
        cv::threshold(img_detection_result, detection_locations_mask, DetectionTr, 255, cv::THRESH_BINARY);

        // Find all points that are above the threshold (non-zero values in the mask)
        std::vector<cv::Point> points;
        cv::findNonZero(detection_locations_mask, points);

        int rects = points.size(); // Count of locations above threshold for this masksize
        resolutions[resolution_idx] = rects; // Store count at the correct index (masksize - 1)

        // Optional: Draw rectangles on the card_mask (for visualizing detected regions)
        // This part does not affect the resolution detection logic itself.
        // Drawing black opaque rectangles (0,0,0,255)
        for (const auto& pt : points) {
            // Check bounds before drawing rectangle (should be okay if template size check passed, but safe)
            if (pt.x >= 0 && pt.y >= 0 && pt.x + w <= card_mask.cols && pt.y + h <= card_mask.rows) {
                cv::rectangle(card_mask, pt, cv::Point(pt.x + w, pt.y + h), cv::Scalar(0, 0, 0, 255), -1);
            } else {
                // This should ideally not happen if template size is checked correctly
                fprintf(stderr, "Warning: Drawing rectangle out of bounds at (%d, %d) with size (%d, %d) for masksize %d.\n", pt.x, pt.y, w, h, masksize);
            }
        }

        // fprintf(stderr, "Masksize %d (res_idx %d) found %d matches.\n", masksize, resolution_idx, rects);
    }

    // At this point, `resolutions` vector (size HighRange+3) has counts at indices [LowRange+1, HighRange+1].
    // Indices [0, LowRange] and [HighRange+2] are 0 as initialized.

    fprintf(stderr, "Calculating resolution...\n");
    // Debugging: Print populated resolutions vector segment
    fprintf(stderr, "Resolutions counts (indices %d to %d): [", LowRange + 1, HighRange + 1);
    for(int i = LowRange + 1; i <= HighRange + 1; ++i) {
        if (i < resolutions.size()) { // Safety check
            fprintf(stderr, "%d%s", resolutions[i], i == HighRange + 1 ? "" : ", ");
        }
    }
    fprintf(stderr, "]\n");


    // --- Calculate Resolution ---
    // The logic is to find local minima in the `resolutions` counts to define groups of potential resolutions.
    // Then, find the group with the highest cumulative match count, breaking ties with the max count within the group,
    // and finally preferring smaller resolutions in case of a full tie.

    // Find local minima in the resolutions vector (indices 0 to HighRange+2)
    // Python argrelextrema(..., np.less) finds indices i where resolutions[i] < resolutions[i-1] AND resolutions[i] < resolutions[i+1]
    std::vector<int> extremaMIN_indices;

    // Manually add LowRange at the start (as per Python). LowRange is 2. Index 2 in resolutions.
    extremaMIN_indices.push_back(LowRange);

    // Find true local minima (value strictly less than left neighbor AND less than or equal to right neighbor)
    // Check indices from 1 up to size-2. resolutions.size() is HighRange + 3.
    // Loop i from 1 to HighRange + 1.
    // Check neighbors i-1 and i+1. Both are within bounds [0, HighRange+2].
    for (size_t i = 1; i < resolutions.size() - 1; ++i) {
        if (resolutions[i] < resolutions[i - 1] && resolutions[i] <= resolutions[i + 1]) {
            extremaMIN_indices.push_back(static_cast<int>(i));
        }
    }

    // Manually add HighRange+2 at the end (as per Python)
    extremaMIN_indices.push_back(HighRange + 2);

    // Sort and unique the extrema indices
    std::sort(extremaMIN_indices.begin(), extremaMIN_indices.end());
    extremaMIN_indices.erase(std::unique(extremaMIN_indices.begin(), extremaMIN_indices.end()), extremaMIN_indices.end());

    // Debugging: Print final extrema indices
    fprintf(stderr, "Final Extrema indices defining groups: [");
    for(size_t i = 0; i < extremaMIN_indices.size(); ++i) {
        fprintf(stderr, "%d%s", extremaMIN_indices[i], i == extremaMIN_indices.size() - 1 ? "" : ", ");
    }
    fprintf(stderr, "]\n");

    // Find the "biggest extrema group" based on sum and max value preference
    int MosaicResolutionOfImage = HighRange + 1; // Default value if no clear best group is found

    // Variables to track the properties of the best group found so far
    int best_group_sum_score = -1; // Stores sum + int(sum*0.05)
    int best_group_max_val_score = -1; // Stores max_val + int(max_val*0.15)
    int best_original_index_of_max = -1; // The index in `resolutions` where the max count of the best group was found

    if (extremaMIN_indices.size() < 2) {
        fprintf(stderr, "Not enough extrema points (%zu) to form groups. Cannot calculate resolution reliably.\n", extremaMIN_indices.size());
        // Keep default resolution HighRange + 1
    } else {
        // Iterate through pairs of extrema indices to define groups [start_idx, end_idx] inclusive
        for (size_t i = 0; i < extremaMIN_indices.size() - 1; ++i) {
            int group_start_res_index = extremaMIN_indices[i];
            int group_end_res_index = extremaMIN_indices[i+1]; // Inclusive range [start, end]

            // Check bounds for group indices (should be within [0, HighRange+2])
            if (group_start_res_index < 0 || group_start_res_index >= (int)resolutions.size() ||
                group_end_res_index < 0 || group_end_res_index >= (int)resolutions.size() ||
                group_start_res_index > group_end_res_index) {
                fprintf(stderr, "ERROR: Invalid extrema group indices [%d, %d] for resolutions size %zu. Skipping group.\n",
                        group_start_res_index, group_end_res_index, resolutions.size());
                continue; // Skip this invalid group
            }

            int current_group_sum = 0;
            int current_max_val_in_group = -1;
            int current_original_index_of_max_in_group = -1; // Index in `resolutions` for max of this group

            // Calculate sum and find max value + its original index within this group range [start, end]
            for (int res_idx = group_start_res_index; res_idx <= group_end_res_index; ++res_idx) {
                // The index res_idx is guaranteed to be within [group_start_res_index, group_end_res_index]
                // which are already checked to be within [0, resolutions.size()-1].
                current_group_sum += resolutions[res_idx];
                if (current_max_val_in_group == -1 || resolutions[res_idx] > current_max_val_in_group) {
                    current_max_val_in_group = resolutions[res_idx];
                    current_original_index_of_max_in_group = res_idx;
                } else if (resolutions[res_idx] == current_max_val_in_group) {
                    // Tie-breaker for max value within the group: Python's slice index().
                    // If multiple indices have the max value, index() returns the first one.
                    // Our loop iterates res_idx from start to end. The first one found will be stored.
                    // This implicitly matches Python's behavior for max_element finding.
                }
            }

            // If the current group has no positive matches, it's unlikely to be the target resolution peak.
            if (current_max_val_in_group <= 0) {
                // fprintf(stderr, "Skipping group [%d, %d] with no matches.\n", group_start_res_index, group_end_res_index);
                continue;
            }

            // Calculate scores for the current group based on Python's logic
            int current_sum_score = current_group_sum + static_cast<int>(current_group_sum * 0.05);
            int current_max_val_score = current_max_val_in_group + static_cast<int>(current_max_val_in_group * 0.15);

            // Compare current group against the best group found so far (using scores and tie-breakers)
            bool update_best = false;
            if (best_original_index_of_max == -1) { // This is the first valid group found
                update_best = true;
            } else {
                // Compare sum scores first
                if (current_sum_score > best_group_sum_score) {
                    update_best = true; // Current group has a strictly better sum score
                } else if (current_sum_score == best_group_sum_score) {
                    // Sum scores are tied. Compare max value scores as tie-breaker.
                    if (current_max_val_score > best_group_max_val_score) {
                        update_best = true; // Current group has a strictly better max value score
                    } else if (current_max_val_score == best_group_max_val_score) {
                        // Both sum scores and max value scores are tied.
                        // Tie-breaker: Prefer the group whose peak (max value) is at a *smaller* index in the `resolutions` vector.
                        // Smaller index corresponds to a smaller masksize (resolution_idx = masksize - 1).
                        if (current_original_index_of_max_in_group < best_original_index_of_max) {
                            update_best = true; // Prefer smaller masksize on full tie
                        }
                    }
                }
            }

            if (update_best) {
                best_group_sum_score = current_sum_score;
                best_group_max_val_score = current_max_val_score;
                best_original_index_of_max = current_original_index_of_max_in_group;
                // Debugging:
                // fprintf(stderr, "New best group found. Peak at res_idx %d (masksize %d). Sum Score %d, Max Score %d.\n",
                //         best_original_index_of_max, best_original_index_of_max + 1, best_group_sum_score, best_group_max_val_score);
            }

        } // End loop through extrema groups

        // After finding the best group, determine the mosaic resolution
        if (best_original_index_of_max != -1) {
            // The best index in `resolutions` corresponds to masksize = index + 1.
            // This index is expected to be in the range [LowRange+1, HighRange+1] if a match was found.
            // Minimum index possible here is LowRange + 1 = 3. So minimum masksize is 4.
            MosaicResolutionOfImage = best_original_index_of_max + 1;
//            fprintf(stderr, "Detected Mosaic Resolution (from best group peak index): %d\n", MosaicResolutionOfImage);

            // Python has a final check: if MosaicResolutionOfImage == 0, set to HighRange+1.
            // Given how we derive it (index + 1), and indices are >= LowRange+1, it won't be 0 if best_original_index_of_max != -1.
            // Let's include the check anyway to be safe and match Python's final logic state.
            if (MosaicResolutionOfImage == 0) {
                fprintf(stderr, "Calculated MosaicResolutionOfImage was unexpectedly 0. Setting to default HighRange + 1 (%d).\n", HighRange + 1);
                MosaicResolutionOfImage = HighRange + 1;
            }

        } else {
            // No clear best group found (e.g., no points above threshold for any masksize,
            // or all groups were skipped). MosaicResolutionOfImage remains at its default value.
            fprintf(stderr, "No clear best group found during resolution calculation. Using default resolution HighRange + 1 (%d).\n", HighRange + 1);
            // MosaicResolutionOfImage is already initialized to HighRange + 1
        }

    } // End if extremaMIN_indices.size() >= 2

    return MosaicResolutionOfImage;
}

#if _WIN32
int wmain(int argc, wchar_t** argv)
#else

int main(int argc, char **argv)
#endif
{
    path_t inputpath;
    path_t outputpath;
    int scale = 4;
    double scale_d = 4;
    std::vector<int> tilesize;
#if _DEMO_PATH
    path_t model = optarg_mo;
#else
    path_t model = PATHSTR("nearest");
#endif
    int verbose = 0;
    bool not_use_ncnn = false;
    path_t format = PATHSTR("png");

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"i:o:s:t:m:g:j:f:vxhn")) != (wchar_t)-1)
    {
        switch (opt)
        {
        case L'i':
            inputpath = optarg;
            break;
        case L'o':
            outputpath = optarg;
            break;
        case L's':
            scale = _wtoi(optarg);
            break;
        case L't':
            tilesize = parse_optarg_int_array(optarg);
            break;
        case L'm':
            model = optarg;
            break;

        case L'f':
            format = optarg;
            break;
        case L'v':
            verbose = 1;
            break;
        case L'n':
            not_use_ncnn = true;
            break;
        case L't':
        case L'g':
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "i:o:s:t:m:g:j:f:vxhn")) != -1) {
        switch (opt) {
            case 'i':
                inputpath = optarg;
                break;
            case 'o':
                outputpath = optarg;
                break;
            case 's':
                scale = atoi(optarg);
                char *endptr;
                scale_d = strtod(optarg, &endptr);
                // printf("scale endptr=%s=,arg=%s=,scale=%f\n", endptr ,optarg,scale_d);
                break;
            case 'm':
                model = optarg;
                break;
            case 'f':
                format = optarg;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'n':
                not_use_ncnn = true;
                break;
            case 'g':
            case 't':
                break;
            case 'h':
            default:
                print_usage();
                return -1;
        }
    }
#endif // _WIN32


    if (inputpath.empty()) {
        print_usage();
#if _DEMO_PATH
        fprintf(stderr, "demo input argument\n");
        inputpath = optarg_in;
#else
        return -1;
#endif
    }


    if (outputpath.empty()) {
        print_usage();
#if _DEMO_PATH
        fprintf(stderr, "demo output argument\n");
        outputpath = optarg_ou;
#else
        return -1;
#endif
    }


    if (!path_is_directory(outputpath)) {
        // guess format from outputpath no matter what format argument specified
        path_t ext = get_file_extension(outputpath);

        if (ext == PATHSTR("png") || ext == PATHSTR("PNG")) {
            format = PATHSTR("png");
        } else if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP")) {
            format = PATHSTR("webp");
        } else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                   ext == PATHSTR("JPEG")) {
            format = PATHSTR("jpg");
        } else {
            fprintf(stderr, "invalid outputpath extension type\n");
            return -1;
        }
    }

    if (format != PATHSTR("png") && format != PATHSTR("webp") && format != PATHSTR("jpg")) {
        fprintf(stderr, "invalid format argument\n");
        return -1;
    }

    // collect input and output filepath
    std::vector<path_t> input_files;
    std::vector<path_t> output_files;
    {
        if (path_is_directory(inputpath) && path_is_directory(outputpath)) {
            std::vector<path_t> filenames;
            int lr = list_directory(inputpath, filenames);
            if (lr != 0)
                return -1;

            const int count = filenames.size();
            input_files.resize(count);
            output_files.resize(count);

            path_t last_filename;
            path_t last_filename_noext;
            for (int i = 0; i < count; i++) {
                path_t filename = filenames[i];
                path_t filename_noext = get_file_name_without_extension(filename);
                path_t output_filename = filename_noext + PATHSTR('.') + format;

                // filename list is sorted, check if output image path conflicts
                if (filename_noext == last_filename_noext) {
                    path_t output_filename2 = filename + PATHSTR('.') + format;
#if _WIN32
                    fwprintf(stderr, L"both %ls and %ls output %ls ! %ls will output %ls\n", filename.c_str(), last_filename.c_str(), output_filename.c_str(), filename.c_str(), output_filename2.c_str());
#else
                    fprintf(stderr, "both %s and %s output %s ! %s will output %s\n",
                            filename.c_str(), last_filename.c_str(), output_filename.c_str(),
                            filename.c_str(), output_filename2.c_str());
#endif
                    output_filename = output_filename2;
                } else {
                    last_filename = filename;
                    last_filename_noext = filename_noext;
                }

                input_files[i] = inputpath + PATHSTR('/') + filename;
                output_files[i] = outputpath + PATHSTR('/') + output_filename;
            }
        } else if (!path_is_directory(inputpath) && !path_is_directory(outputpath)) {
            input_files.push_back(inputpath);
            output_files.push_back(outputpath);
        } else {
            fprintf(stderr,
                    "inputpath and outputpath must be either file or directory at the same time\n");
            return -1;
        }
    }

    int prepadding = 0;

    if (model.find(PATHSTR("bicubic")) != path_t::npos ||
        model.find(PATHSTR("bilinear")) != path_t::npos ||
        model.find(PATHSTR("nearest")) != path_t::npos ||
        model.find(PATHSTR("avir")) != path_t::npos
            ) {
        prepadding = 10;
    } else {
        fprintf(stderr, "unknown mode type\n");
        return -1;
    }


//    ncnn::create_gpu_instance();


    if (verbose)
        fprintf(stderr, "init classical resize\n");
    else
        fprintf(stderr, "busy...\n");

    for (int i = 0; i < input_files.size(); i++) {
        const path_t &imagepath = input_files[i];
        path_t &outputpath = output_files[i];

//        int webp = 0;

        unsigned char *pixeldata = 0;
        int w;
        int h;
        int c;

#if _WIN32
        FILE* fp = _wfopen(imagepath.c_str(), L"rb");
#else
        FILE *fp = fopen(imagepath.c_str(), "rb");
#endif
        if (fp) {
            // read whole file
            unsigned char *filedata = 0;
            int length = 0;
            {
                fseek(fp, 0, SEEK_END);
                length = ftell(fp);
                rewind(fp);
                filedata = (unsigned char *) malloc(length);
                if (filedata) {
                    fread(filedata, 1, length, fp);
                }
                fclose(fp);
            }

            if (filedata) {
                pixeldata = webp_load(filedata, length, &w, &h, &c);
                if (!pixeldata) {
//                    webp = 1;
//                } else {
                    // not webp, try jpg png etc.
#if _WIN32
                    pixeldata = wic_decode_image(imagepath.c_str(), &w, &h, &c);
#else // _WIN32
                    pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 0);


                    fprintf(stderr, "pixeldata stbi_load w/h/c %d/%d/%d \n",
                            w, h, c
                    );

                    if (pixeldata) {
                        // stb_image auto channel
                        if (c == 1) {
                            // grayscale -> rgb
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 3);
                            c = 3;
                            fprintf(stderr, "pixeldata c==1 w/h/c %d/%d/%d \n",
                                    w, h, c
                            );
                        } else if (c == 2) {
                            // grayscale + alpha -> rgba
                            stbi_image_free(pixeldata);
                            pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 4);
                            c = 4;
                            fprintf(stderr, "pixeldata c==2 w/h/c %d/%d/%d \n",
                                    w, h, c
                            );
                        }
                    }
#endif // _WIN32
                }
            } else if (_VERBOSE_LOG) {
#if _WIN32
                fwprintf(stderr, L"no filedata\n");
#else // _WIN32
                fprintf(stderr, "no filedata\n");
#endif // _WIN32
            }

            free(filedata);
        } else if (_VERBOSE_LOG) {
#if _WIN32
            fwprintf(stderr, L"fopen failed\n");
#else // _WIN32
            fprintf(stderr, "fopen failed\n");
#endif // _WIN32
        }


        // 计算降采样的倍率
        if (pixeldata) {
            if (model.find(PATHSTR("de-nearest3")) != path_t::npos){
                scale = detectMosaicResolution(pixeldata, w, h, c);
                scale_d = 1.0 / scale;
                fprintf(stderr, "image might be interpolated by nearest x%d\n", scale);
            } else if (model.find(PATHSTR("de-nearest2")) != path_t::npos) {
                fprintf(stderr, "Running de-nearest2 (identical row/col check):\n");

                int identical_rows = 0;
                int identical_cols = 0;
                // Tolerance for comparison (e.g., handle minor JPEG artifacts)
                // Set to 0 for strict nearest neighbor, maybe 1 or 2 otherwise.
                const int tolerance = 32;
                int line_size = w * c;

                // --- Row Comparison ---
                // Compare row i with row i-1
                for (int i = 1; i < h; ++i) {
                    bool rows_are_identical = true;
                    // Pointer to the start of row i and row i-1
                    const unsigned char* p1 = pixeldata + i * line_size;
                    const unsigned char* p0 = pixeldata + (i - 1) * line_size;
                    // Compare the entire row byte by byte (or pixel by pixel)
                    for (int j = 0; j < line_size; ++j) {
                        if (abs(p1[j] - p0[j]) > tolerance) {
                            rows_are_identical = false;
                            break; // Rows differ, no need to check further
                        }
                    }
                    if (rows_are_identical) {
                        identical_rows++;
                    }
                }

                // --- Column Comparison ---
                // Compare column j with column j-1
                for (int j = 1; j < w; ++j) {
                    bool cols_are_identical = true;
                    // Iterate through each row for the current pair of columns
                    for (int i = 0; i < h; ++i) {
                        // Pointer to pixel (i, j) and pixel (i, j-1)
                        const unsigned char* p1 = pixeldata + i * line_size + j * c;
                        const unsigned char* p0 = pixeldata + i * line_size + (j - 1) * c;
                        // Compare all channels for this pixel
                        for(int k = 0; k < c; ++k) {
                            if (abs(p1[k] - p0[k]) > tolerance) {
                                cols_are_identical = false;
                                break; // Pixels differ in this channel
                            }
                        }
                        if (!cols_are_identical) {
                            break; // Columns differ in this row, no need to check further rows
                        }
                    }
                    if (cols_are_identical) {
                        identical_cols++;
                    }
                }

                fprintf(stderr, "Identical adjacent rows found: %d (out of %d pairs)\n", identical_rows, h - 1);
                fprintf(stderr, "Identical adjacent cols found: %d (out of %d pairs)\n", identical_cols, w - 1);

                // --- Calculate Scale ---
                double scale_y = 1.0;
                double scale_x = 1.0;

                // Number of unique rows = total rows - identical rows = h - identical_rows
                // Scale = total rows / unique rows
                int unique_rows = h - identical_rows;
                if (unique_rows > 0) {
                    // We expect identical_rows ~= (scale_y - 1) * unique_rows
                    // So h = scale_y * unique_rows => scale_y = h / unique_rows
                    scale_y = static_cast<double>(h) / unique_rows;
                } else if (h > 0) {
                    fprintf(stderr, "Warning: Zero unique rows estimated (image might be uniform vertically?). Assuming scale_y=1.\n");
                    // If the image is perfectly uniform vertically, scale is ambiguous.
                    // Or maybe identical_rows calculation is slightly off near 100%
                    // If identical_rows == h-1 (all rows same), scale_y is effectively h if h>1, or 1 if h=1.
                    // Let's refine: if identical_rows == h-1 and h > 1, maybe scale_y = h?
                    // But sticking to scale_y = h / unique_rows seems more general. A near-zero unique_rows will give large scale.
                    // If unique_rows is truly 0, maybe scale_y=1 is safest default.
                    scale_y = 1.0;
                }

                // Similarly for columns
                int unique_cols = w - identical_cols;
                if (unique_cols > 0) {
                    scale_x = static_cast<double>(w) / unique_cols;
                } else if (w > 0) {
                    fprintf(stderr, "Warning: Zero unique cols estimated (image might be uniform horizontally?). Assuming scale_x=1.\n");
                    scale_x = 1.0;
                }

                fprintf(stderr, "Calculated scale_x: %.3f, scale_y: %.3f\n", scale_x, scale_y);

                // --- Validation and Output ---
                double scale_threshold = 1.9; // Require scale to be close to at least 2
                double integer_proximity = 0.3; // How close scale needs to be to an integer
                double scale_diff_threshold = 0.4; // How close scale_x and scale_y need to be

                bool detected = false;
                int final_scale = 1;

                // Check if both scales are valid and close to each other
                if (scale_x >= scale_threshold && scale_y >= scale_threshold &&
                    abs(scale_x - round(scale_x)) < integer_proximity &&
                    abs(scale_y - round(scale_y)) < integer_proximity &&
                    abs(scale_x - scale_y) <= scale_diff_threshold)
                {
                    final_scale = static_cast<int>(round((scale_x + scale_y) / 2.0));
                    fprintf(stderr, "Consistent x/y scaling detected.\n");
                    detected = true;
                }
                    // Check if only scale_x is valid (potential horizontal-only scaling)
                else if (scale_x >= scale_threshold && scale_y < 1.5 && // Check y scale is low
                         abs(scale_x - round(scale_x)) < integer_proximity)
                {
                    final_scale = static_cast<int>(round(scale_x));
                    fprintf(stderr, "Horizontal-only scaling detected.\n");
                    detected = true;
                }
                    // Check if only scale_y is valid (potential vertical-only scaling)
                else if (scale_y >= scale_threshold && scale_x < 1.5 && // Check x scale is low
                         abs(scale_y - round(scale_y)) < integer_proximity)
                {
                    final_scale = static_cast<int>(round(scale_y));
                    fprintf(stderr, "Vertical-only scaling detected.\n");
                    detected = true;
                }
                    // Maybe a fallback if they are different but both look like integers?
                else if (scale_x >= scale_threshold && scale_y >= scale_threshold &&
                         abs(scale_x - round(scale_x)) < integer_proximity &&
                         abs(scale_y - round(scale_y)) < integer_proximity)
                {
                    // Scales differ but both look valid - take the minimum? Or average?
                    // Taking minimum might be safer for downscaling.
                    final_scale = static_cast<int>(round(fmin(scale_x, scale_y)));
                    fprintf(stderr, "Warning: Different integer scales detected (x%.1f, y%.1f). Using minimum scale %d.\n", scale_x, scale_y, final_scale);
                    detected = true;
                }


                if (detected && final_scale >= 2) {
                    scale = final_scale; // Set the global scale variable
                    scale_d = 1.0 / scale;
                    not_use_ncnn = false; // Indicate we found a scale and NCNN is not needed for this task
                    fprintf(stderr, "de-nearest2: Image likely interpolated by nearest x%d\n", scale);
                } else {
                    fprintf(stderr, "de-nearest2: Image does not appear to be clearly interpolated by nearest neighbor (scale_x=%.2f, scale_y=%.2f, final_scale=%.2f)\n", scale_x, scale_y, final_scale);
                    return -1; // Indicate failure or non-detection by this method
                }
            }

            else if (model.find(PATHSTR("de-nearest")) != path_t::npos) {
                double lost_y[h - 1], lost_x[w - 1];
                double avg_y = 0, avg_x = 0;
                double scale_y = 1, scale_x = 1;
                int line_size = w * c;

                double **lost_x0 = new double *[h];
                for (int i = 0; i < h; i++) {
                    lost_x0[i] = new double[w - 1];
                }

//                fprintf(stderr, "de-nearest:");

                //row
                int p = line_size;
                int q = 0;
                for (int i = 0; i < h - 1; i++) {
                    //line
                    double l = 0;
                    for (int j = 0; j < line_size; j++) {
                        l += pow(pixeldata[p] - pixeldata[q], 2);
                        p++;
                        q++;
                        //   fprintf(stderr," [%d,%d]",i ,j);
                    }

                    double m = l / line_size;
                    lost_y[i] = m;
                    avg_y += m;
                }

                avg_y = avg_y / (h - 1);

                for (int i = 0; i < h - 1; i++) {
                    if (lost_y[i] > avg_y) {
//                         fprintf(stderr," [%.2f]", lost_y[i]);
                        scale_y++;
                    } //else
//                         fprintf(stderr," %.2f", lost_y[i]);
                }
                fprintf(stderr, "scale_y: %d/%f", h, scale_y);
                scale_y = h / scale_y;
                fprintf(stderr, " = %f; avg_lost_y=[%f]\n", scale_y, avg_y);

                for (int i = 0; i < h; i++) {
                    p = i * line_size;
                    for (int j = 0; j < w - 1; j++) {
                        double l = 0;
                        for (int k = 0; k < c; k++) {
//                            l += pow(pixeldata[p] - pixeldata[p + c], 2);
                            l = fmax(l, pow(pixeldata[p] - pixeldata[p + c], 2));
                            p++;
                        }
                        lost_x0[i][j] = l;

                        // fprintf(stderr," [%d,%d]",i ,j);
                    }
//                    fprintf(stderr, " %d", i);
                }

                for (int j = 0; j < w - 1; j++) {
                    double l = 0;
                    for (int i = 0; i < h; i++) {
                        l += lost_x0[i][j];
                    }
                    double m = l / h;
                    lost_x[j] = m;
                    avg_x += m;
                }

                avg_x = avg_x / (w - 1);

                for (int i = 0; i < w - 1; i++) {
                    if (lost_x[i] > avg_x) {
//                    fprintf(stderr," [%.2f]", lost_x[i]);
                        scale_x++;
                    } // else
//                    fprintf(stderr," %.2f", lost_x[i]);
                }
                fprintf(stderr, "scale_x: %d/%f", w, scale_x);
                scale_x = w / scale_x;
                fprintf(stderr, " = %f; avg_lost_x=[%f]\n", scale_x, avg_x);

                for (int i = 0; i < h - 1; i++) {
                    delete[] lost_x0[i];
                }
                delete[] lost_x0;

                if (scale_x < 1.5 || scale_y < 1.5) {
                    fprintf(stderr, "image is not interpolated by nearest\n");
                    return -1;
                } else {
                    not_use_ncnn = false;
                    if (abs(scale_y - scale_x) < 1)
                        scale = round((scale_x + scale_y) / 2);
                    else if (scale_x > scale_y)
                        scale = round(scale_y);
                    else
                        scale = round(scale_x);

                    scale_d = 1.0 / scale;
                    fprintf(stderr, "image might be interpolated by nearest x%d\n", scale);
                }
            }

        }


        int out_w, out_h;
        if ( scale_d<1 ) {
            out_w = (int) (w * scale_d);
            out_h = (int) (h * scale_d);
        } else {
            out_w = w * scale;
            out_h = h * scale;
        }
        int out_line_size = out_w * c;
        unsigned char *buf = new unsigned char[out_line_size * out_h];
        fprintf(stderr, "output w/h/s/s = %d/%d/%d/%f, mode=%s\n", out_w, out_h, scale, scale_d,
                model.c_str());

        if (pixeldata) {

            if (verbose) {
                fprintf(stderr, "pixeldata: ");
                for (int i = 0; i < 32; i++) {
                    fprintf(stderr, " %08X", pixeldata[i]);
                }
                fprintf(stderr, "\n");
            }

            high_resolution_clock::time_point process_begin = high_resolution_clock::now();


            if (not_use_ncnn && model.find(PATHSTR("nearest")) != path_t::npos) {

                if (_MODE == 1) {

                    //row
                    for (int i = 0; i < h; i++) {
                        //line
                        for (int j = 0; j < w; j++) {
                            int p = (w * i + j) * c;
                            int q = (out_w * i + j) * c * scale;

                            //sub-pixel
                            for (int k = 0; k < c; k++) {
                                int m = pixeldata[p + k];

                                // output x offset
                                int x_offset = 0;
                                for (int n = 0; n < scale; n++) {
                                    int o = q + k + x_offset;

                                    // output y offset
                                    int y_offset = 0;
                                    for (int g = 0; g < scale; g++) {
                                        buf[o + y_offset] = m;
                                        y_offset += out_line_size;
                                    }
                                    x_offset += c;
                                }

                                if (verbose) {
                                    fprintf(stderr, "i:%d j:%d :", i, j);
                                    for (int i = 0; i < 32; i++) {
                                        fprintf(stderr, " %02X", buf[i]
                                        );
                                    }
                                    fprintf(stderr, " ,m=%d %d\n", p + k, k + q);
                                }
                            }
                        }
                    }
                } else {
                    if(scale_d>=1)
                    {
                        int p = 0;
                        int q = 0;
                        //row
                        for (int i = 0; i < h; i++) {
                            //line
                            void *l = buf + q;
                            for (int j = 0; j < w; j++) {

                                //pixel
                                for (int k = 0; k < scale; k++) {
                                    memcpy(buf + q, pixeldata + p, sizeof(unsigned char) * c);
                                    q += c;
                                }
                                p += c;
                            }
                            // mult-line
                            for (int k = 1; k < scale; k++) {
                                memcpy(buf + q, l, sizeof(unsigned char) * out_line_size);
                                q += out_line_size;
                            }
                        }
                    }else{
                        // de-nearest
                        int p=(scale-1)/2, q=0;
                        // double step = 1/scale_d;
                        // row
                        for(int i=0;i<out_h;i++){
                            for(int j=0;j<out_w;j++){
                                memcpy(buf + q, pixeldata + p+j*scale, sizeof(unsigned char) * c);
                                q+=c;
                            }
                            p+=w*c;
                        }
                    }
                }

            } else if (not_use_ncnn && model.find(PATHSTR("bilinear")) != path_t::npos) {
                if (w < 2 && h < 2) {
                    fprintf(stderr, "[Err]image is too small\n");
                    return -1;
                }
                if (scale < 2) {
                    fprintf(stderr, "[Err]scale <2\n");
                    return -1;
                }


                out_w = out_w - scale + 1;
                out_h = out_h - scale + 1;
                out_line_size = out_w * c;
                free(buf);
                buf = new unsigned char[out_line_size * out_h];

                // 子像素a点，b点
                unsigned char a, b;
                int p = 0;
                int q = 0;

                int block_size = scale * out_line_size;

                // channel
                for (int l = 0; l < c; l++) {
                    p = l;
                    q = l;
                    for (int i = 0; i < h; i++) {
                        for (int j = 1; j < w; j++) {

                            a = pixeldata[p];
                            b = pixeldata[p + c];
                            if (a == b) {
                                for (int k = 0; k < scale; k++) {
                                    buf[q] = a;
                                    q += c;
                                }
                            } else {
                                float d = ((float) (b - a)) / scale;
                                float e = 0;
                                for (int k = 0; k < scale; k++) {
                                    buf[q] = a + (int) (e);
                                    e += d;
                                    q += c;
                                }
                            }

                            p += c;
                        }
                        buf[q] = b;
                        p += c;
                        q = q + block_size - out_line_size + c;
                        //                        fprintf(stderr, " %d-%d",l,i);
                    }

                }

                //                fprintf(stderr, "\n 2nd\n");
                //
                //                fprintf(stderr, "out_line_size=%d block=%d\n", out_line_size, block_size);
                p = 0;
                for (int i = 0; i < h; i++) {

                    for (int j = 0; j < out_line_size; j++) {
                        a = buf[p];
                        q = out_line_size;
                        b = buf[p + block_size];
                        if (a == b) {
                            for (int k = 1; k < scale; k++) {
                                buf[p + q] = a;
                                q += out_line_size;
                            }
                        } else {
                            float d = ((float) (b - a)) / scale;
                            float e = d;
                            for (int k = 1; k < scale; k++) {
                                buf[p + q] = a + (int) (e);
                                //                            fprintf(stderr, "i-j:%d-%d p-q-k:%d-%d-%d\n",i,j, p, q,k);
                                e += d;
                                q += out_line_size;
                            }

                        }

                        p++;
                    }
                    p = p + block_size - out_line_size;

                }

            } else if (model.find(PATHSTR("lancir")) != path_t::npos) {
                avir::CLancIR ImageResizer;
                ImageResizer.resizeImage(pixeldata, w, h, 0, buf, out_w, out_h, 0, c);
            } else if (model.find(PATHSTR("avir")) != path_t::npos) {
                avir::CImageResizer<> ImageResizer(8);
                ImageResizer.resizeImage(pixeldata, w, h, 0, buf, out_w, out_h, c, 0);
            } else {
                ncnn::Mat in, out;
                if (c == 4) {
                    in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGBA, w, h);
                } else {
                    in = ncnn::Mat::from_pixels(pixeldata, ncnn::Mat::PIXEL_RGB, w, h);
                }

                if (model.find(PATHSTR("nearest")) != path_t::npos) {
                    ncnn::resize_nearest(in, out, out_w, out_h);
                } else if (model.find(PATHSTR("bilinear")) != path_t::npos) {
                    ncnn::resize_bilinear(in, out, out_w, out_h);
                } else {
                    ncnn::resize_bicubic(in, out, out_w, out_h);
                }

                if (c == 4) {
                    out.to_pixels(buf, ncnn::Mat::PIXEL_RGBA);
                } else {
                    out.to_pixels(buf, ncnn::Mat::PIXEL_RGB);
                }
            }

            high_resolution_clock::time_point process_end = high_resolution_clock::now();
            duration<double> process_time_span = duration_cast<duration<double>>(process_end - process_begin);
            fprintf(stderr, "%s use time: %.3lf\n", model.c_str(),process_time_span.count());



            if (verbose) {
                fprintf(stderr, "buf data: ");
                for (int i = 0; i < 32; i++) {
                    fprintf(stderr, " %02X", buf[i]
                    );
                }
                fprintf(stderr, "\n");
            }

            path_t ext = get_file_extension(outputpath);
            if (c == 4 &&
                (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") ||
                 ext == PATHSTR("JPEG"))) {
                path_t output_filename2 = output_files[i] + PATHSTR(".png");
                outputpath = output_filename2;
#if _WIN32
                fwprintf(stderr, L"image %ls has alpha channel ! %ls will output %ls\n", imagepath.c_str(), imagepath.c_str(), output_filename2.c_str());
#else // _WIN32
                fprintf(stderr, "image %s has alpha channel ! %s will output %s\n",
                        imagepath.c_str(),
                        imagepath.c_str(), output_filename2.c_str());
#endif // _WIN32
            }

            // save
            high_resolution_clock::time_point save_begin = high_resolution_clock::now();

            {
                int success = 0;

                path_t ext = get_file_extension(imagepath);

#if _WIN32

#else
                if (ext != PATHSTR("gif")) {
                    // 使用opencv保存图片，速度比默认的stb更快
                    cv::Mat image;
                    switch (c) {
                        case 1:
                            image = cv::Mat( out_h, out_w, CV_8UC1, buf); // 单通道图像
                            break;
                        case 3:
                            image = cv::Mat(out_h, out_w, CV_8UC3, buf); // 3通道图像
                            cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
                            break;
                        case 4:
                            image = cv::Mat(out_h, out_w, CV_8UC4, buf); // 4通道图像
                            cv::cvtColor(image, image, cv::COLOR_RGBA2BGRA);
                            break;
                    }
                    if (image.empty()) {
                        std::cerr << "Error: Image data not loaded." << std::endl;
                        success = false;
                    } else {
#if _WIN32
                        success = imwrite_unicode(outputpath, image);
#else
                        success = imwrite(outputpath.c_str(), image);
#endif
                        fprintf(stderr, "opencv save image success, c=%d, w=%d, h=%d\n", c, out_w, out_h);
                    }
                }else
#endif
                if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP")) {
                    success = webp_save(outputpath.c_str(), out_w, out_h, c, buf);

                } else if (ext == PATHSTR("png") || ext == PATHSTR("PNG")) {
#if _WIN32
                    success = wic_encode_image(outputpath.c_str(), outimage.w, outimage.h, outimage.elempack, outimage.data);
#else
//                    success = stbi_write_png(outputpath.c_str(), out_w, out_h, c, buf, out_w * c);
                    success = stbi_write_png(outputpath.c_str(), out_w, out_h, c, buf, 0);

#endif
                } else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") ||
                           ext == PATHSTR("jpeg") ||
                           ext == PATHSTR("JPEG")) {
#if _WIN32
                    success = wic_encode_jpeg_image(outputpath.c_str(), outimage.w, outimage.h, outimage.elempack, outimage.data);
#else
                    success = stbi_write_jpg(outputpath.c_str(), out_w, out_h, c, buf, 100);
#endif
                }
                if (success) {
                    if (verbose) {
#if _WIN32
                        fwprintf(stderr, L"%ls -> %ls done\n", imagepath.c_str(), outputpath.c_str());
#else
                        fprintf(stderr, "%s -> %s done\n", imagepath.c_str(), outputpath.c_str());
#endif
                    }
                } else {
#if _WIN32
                    fwprintf(stderr, L"encode image %ls failed\n", outputpath.c_str());
#else
                    fprintf(stderr, "encode image %s failed\n", outputpath.c_str());
#endif
                }
            }

            high_resolution_clock::time_point save_end = high_resolution_clock::now();
            duration<double> time_span = duration_cast<duration<double>>(save_end - save_begin);
            fprintf(stderr, "Total use time: %.3lf\n",time_span.count());



        } else {
#if _WIN32
            fwprintf(stderr, L"decode image %ls failed\n", imagepath.c_str());
#else // _WIN32
            fprintf(stderr, "decode image %s failed\n", imagepath.c_str());
#endif // _WIN32
        }
    }

    return 0;
}

void pretty_print(ncnn::Mat m) {
    for (int q = 0; q < m.c; q++) {
        const int *ptr = m.channel(q);
        for (int y = 0; y < m.h; y++) {
            for (int x = 0; x < m.w; x++) {
                printf("%08X ", ptr[x]);
            }
            ptr += m.w;
            printf("\n");

        }
        printf("------------------------\n");
    }
}

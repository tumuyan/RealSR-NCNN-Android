#ifndef ENABLE_OPENCV_DNN

#include <limits>

#ifdef USE_RYZEN
#include <immintrin.h>
#elif defined(USE_EIGEN3)
#define EIGEN_DONT_PARALLELIZE
#include <Eigen/Core>
#endif

#include "Parallel.hpp"
#include "CPUCNNProcessor.hpp"

namespace Anime4KCPP::CPU::detail
{
#ifdef USE_RYZEN
    using StorageType = std::uint16_t;
#else
    using StorageType = float;
#endif

    template<typename T, typename F>
    static void changEachPixel1ToN(const cv::Mat& src, F&& callBack, cv::Mat& tmpMat, int outChannels)
    {
        const int h = src.rows, w = src.cols;
        const int jMAX = w * outChannels;
#ifdef USE_RYZEN
        tmpMat.create(h, w, CV_16UC(outChannels));
#else
        tmpMat.create(h, w, CV_32FC(outChannels));
#endif
        const std::size_t srcStep = src.step;
        const std::size_t step = tmpMat.step;

        Anime4KCPP::Utils::parallelFor(0, h,
            [&](const int i) {
                T* lineData = reinterpret_cast<T*>(src.data + static_cast<std::size_t>(i) * srcStep);
                StorageType* tmpLineData = reinterpret_cast<StorageType*>(tmpMat.data + static_cast<std::size_t>(i) * step);
                for (int j = 0; j < jMAX; j += outChannels)
                    callBack(i, j, tmpLineData + j, lineData);
            });
    }

    template<typename F>
    static void changEachPixelNToN(F&& callBack, cv::Mat& tmpMat)
    {
        const int h = tmpMat.rows, w = tmpMat.cols;
        const int channels = tmpMat.channels();
        const int jMAX = w * channels;
        const std::size_t step = tmpMat.step;

        cv::Mat tmp;
        tmp.create(h, w, tmpMat.type());

        Anime4KCPP::Utils::parallelFor(0, h,
            [&](const int i) {
                StorageType* lineData = reinterpret_cast<StorageType*>(tmpMat.data + static_cast<std::size_t>(i) * step);
                StorageType* tmpLineData = reinterpret_cast<StorageType*>(tmp.data + static_cast<std::size_t>(i) * step);
                for (int j = 0; j < jMAX; j += channels)
                    callBack(i, j, tmpLineData + j, lineData);
            });

        tmpMat = tmp;
    }

    template<typename T, typename F>
    static void changEachPixelNTo1(cv::Mat& img, F&& callBack, const cv::Mat& tmpMat)
    {
        const int h = 2 * tmpMat.rows, w = 2 * tmpMat.cols;
        img.create(h, w, cv::DataType<T>::type);

        const int jMAX = w;
        const std::size_t channels = tmpMat.channels();
        const std::size_t step = tmpMat.step;
        const std::size_t dstStep = img.step;

        Anime4KCPP::Utils::parallelFor(0, h,
            [&](const int i) {
                StorageType* lineData = reinterpret_cast<StorageType*>(tmpMat.data + static_cast<std::size_t>(i >> 1) * step);
                T* tmpLineData = reinterpret_cast<T*>(img.data + static_cast<std::size_t>(i) * dstStep);
                for (int j = 0; j < jMAX; j++)
                    callBack(i, j, tmpLineData + j, lineData + static_cast<std::size_t>(j >> 1) * channels);
            });
    }

    template<typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
    static constexpr float norm(T v)
    {
        return static_cast<float>(v) / std::numeric_limits<T>::max();
    }

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
    static constexpr float norm(T v)
    {
        return static_cast<float>(v);
    }

    template<typename T, std::enable_if_t<std::is_integral<T>::value>* = nullptr>
    static constexpr T unnorm(float v)
    {
        return v >= 1.0f ?
            std::numeric_limits<T>::max() :
            (v <= 0.0f ?
                std::numeric_limits<T>::min() :
                static_cast<T>(std::roundf(v * std::numeric_limits<T>::max())));
    }

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value>* = nullptr>
    static constexpr T unnorm(float v)
    {
        return v < 0.0f ? 0.0f : (1.0f < v ? 1.0f : v);
    }

    template<typename T>
    static void conv1To8Impl(const cv::Mat& img, const float* kernels, const float* biases, cv::Mat& tmpMat)
    {
        const int channels = 8;
        const int srcChannels = img.channels();
        const std::size_t lineStep = img.step1();
        detail::changEachPixel1ToN<T>(img, [&](const int i, const int j, StorageType* outMat, T* curLine) {
            const int orgJ = j / channels * srcChannels;
            const int jp = orgJ < (img.cols - 1)* srcChannels ? srcChannels : 0;
            const int jn = orgJ > 0 ? -srcChannels : 0;

            const T* const pLineData = i < img.rows - 1 ? curLine + lineStep : curLine;
            const T* const cLineData = curLine;
            const T* const nLineData = i > 0 ? curLine - lineStep : curLine;

            const T* tl = nLineData + orgJ + jn, * const tc = nLineData + orgJ, * const tr = nLineData + orgJ + jp;
            const T* ml = cLineData + orgJ + jn, * const mc = cLineData + orgJ, * const mr = cLineData + orgJ + jp;
            const T* bl = pLineData + orgJ + jn, * const bc = pLineData + orgJ, * const br = pLineData + orgJ + jp;

            const float tln = norm<T>(tl[Y]);
            const float tcn = norm<T>(tc[Y]);
            const float trn = norm<T>(tr[Y]);
            const float mln = norm<T>(ml[Y]);
            const float mcn = norm<T>(mc[Y]);
            const float mrn = norm<T>(mr[Y]);
            const float bln = norm<T>(bl[Y]);
            const float bcn = norm<T>(bc[Y]);
            const float brn = norm<T>(br[Y]);

#ifdef USE_RYZEN
            const float* const kptr = kernels;
            const float* const bptr = biases;

            __m256 out0 = _mm256_loadu_ps(bptr);
            __m256 out1 = _mm256_setzero_ps();
            __m256 out2 = _mm256_setzero_ps();

            const __m256 r0 = _mm256_broadcast_ss(&tln);
            const __m256 r1 = _mm256_broadcast_ss(&tcn);
            const __m256 r2 = _mm256_broadcast_ss(&trn);
            const __m256 r3 = _mm256_broadcast_ss(&mln);
            const __m256 r4 = _mm256_broadcast_ss(&mcn);
            const __m256 r5 = _mm256_broadcast_ss(&mrn);
            const __m256 r6 = _mm256_broadcast_ss(&bln);
            const __m256 r7 = _mm256_broadcast_ss(&bcn);
            const __m256 r8 = _mm256_broadcast_ss(&brn);

            const __m256 k0 = _mm256_loadu_ps(kptr);
            const __m256 k1 = _mm256_loadu_ps(kptr + 8);
            const __m256 k2 = _mm256_loadu_ps(kptr + 16);
            const __m256 k3 = _mm256_loadu_ps(kptr + 24);
            const __m256 k4 = _mm256_loadu_ps(kptr + 32);
            const __m256 k5 = _mm256_loadu_ps(kptr + 40);
            const __m256 k6 = _mm256_loadu_ps(kptr + 48);
            const __m256 k7 = _mm256_loadu_ps(kptr + 56);
            const __m256 k8 = _mm256_loadu_ps(kptr + 64);

            out0 = _mm256_fmadd_ps(r0, k0, out0);
            out1 = _mm256_fmadd_ps(r1, k1, out1);
            out2 = _mm256_fmadd_ps(r2, k2, out2);
            out0 = _mm256_fmadd_ps(r3, k3, out0);
            out1 = _mm256_fmadd_ps(r4, k4, out1);
            out2 = _mm256_fmadd_ps(r5, k5, out2);
            out0 = _mm256_fmadd_ps(r6, k6, out0);
            out1 = _mm256_fmadd_ps(r7, k7, out1);
            out2 = _mm256_fmadd_ps(r8, k8, out2);

            out0 = _mm256_max_ps(_mm256_add_ps(out2, _mm256_add_ps(out0, out1)), _mm256_setzero_ps());

            _mm_storeu_si128(reinterpret_cast<__m128i*>(outMat), _mm256_cvtps_ph(out0, 0));
#elif defined(USE_EIGEN3)
            float* const kptr = const_cast<float*>(kernels);
            float* const bptr = const_cast<float*>(biases);

            Eigen::Array<float, 8, 1> out = Eigen::Map<Eigen::Array<float, 8, 1>>(bptr, 8);

            const Eigen::Map<Eigen::Array<float, 8, 1>> k0(kptr, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k1(kptr + 8, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k2(kptr + 16, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k3(kptr + 24, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k4(kptr + 32, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k5(kptr + 40, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k6(kptr + 48, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k7(kptr + 56, 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k8(kptr + 64, 8);

            auto t0 = tln * k0;
            auto t1 = tcn * k1;
            auto t2 = trn * k2;
            auto t3 = mln * k3;
            auto t4 = mcn * k4;
            auto t5 = mrn * k5;
            auto t6 = bln * k6;
            auto t7 = bcn * k7;
            auto t8 = brn * k8;

            out += (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8);

            Eigen::Map<Eigen::Array<StorageType, 8, 1>>(outMat, 8) = out.max(0.0f);
#else
            const float* const kptr = kernels;
            const float* const bptr = biases;

            const float* const k0 = kptr;
            const float* const k1 = kptr + 8;
            const float* const k2 = kptr + 16;
            const float* const k3 = kptr + 24;
            const float* const k4 = kptr + 32;
            const float* const k5 = kptr + 40;
            const float* const k6 = kptr + 48;
            const float* const k7 = kptr + 56;
            const float* const k8 = kptr + 64;

            alignas(32) float out[8];
            std::copy_n(bptr, 8, out);

            for (std::size_t i = 0; i < 8; i++)
                out[i] += tln * k0[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += tcn * k1[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += trn * k2[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += mln * k3[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += mcn * k4[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += mrn * k5[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += bln * k6[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += bcn * k7[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += brn * k8[i];

            for (std::size_t i = 0; i < 8; i++)
                outMat[i] = std::max(out[i], 0.0f);
#endif // USE_RYZEN
            }, tmpMat, 8);
    }

    template<typename T>
    static void convTranspose8To1Impl(cv::Mat& img, const float* kernels, cv::Mat& tmpMat)
    {
        detail::changEachPixelNTo1<T>(img, [&](const std::ptrdiff_t i, const std::ptrdiff_t j, T* outMat, StorageType* inMat) {
            //180 degree rotation for kernel
            //0 1  to  3 2
            //2 3      1 0
            const std::ptrdiff_t index = ((i & 1) << 1) + (j & 1);
#ifdef USE_RYZEN
            const __m256 in = _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(inMat)));
            const __m256 k0 = _mm256_loadu_ps(kernels + index * 8);
            const __m256 r0 = _mm256_dp_ps(in, k0, 0xf1);
            const __m128 r1 = _mm256_extractf128_ps(r0, 0x01);
            const __m128 r2 = _mm256_castps256_ps128(r0);
            const __m128 r3 = _mm_add_ps(r1, r2);

            const float luma = _mm_cvtss_f32(r3);
#elif defined(USE_EIGEN3)
            float* const kptr = const_cast<float*>(kernels + index * 8);

            const float luma =
                Eigen::Map<Eigen::Matrix<StorageType, 8, 1>>(inMat)
                .dot(Eigen::Map<Eigen::Matrix<float, 8, 1>>(kptr));
#else
            const float* const kptr = kernels + index * 8;

            float luma = 0;
            for (std::size_t i = 0; i < 8; i++)
                luma += kptr[i] * inMat[i];
#endif
            * outMat = unnorm<T>(luma);
            }, tmpMat);
    }
}

void Anime4KCPP::CPU::CNNProcessor::conv1To8(const cv::Mat& img, const float* kernels, const float* biases, cv::Mat& tmpMat)
{
    switch (img.depth())
    {
    case CV_8U:
        detail::conv1To8Impl<std::uint8_t>(img, kernels, biases, tmpMat);
        break;
    case CV_16U:
        detail::conv1To8Impl<std::uint16_t>(img, kernels, biases, tmpMat);
        break;
    case CV_32F:
        detail::conv1To8Impl<float>(img, kernels, biases, tmpMat);
        break;
    default:
        throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
    }
}

void Anime4KCPP::CPU::CNNProcessor::conv8To8(const float* kernels, const float* biases, cv::Mat& tmpMat)
{
    const int channels = 8;
    const std::size_t lineStep = tmpMat.step1();
    detail::changEachPixelNToN([&](const int i, const int j, detail::StorageType* outMat, detail::StorageType* curLine) {
        const int jp = j < (tmpMat.cols - 1)* channels ? channels : 0;
        const int jn = j > 0 ? -channels : 0;

        const detail::StorageType* const pLineData = i < tmpMat.rows - 1 ? curLine + lineStep : curLine;
        const detail::StorageType* const cLineData = curLine;
        const detail::StorageType* const nLineData = i > 0 ? curLine - lineStep : curLine;

        const detail::StorageType* const tl = nLineData + j + jn, * const tc = nLineData + j, * const tr = nLineData + j + jp;
        const detail::StorageType* const ml = cLineData + j + jn, * const mc = cLineData + j, * const mr = cLineData + j + jp;
        const detail::StorageType* const bl = pLineData + j + jn, * const bc = pLineData + j, * const br = pLineData + j + jp;

#ifdef USE_RYZEN
        const float* const kptr = kernels;
        const float* const bptr = biases;

        alignas(32) float d0[8];
        alignas(32) float d1[8];
        alignas(32) float d2[8];
        alignas(32) float d3[8];
        alignas(32) float d4[8];
        alignas(32) float d5[8];
        alignas(32) float d6[8];
        alignas(32) float d7[8];
        alignas(32) float d8[8];

        _mm256_store_ps(d0, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(tl))));
        _mm256_store_ps(d1, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(tc))));
        _mm256_store_ps(d2, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(tr))));
        _mm256_store_ps(d3, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(ml))));
        _mm256_store_ps(d4, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(mc))));
        _mm256_store_ps(d5, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(mr))));
        _mm256_store_ps(d6, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(bl))));
        _mm256_store_ps(d7, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(bc))));
        _mm256_store_ps(d8, _mm256_cvtph_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(br))));

        __m256 out0 = _mm256_loadu_ps(bptr);
        __m256 out1 = _mm256_setzero_ps();
        __m256 out2 = _mm256_setzero_ps();

        for (std::size_t i = 0; i < 8; i += 2)
        {
            const __m256 r00 = _mm256_broadcast_ss(d0 + i);
            const __m256 r01 = _mm256_broadcast_ss(d1 + i);
            const __m256 r02 = _mm256_broadcast_ss(d2 + i);
            const __m256 r03 = _mm256_broadcast_ss(d3 + i);
            const __m256 r04 = _mm256_broadcast_ss(d4 + i);
            const __m256 r05 = _mm256_broadcast_ss(d5 + i);
            const __m256 r06 = _mm256_broadcast_ss(d6 + i);
            const __m256 r07 = _mm256_broadcast_ss(d7 + i);
            const __m256 r08 = _mm256_broadcast_ss(d8 + i);

            const __m256 k00 = _mm256_loadu_ps(kptr + i * 72);
            const __m256 k01 = _mm256_loadu_ps(kptr + i * 72 + 8);
            const __m256 k02 = _mm256_loadu_ps(kptr + i * 72 + 16);
            const __m256 k03 = _mm256_loadu_ps(kptr + i * 72 + 24);
            const __m256 k04 = _mm256_loadu_ps(kptr + i * 72 + 32);
            const __m256 k05 = _mm256_loadu_ps(kptr + i * 72 + 40);
            const __m256 k06 = _mm256_loadu_ps(kptr + i * 72 + 48);
            const __m256 k07 = _mm256_loadu_ps(kptr + i * 72 + 56);
            const __m256 k08 = _mm256_loadu_ps(kptr + i * 72 + 64);

            out0 = _mm256_fmadd_ps(r00, k00, out0);
            out1 = _mm256_fmadd_ps(r01, k01, out1);
            out2 = _mm256_fmadd_ps(r02, k02, out2);
            out0 = _mm256_fmadd_ps(r03, k03, out0);
            out1 = _mm256_fmadd_ps(r04, k04, out1);
            out2 = _mm256_fmadd_ps(r05, k05, out2);
            out0 = _mm256_fmadd_ps(r06, k06, out0);
            out1 = _mm256_fmadd_ps(r07, k07, out1);
            out2 = _mm256_fmadd_ps(r08, k08, out2);

            const __m256 r10 = _mm256_broadcast_ss(d0 + i + 1);
            const __m256 r11 = _mm256_broadcast_ss(d1 + i + 1);
            const __m256 r12 = _mm256_broadcast_ss(d2 + i + 1);
            const __m256 r13 = _mm256_broadcast_ss(d3 + i + 1);
            const __m256 r14 = _mm256_broadcast_ss(d4 + i + 1);
            const __m256 r15 = _mm256_broadcast_ss(d5 + i + 1);
            const __m256 r16 = _mm256_broadcast_ss(d6 + i + 1);
            const __m256 r17 = _mm256_broadcast_ss(d7 + i + 1);
            const __m256 r18 = _mm256_broadcast_ss(d8 + i + 1);

            const __m256 k10 = _mm256_loadu_ps(kptr + (i + 1) * 72);
            const __m256 k11 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 8);
            const __m256 k12 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 16);
            const __m256 k13 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 24);
            const __m256 k14 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 32);
            const __m256 k15 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 40);
            const __m256 k16 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 48);
            const __m256 k17 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 56);
            const __m256 k18 = _mm256_loadu_ps(kptr + (i + 1) * 72 + 64);

            out0 = _mm256_fmadd_ps(r10, k10, out0);
            out1 = _mm256_fmadd_ps(r11, k11, out1);
            out2 = _mm256_fmadd_ps(r12, k12, out2);
            out0 = _mm256_fmadd_ps(r13, k13, out0);
            out1 = _mm256_fmadd_ps(r14, k14, out1);
            out2 = _mm256_fmadd_ps(r15, k15, out2);
            out0 = _mm256_fmadd_ps(r16, k16, out0);
            out1 = _mm256_fmadd_ps(r17, k17, out1);
            out2 = _mm256_fmadd_ps(r18, k18, out2);
        }
        out0 = _mm256_max_ps(_mm256_add_ps(out2, _mm256_add_ps(out0, out1)), _mm256_setzero_ps());

        _mm_storeu_si128(reinterpret_cast<__m128i*>(outMat), _mm256_cvtps_ph(out0, 0));
#elif defined(USE_EIGEN3)
        float* const kptr = const_cast<float*>(kernels);
        float* const bptr = const_cast<float*>(biases);

        Eigen::Array<float, 8, 1> out = Eigen::Map<Eigen::Array<float, 8, 1>>(bptr, 8);

        for (std::size_t i = 0; i < 8; i++)
        {
            const Eigen::Map<Eigen::Array<float, 8, 1>> k0(kptr + i * 72);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k1(kptr + i * 72 + 8);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k2(kptr + i * 72 + 16);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k3(kptr + i * 72 + 24);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k4(kptr + i * 72 + 32);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k5(kptr + i * 72 + 40);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k6(kptr + i * 72 + 48);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k7(kptr + i * 72 + 56);
            const Eigen::Map<Eigen::Array<float, 8, 1>> k8(kptr + i * 72 + 64);

            auto t0 = tl[i] * k0;
            auto t1 = tc[i] * k1;
            auto t2 = tr[i] * k2;
            auto t3 = ml[i] * k3;
            auto t4 = mc[i] * k4;
            auto t5 = mr[i] * k5;
            auto t6 = bl[i] * k6;
            auto t7 = bc[i] * k7;
            auto t8 = br[i] * k8;

            out += (t0 + t1 + t2 + t3 + t4 + t5 + t6 + t7 + t8);
        }

        Eigen::Map<Eigen::Array<detail::StorageType, 8, 1>>(outMat, 8) = out.max(0.0f);
#else
        const float* const kptr = kernels;
        const float* const bptr = biases;

        alignas(32) float out[8];
        std::copy_n(bptr, 8, out);

        for (std::size_t c = 0; c < 8; c++)
        {
            const float* const k0 = kptr + c * 72;
            const float* const k1 = kptr + c * 72 + 8;
            const float* const k2 = kptr + c * 72 + 16;
            const float* const k3 = kptr + c * 72 + 24;
            const float* const k4 = kptr + c * 72 + 32;
            const float* const k5 = kptr + c * 72 + 40;
            const float* const k6 = kptr + c * 72 + 48;
            const float* const k7 = kptr + c * 72 + 56;
            const float* const k8 = kptr + c * 72 + 64;

            for (std::size_t i = 0; i < 8; i++)
                out[i] += tl[c] * k0[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += tc[c] * k1[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += tr[c] * k2[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += ml[c] * k3[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += mc[c] * k4[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += mr[c] * k5[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += bl[c] * k6[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += bc[c] * k7[i];
            for (std::size_t i = 0; i < 8; i++)
                out[i] += br[c] * k8[i];
        }

        for (std::size_t i = 0; i < 8; i++)
            outMat[i] = std::max(out[i], 0.0f);

#endif // USE_RYZEN

        }, tmpMat);
}

void Anime4KCPP::CPU::CNNProcessor::convTranspose8To1(cv::Mat& img, const float* kernels, cv::Mat& tmpMat)
{
    switch (img.depth())
    {
    case CV_8U:
        detail::convTranspose8To1Impl<std::uint8_t>(img, kernels, tmpMat);
        break;
    case CV_16U:
        detail::convTranspose8To1Impl<std::uint16_t>(img, kernels, tmpMat);
        break;
    case CV_32F:
        detail::convTranspose8To1Impl<float>(img, kernels, tmpMat);
        break;
    default:
        throw ACException<ExceptionType::RunTimeError>("Unsupported image data type");
    }
}

#endif

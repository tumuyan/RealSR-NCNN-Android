//$ nobt
//$ nocpp

/**
 * @file lancir.h
 *
 * @brief The self-contained "lancir" inclusion file.
 *
 * This is the self-contained inclusion file for the "LANCIR" image resizer,
 * a part of the AVIR library. Features scalar, AVX, SSE2, and NEON
 * optimizations as well as progressive resizing technique which provides a
 * better CPU cache performance.
 *
 * AVIR Copyright (c) 2015-2021 Aleksey Vaneev
 *
 * @mainpage
 *
 * @section intro_sec Introduction
 *
 * Description is available at https://github.com/avaneev/avir
 *
 * @section license License
 *
 * License
 *
 * Copyright (c) 2015-2021 Aleksey Vaneev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * @version 3.0.7
 */

#ifndef AVIR_CLANCIR_INCLUDED
#define AVIR_CLANCIR_INCLUDED

#include <stdint.h>
#include <string.h>
#include <math.h>

#if defined( __AVX__ ) || defined( __AVX2__ )

	#include <immintrin.h>

	#define LANCIR_AVX
	#define LANCIR_SSE2 // Some functions use SSE2; AVX has a higher priority.
	#define LANCIR_ALIGN 32

#elif defined( __SSE4_2__ ) || defined( __SSE4_1__ ) || \
	defined( __SSSE3__ ) || defined( __SSE3__ ) || defined( __SSE2__ ) || \
	defined( __x86_64__ ) || defined( _M_AMD64 ) || defined( _M_X64 ) || \
	defined( __amd64 )

	#include <immintrin.h>

	#define LANCIR_SSE2
	#define LANCIR_ALIGN 16

#elif defined( __aarch64__ ) || defined( __arm64__ ) || defined( __ARM_NEON )

	#include <arm_neon.h>

	#define LANCIR_NEON
	#define LANCIR_ALIGN 16

#else // NEON

	#define LANCIR_ALIGN 4

#endif // NEON

namespace avir {

/**
 * The macro equals to "pi" constant, fills 53-bit floating point mantissa.
 * Undefined at the end of file.
 */

#define LANCIR_PI 3.1415926535897932

/**
 * Function reallocates a typed buffer if its current length is smaller than
 * the required length.
 *
 * @param buf0 Reference to the pointer of the previously allocated buffer.
 * @param buf Reference to address-aligned "buf0" pointer.
 * @param len The current length of the "buf0".
 * @param newlen A new required length.
 * @tparam Tb Buffer element type.
 * @tparam Tl Length variable type.
 */

template< typename Tb, typename Tl >
inline void reallocBuf( Tb*& buf0, Tb*& buf, Tl& len, Tl newlen )
{
	newlen += LANCIR_ALIGN;

	if( newlen > len )
	{
		delete[] buf0;
		len = newlen;
		buf0 = new Tb[ newlen ];
		buf = (Tb*) (( (uintptr_t) buf0 + LANCIR_ALIGN - 1 ) &
			~(uintptr_t) ( LANCIR_ALIGN - 1 ));
	}
}

/**
 * Function reallocates a typed buffer if its current length is smaller than
 * the required length.
 *
 * @param buf Reference to the pointer of the previously allocated buffer;
 * address alignment will not be applied.
 * @param len The current length of the "buf0".
 * @param newlen A new required length.
 * @tparam Tb Buffer element type.
 * @tparam Tl Length variable type.
 */

template< typename Tb, typename Tl >
inline void reallocBuf( Tb*& buf, Tl& len, const Tl newlen )
{
	if( newlen > len )
	{
		delete[] buf;
		len = newlen;
		buf = new Tb[ newlen ];
	}
}

/**
 * @brief LANCIR image resizer class.
 *
 * The object of this class can be used to resize 1-4 channel images to any
 * required size. Resizing is performed by utilizing Lanczos filters, with
 * 8-bit precision. This class offers a kind of "optimal" Lanczos resampling
 * implementation.
 *
 * Object of this class can be allocated on stack.
 *
 * Note that object of this class does not free temporary buffers and
 * variables after the resizeImage() function call (until object's
 * destruction): these buffers are reused (or reallocated) on subsequent
 * calls, thus making batch resizing of images faster. This means resizing is
 * not thread-safe: a separate object should be created for each thread.
 */

class CLancIR
{
private:
	CLancIR( const CLancIR& )
	{
		// Unsupported.
	}

	CLancIR& operator = ( const CLancIR& )
	{
		// Unsupported.
		return( *this );
	}

public:
	CLancIR()
		: FltBuf0( NULL )
		, FltBuf0Len( 0 )
		, spv0( NULL )
		, spv0len( 0 )
	{
	}

	~CLancIR()
	{
		delete[] FltBuf0;
		delete[] spv0;
	}

	/**
	 * Function resizes an image and performs input-to-output type conversion,
	 * if necessary.
	 *
	 * @param[in] SrcBuf Source image buffer.
	 * @param SrcWidth Source image width, in pixels.
	 * @param SrcHeight Source image height, in pixels.
	 * @param SrcSSize Physical size of the source scanline, in elements (not
	 * bytes). If this value is below 1, SrcWidth * ElCount will be used.
	 * @param[out] NewBuf Buffer to accept the resized image. Can be equal to
	 * SrcBuf if the size of the resized image is smaller or equal to the
	 * SrcBuf in size. Specifying a correctly-sized SrcBuf here may be an
	 * efficient approach for just-in-time resizing of small graphical assets,
	 * right after they were loaded: this may provide a better CPU cache
	 * performance, and reduce the number of memory allocations.
	 * @param NewWidth New image width, in pixels.
	 * @param NewHeight New image height, in pixels.
	 * @param NewSSize Physical size of the destination scanline, in elements
	 * (not bytes). If this value is below 1, NewWidth * ElCount will be used.
	 * @param ElCount The number of elements (channels) used to store each
	 * source and destination pixel (1-4).
	 * @param kx0 Resizing step - horizontal (one output pixel corresponds to
	 * "k" input pixels). A downsizing factor if > 1.0; upsizing factor
	 * if <= 1.0. Multiply by -1 if you would like to bypass "ox" and "oy"
	 * adjustment which is done by default to produce a centered image. If
	 * the step value equals 0, the step value will be chosen automatically.
	 * @param ky0 Resizing step - vertical. Same as "kx0".
	 * @param ox Start X pixel offset within source image (can be negative).
	 * Positive offset moves the image to the left.
	 * @param oy Start Y pixel offset within source image (can be negative).
	 * Positive offset moves the image to the top.
	 * @tparam Tin Input buffer's element type. Can be uint8_t (0-255 value
	 * range), uint16_t (0-65535 value range), float (0-1 value range), double
	 * (0-1 value range). uint32_t type is treated as uint16_t. Signed integer
	 * types and larger integer types are unsupported.
	 * @tparam Tout Output buffer's element type, treated like "Tin". If "Tin"
	 * and "Tout" types do not match, an output value scaling will be applied.
	 * Floating-point output will not clamped/clipped/saturated, integer
	 * output is always rounded and clamped.
	 */

	template< typename Tin, typename Tout >
	void resizeImage( const Tin* const SrcBuf, const int SrcWidth,
		const int SrcHeight, const int SrcSSize, Tout* const NewBuf,
		const int NewWidth, const int NewHeight, const int NewSSize,
		const int ElCount, const double kx0 = 0.0, const double ky0 = 0.0,
		double ox = 0.0, double oy = 0.0 )
	{
		if( NewWidth <= 0 || NewHeight <= 0 )
		{
			return;
		}

		const int OutSSize = NewWidth * ElCount;
		const size_t NewScanlineSize = ( NewSSize < 1 ? OutSSize : NewSSize );

		if( SrcWidth <= 0 || SrcHeight <= 0 )
		{
			memset( NewBuf, 0, NewScanlineSize * NewHeight * sizeof( Tout ));
			return;
		}

		const size_t SrcScanlineSize = ( SrcSSize < 1 ?
			SrcWidth * ElCount : SrcSSize );

		const double la = 3.0; // Lanczos "a" parameter.
		double kx;
		double ky;

		if( kx0 == 0.0 )
		{
			kx = (double) SrcWidth / NewWidth;
			ox += ( kx - 1.0 ) * 0.5;
		}
		else
		if( kx0 > 0.0 )
		{
			kx = kx0;
			ox += ( kx0 - 1.0 ) * 0.5;
		}
		else
		{
			kx = -kx0;
		}

		if( ky0 == 0.0 )
		{
			ky = (double) SrcHeight / NewHeight;
			oy += ( ky - 1.0 ) * 0.5;
		}
		else
		if( ky0 > 0.0 )
		{
			ky = ky0;
			oy += ( ky0 - 1.0 ) * 0.5;
		}
		else
		{
			ky = -ky0;
		}

		if( rfv.update( la, ky, ElCount ))
		{
			rsv.reset();
			rsh.reset();
		}

		CResizeFilters* rfh; // Pointer to resizing filters for horizontal
			// resizing, may equal to "rfv" if the same stepping is in use.

		if( kx == ky )
		{
			rfh = &rfv;
		}
		else
		{
			rfh = &rfh0;

			if( rfh0.update( la, kx, ElCount ))
			{
				rsh.reset();
			}
		}

		rsv.update( SrcHeight, NewHeight, oy, rfv );
		rsh.update( SrcWidth, NewWidth, ox, *rfh );

		// Allocate/resize intermediate buffers.

		const int svs = ( rsv.padl + SrcHeight + rsv.padr ) * ElCount;
		reallocBuf( spv0, spv, spv0len, ( svs > OutSSize ? svs : OutSSize ));

		const size_t FltWidthE = ( rsh.padl + SrcWidth + rsh.padr ) * ElCount;
		reallocBuf( FltBuf0, FltBuf, FltBuf0Len, FltWidthE * NewHeight );

		// Calculate vertical progressive resizing's batch size. Progressive
		// batching is used to try to keep addressing within the cache
		// capacity. This technique definitely works well for single-threaded
		// resizing on most CPUs, but may not provide an additional benefit
		// for multi-threaded resizing, or in a system-wide high-load
		// situations.

		const double CacheSize = 5500000.0; // Tuned for various CPUs.
		const double OpSize = (double) SrcScanlineSize * SrcHeight *
			sizeof( Tin ) + (double) FltWidthE * NewHeight * sizeof( float );

		int BatchSize = (int) ( NewHeight * CacheSize / ( OpSize + 1.0 ));

		if( BatchSize < 16 )
		{
			BatchSize = 16;
		}

		// Perform vertical resizing.

		float* opf = FltBuf + rsh.padl * ElCount;
		const CResizePos* rp = rsv.pos;
		int bl = NewHeight;
		int i;

		while( bl > 0 )
		{
			const int bc = ( bl > BatchSize ? BatchSize : bl );

			const int kl = rfv.KernelLen;
			const Tin* ip = SrcBuf;
			float* op = opf;

			const int so = rp -> so;
			float* const sp = spv + so * ElCount;

			int cc = ( rp + bc - 1 ) -> so - so + kl; // Pixel copy count.
			int rl = 0; // Leftmost pixel's replication count.
			int rr = 0; // Rightmost pixel's replication count.

			const int socc = so + cc;
			const int spe = rsv.padl + SrcHeight;

			// Calculate scanline copying and padding parameters, depending on
			// the batch's size and its vertical offset.

			if( so < rsv.padl )
			{
				if( socc <= rsv.padl )
				{
					rl = cc;
					cc = 0;
				}
				else
				{
					if( socc > spe )
					{
						rr = socc - spe;
						cc -= rr;
					}

					rl = rsv.padl - so;
					cc -= rl;
				}
			}
			else
			{
				if( so >= spe )
				{
					rr = cc;
					cc = 0;
					ip += SrcHeight * SrcScanlineSize;
				}
				else
				{
					if( socc > spe )
					{
						rr = socc - spe;
						cc -= rr;
					}

					ip += ( so - rsv.padl ) * SrcScanlineSize;
				}
			}

			// Batched vertical resizing.

			if( ElCount == 1 )
			{
				for( i = 0; i < SrcWidth; i++ )
				{
					copyScanline1v( ip, SrcScanlineSize, sp, cc, rl, rr );
					resize1( spv, op, FltWidthE, rp, kl, bc );
					ip += 1;
					op += 1;
				}
			}
			else
			if( ElCount == 2 )
			{
				for( i = 0; i < SrcWidth; i++ )
				{
					copyScanline2v( ip, SrcScanlineSize, sp, cc, rl, rr );
					resize2( spv, op, FltWidthE, rp, kl, bc );
					ip += 2;
					op += 2;
				}
			}
			else
			if( ElCount == 3 )
			{
				for( i = 0; i < SrcWidth; i++ )
				{
					copyScanline3v( ip, SrcScanlineSize, sp, cc, rl, rr );
					resize3( spv, op, FltWidthE, rp, kl, bc );
					ip += 3;
					op += 3;
				}
			}
			else // ElCount == 4
			{
				for( i = 0; i < SrcWidth; i++ )
				{
					copyScanline4v( ip, SrcScanlineSize, sp, cc, rl, rr );
					resize4( spv, op, FltWidthE, rp, kl, bc );
					ip += 4;
					op += 4;
				}
			}

			opf += bc * FltWidthE;
			rp += bc;
			bl -= bc;
		}

		// Prepare output-related constants.

		const bool IsOutFloat = ( (Tout) 0.25 != 0 );
		const int Clamp = ( sizeof( Tout ) == 1 ? 255 : 65535 );
		float OutMul = ( IsOutFloat ? 1.0f : (float) Clamp );

		if( (Tin) 0.25 == 0 )
		{
			OutMul /= ( sizeof( Tin ) == 1 ? 255 : 65535 );
		}

		// Perform horizontal resizing and produce final output.

		float* ip = FltBuf;
		Tout* opn = NewBuf;

		if( ElCount == 1 )
		{
			for( i = 0; i < NewHeight; i++ )
			{
				padScanline1h( ip, rsh, SrcWidth );
				resize1( ip, spv, 1, rsh.pos, rfh -> KernelLen, NewWidth );
				copyToOutput( spv, opn, OutSSize, Clamp, IsOutFloat, OutMul );
				ip += FltWidthE;
				opn += NewScanlineSize;
			}
		}
		else
		if( ElCount == 2 )
		{
			for( i = 0; i < NewHeight; i++ )
			{
				padScanline2h( ip, rsh, SrcWidth );
				resize2( ip, spv, 2, rsh.pos, rfh -> KernelLen, NewWidth );
				copyToOutput( spv, opn, OutSSize, Clamp, IsOutFloat, OutMul );
				ip += FltWidthE;
				opn += NewScanlineSize;
			}
		}
		else
		if( ElCount == 3 )
		{
			for( i = 0; i < NewHeight; i++ )
			{
				padScanline3h( ip, rsh, SrcWidth );
				resize3( ip, spv, 3, rsh.pos, rfh -> KernelLen, NewWidth );
				copyToOutput( spv, opn, OutSSize, Clamp, IsOutFloat, OutMul );
				ip += FltWidthE;
				opn += NewScanlineSize;
			}
		}
		else // ElCount == 4
		{
			for( i = 0; i < NewHeight; i++ )
			{
				padScanline4h( ip, rsh, SrcWidth );
				resize4( ip, spv, 4, rsh.pos, rfh -> KernelLen, NewWidth );
				copyToOutput( spv, opn, OutSSize, Clamp, IsOutFloat, OutMul );
				ip += FltWidthE;
				opn += NewScanlineSize;
			}
		}
	}

protected:
	float* FltBuf0; ///< Intermediate resizing buffer.
		///<
	size_t FltBuf0Len; ///< Length of "FltBuf0".
		///<
	float* FltBuf; ///< Address-aligned "FltBuf0".
		///<
	float* spv0; ///< Scanline buffer for vertical resizing, also used at the
		///< output stage.
		///<
	int spv0len; ///< Length of "spv0".
		///<
	float* spv; ///< Address-aligned "spv0".
		///<

	class CResizeScanline;

	/**
	 * Class implements fractional delay filter bank calculation.
	 */

	class CResizeFilters
	{
		friend class CResizeScanline;

	public:
		int KernelLen; ///< Resampling filter kernel's length, taps. Available
			///< after the update() function call. Always an even value,
			///< should not be lesser than 4.
			///<

		CResizeFilters()
			: Filters( NULL )
			, FiltersLen( 0 )
			, la( 0.0 )
			, k( 0.0 )
			, ElCount( 0 )
		{
			memset( Bufs0, 0, sizeof( Bufs0 ));
			memset( Bufs0Len, 0, sizeof( Bufs0Len ));
		}

		~CResizeFilters()
		{
			int i;

			for( i = 0; i < BufCount; i++ )
			{
				delete[] Bufs0[ i ];
			}

			delete[] Filters;
		}

		/**
		 * Function updates the resizing filter bank.
		 *
		 * @param la0 Lanczos "a" parameter value (>=2.0), can be fractional.
		 * @param k0 Resizing step.
		 * @param ElCount0 Image's element count, may be used for SIMD filter
		 * tap replication.
		 * @return "True" if an update occured and scanline resizing positions
		 * should be updated unconditionally.
		 */

		bool update( const double la0, const double k0, const int ElCount0 )
		{
			if( la0 == la && k0 == k && ElCount0 == ElCount )
			{
				return( false );
			}

			la = la0;
			k = k0;
			ElCount = ElCount0;

			NormFreq = ( k <= 1.0 ? 1.0 : 1.0 / k );
			Freq = LANCIR_PI * NormFreq;
			FreqA = LANCIR_PI * NormFreq / la;

			Len2 = la / NormFreq;
			fl2 = (int) ceil( Len2 );
			KernelLen = fl2 + fl2;

			#if LANCIR_ALIGN > 4

				ElRepl = ElCount;
				KernelLenA = KernelLen * ElRepl;

				const int elalign =
					(int) ( LANCIR_ALIGN / sizeof( float )) - 1;

				KernelLenA = ( KernelLenA + elalign ) & ~elalign;

			#else // LANCIR_ALIGN > 4

				ElRepl = 1;
				KernelLenA = KernelLen;

			#endif // LANCIR_ALIGN > 4

			FracCount = 1000; // Enough for Lanczos-3 implicit 8-bit precision.

			reallocBuf( Filters, FiltersLen, FracCount + 1 ); // Add +1 to
				// cover cases of fractional delay == 1.0 due to rounding.

			memset( Filters, 0, FiltersLen * sizeof( Filters[ 0 ]));

			setBuf( 0 );

			return( true );
		}

		/**
		 * Function returns filter at the specified fractional offset. This
		 * function can only be called after a prior update() function call.
		 *
		 * @param x Fractional offset, [0; 1].
		 */

		const float* getFilter( const double x )
		{
			const int Frac = (int) ( x * FracCount + 0.5 );
			float* flt = Filters[ Frac ];

			if( flt != NULL )
			{
				return( flt );
			}

			flt = Bufs[ CurBuf ] + CurBufFill * KernelLenA;
			Filters[ Frac ] = flt;
			CurBufFill++;

			if( CurBufFill == BufLen )
			{
				setBuf( CurBuf + 1 );
			}

			makeFilterNorm( flt, 1.0 - (double) Frac / FracCount );

			if( ElRepl > 1 )
			{
				replicateFilter( flt, KernelLen, ElRepl );
			}

			return( flt );
		}

	protected:
		double NormFreq; ///< Normalized frequency of the filter.
			///<
		double Freq; ///< Circular frequency of the filter.
			///<
		double FreqA; ///< Circular frequency of the window function.
			///<
		double Len2; ///< Half resampling filter's length, unrounded.
			///<
		int fl2; ///< Half resampling filter's length, integer.
			///<
		int FracCount; ///< The number of fractional positions for which
			///< filters can be created.
			///<
		int KernelLenA; ///< SIMD-aligned and replicated filter kernel's
			///< length.
			///<
		int ElRepl; ///< The number of repetitions of each filter tap.
			///<
		static const int BufCount = 4; ///< The maximal number of buffers that
			///< can be in use.
			///<
		static const int BufLen = 256; ///< The number of fractional filters
			///< a single buffer may contain. Both "BufLen" and "BufCount"
			///< should correspond to the "FracCount" used.
			///<
		float* Bufs0[ BufCount ]; ///< Buffers that hold all filters,
			///< original.
			///<
		int Bufs0Len[ BufCount ]; ///< Allocated lengthes in "Bufs0", in
			///< "float" elements.
			///<
		float* Bufs[ BufCount ]; ///< Address-aligned "Bufs0".
			///<
		int CurBuf; ///< Filter buffer currently being filled.
			///<
		int CurBufFill; ///< The number of fractional positions filled in the
			///< current filter buffer.
			///<
		float** Filters; ///< Fractional delay filters for all positions.
			///< A particular pointer equals NULL if a filter for such
			///< position has not been created yet.
			///<
		int FiltersLen; ///< Allocated length of Filters, in elements.
			///<
		double la; ///< Current "la".
			///<
		double k; ///< Current "k".
			///<
		int ElCount; ///< Current "ElCount".
			///<

		/**
		 * Function changes the buffer currently being filled, check its
		 * size and reallocates it if necessary, then resets its fill counter.
		 *
		 * @param bi New current buffer index.
		 */

		void setBuf( const int bi )
		{
			reallocBuf( Bufs0[ bi ], Bufs[ bi ], Bufs0Len[ bi ],
				BufLen * KernelLenA );

			CurBuf = bi;
			CurBufFill = 0;
		}

		/**
		 * @brief Sine-wave signal generator class.
		 *
		 * Class implements sine-wave signal generator without biasing, with
		 * constructor-based initialization only. This generator uses an
		 * oscillator instead of the "sin" function.
		 */

		class CSineGen
		{
		public:
			/**
			 * Constructor initializes *this sine-wave signal generator.
			 *
			 * @param si Sine function increment, in radians.
			 * @param ph Starting phase, in radians. Add 0.5 * LANCIR_PI for
			 * cosine function.
			 */

			CSineGen( const double si, const double ph )
				: svalue1( sin( ph ))
				, svalue2( sin( ph - si ))
				, sincr( 2.0 * cos( si ))
			{
			}

			/**
			 * @return The next value of the sine-wave, without biasing.
			 */

			double generate()
			{
				const double res = svalue1;

				svalue1 = sincr * res - svalue2;
				svalue2 = res;

				return( res );
			}

		private:
			double svalue1; ///< Current sine value.
				///<
			double svalue2; ///< Previous sine value.
				///<
			double sincr; ///< Sine value increment.
				///<
		};

		/**
		 * Function creates a filter for the specified fractional delay. The
		 * update() function should be called prior to calling this function.
		 * The created filter is normalized (DC gain=1).
		 *
		 * @param[out] op Output filter buffer.
		 * @param FracDelay Fractional delay, 0 to 1, inclusive.
		 */

		void makeFilterNorm( float* op, const double FracDelay ) const
		{
			CSineGen f( Freq, Freq * ( FracDelay - fl2 ));
			CSineGen fw( FreqA, FreqA * ( FracDelay - fl2 ));

			float* op0 = op;
			double s = 0.0;
			double ut;

			int t = -fl2;

			if( t + FracDelay < -Len2 )
			{
				f.generate();
				fw.generate();
				*op = (float) 0;
				op++;
				t++;
			}

			int IsZeroX = ( fabs( FracDelay - 1.0 ) < 0x1p-42 );
			int mt = 0 - IsZeroX;
			IsZeroX = ( IsZeroX || fabs( FracDelay ) < 0x1p-42 );

			while( t < mt )
			{
				ut = t + FracDelay;
				*op = (float) ( f.generate() * fw.generate() / ( ut * ut ));
				s += *op;
				op++;
				t++;
			}

			if( IsZeroX ) // t+FracDelay==0
			{
				*op = (float) ( Freq * FreqA );
				s += *op;
				f.generate();
				fw.generate();
			}
			else
			{
				ut = FracDelay; // t==0
				*op = (float) ( f.generate() * fw.generate() / ( ut * ut ));
				s += *op;
			}

			mt = fl2 - 2;

			while( t < mt )
			{
				op++;
				t++;
				ut = t + FracDelay;
				*op = (float) ( f.generate() * fw.generate() / ( ut * ut ));
				s += *op;
			}

			op++;
			ut = t + 1 + FracDelay;

			if( ut > Len2 )
			{
				*op = (float) 0;
			}
			else
			{
				*op = (float) ( f.generate() * fw.generate() / ( ut * ut ));
				s += *op;
			}

			s = 1.0 / s;
			t = (int) ( op - op0 + 1 );

			while( t != 0 )
			{
				*op0 = (float) ( *op0 * s );
				op0++;
				t--;
			}
		}

		/**
		 * Function replicates taps of the specified filter so that it can
		 * be used with SIMD loading instructions. This function works
		 * "in-place".
		 *
		 * @param[in,out] p Filter buffer pointer, should be sized to contain
		 * "kl * erp" elements.
		 * @param kl Filter kernel's length, in taps.
		 * @param erp The number of repetitions to apply.
		 */

		static void replicateFilter( float* const p, const int kl,
			const int erp )
		{
			const float* ip = p + kl - 1;
			float* op = p + ( kl - 1 ) * erp;
			int c = kl;

			if( erp == 2 )
			{
				while( c != 0 )
				{
					const float v = *ip;
					op[ 0 ] = v;
					op[ 1 ] = v;
					ip--;
					op -= 2;
					c--;
				}
			}
			else
			if( erp == 3 )
			{
				while( c != 0 )
				{
					const float v = *ip;
					op[ 0 ] = v;
					op[ 1 ] = v;
					op[ 2 ] = v;
					ip--;
					op -= 3;
					c--;
				}
			}
			else // erp == 4
			{
				while( c != 0 )
				{
					const float v = *ip;
					op[ 0 ] = v;
					op[ 1 ] = v;
					op[ 2 ] = v;
					op[ 3 ] = v;
					ip--;
					op -= 4;
					c--;
				}
			}
		}
	};

	/**
	 * Structure defines source scanline positions and filters for each
	 * destination pixel.
	 */

	struct CResizePos
	{
		uintptr_t spo; ///< Source scanline's pixel offset, in bytes.
			///<
		const float* flt; ///< Fractional delay filter.
			///<
		int so; ///< Offset within the source scanline, in pixels.
			///<
	};

	/**
	 * Class contains resizing positions, and prepares source scanline
	 * positions for resize filtering. The public variables become available
	 * after the update() function call.
	 */

	class CResizeScanline
	{
	public:
		int padl; ///< Left-padding (in pixels) required for source scanline.
			///<
		int padr; ///< Right-padding (in pixels) required for source scanline.
			///<
		CResizePos* pos; ///< Source scanline positions (offsets) and filters
			///< for each destination pixel position.
			///<

		CResizeScanline()
			: pos( NULL )
			, poslen( 0 )
			, SrcLen( 0 )
			, DstLen( 0 )
			, o( 0.0 )
		{
		}

		~CResizeScanline()
		{
			delete[] pos;
		}

		/**
		 * Function "resets" *this object so that the next update() call fully
		 * updates the position buffer. Reset is necessary if the filter
		 * object was updated.
		 */

		void reset()
		{
			SrcLen = 0;
		}

		/**
		 * Function updates resizing positions, updates "padl", "padr" and
		 * "pos" buffer.
		 *
		 * @param SrcLen0 Source image scanline length, used to create a
		 * scanline buffer without length pre-calculation.
		 * @param DstLen0 Destination image scanline length.
		 * @param o0 Initial source image offset.
		 * @param rf Resizing filters object.
		 */

		void update( const int SrcLen0, const int DstLen0, const double o0,
			CResizeFilters& rf )
		{
			if( SrcLen0 == SrcLen && DstLen0 == DstLen && o0 == o )
			{
				return;
			}

			SrcLen = SrcLen0;
			DstLen = DstLen0;
			o = o0;

			const int fl2m1 = rf.fl2 - 1;
			padl = fl2m1 - (int) floor( o0 );

			if( padl < 0 )
			{
				padl = 0;
			}

			// Make sure "padr" and "pos" are in sync: calculate ending "pos"
			// offset in advance.

			const double k = rf.k;

			const int DstLen_m1 = DstLen - 1;
			const double oe = o0 + k * DstLen_m1;
			const int ie = (int) floor( oe );

			padr = ie + rf.fl2 + 1 - SrcLen;

			if( padr < 0 )
			{
				padr = 0;
			}

			reallocBuf( pos, poslen, DstLen );

			const size_t ElCountF = rf.ElCount * sizeof( float );
			const int so = padl - fl2m1;
			CResizePos* rp = pos;
			int i;

			for( i = 0; i < DstLen_m1; i++ )
			{
				const double ox = o0 + k * i;
				const int ix = (int) floor( ox );

				rp -> so = so + ix;
				rp -> spo = rp -> so * ElCountF;
				rp -> flt = rf.getFilter( ox - ix );
				rp++;
			}

			rp -> so = so + ie;
			rp -> spo = rp -> so * ElCountF;
			rp -> flt = rf.getFilter( oe - ie );
		}

	protected:
		int poslen; ///< Allocated "pos" buffer's length.
			///<
		int SrcLen; ///< Current SrcLen.
			///<
		int DstLen; ///< Current DstLen.
			///<
		double o; ///< Current "o".
			///<
	};

	CResizeFilters rfv; ///< Resizing filters for vertical resizing.
		///<
	CResizeFilters rfh0; ///< Resizing filters for horizontal resizing (may
		///< not be in use).
		///<
	CResizeScanline rsv; ///< Vertical resize scanline.
		///<
	CResizeScanline rsh; ///< Horizontal resize scanline.
		///<

	/**
	 * Function copies scanline (fully or partially) from the source buffer,
	 * in its native format, to the internal scanline buffer, in preparation
	 * for vertical resizing. Variants for 1-4-channel images.
	 *
	 * @param ip Source scanline buffer pointer.
	 * @param ipinc "ip" increment per pixel.
	 * @param op Output scanline pointer.
	 * @param cc Source pixel copy count.
	 * @param repl Leftmost pixel's replication count.
	 * @param repr Rightmost pixel's replication count.
	 * @tparam T Source buffer's element type.
	 */

	template< typename T >
	static void copyScanline1v( const T* ip, const size_t ipinc, float* op,
		int cc, int repl, int repr )
	{
		float v0;

		if( repl > 0 )
		{
			v0 = (float) ip[ 0 ];

			do
			{
				op[ 0 ] = v0;
				op += 1;

			} while( --repl != 0 );
		}

		while( cc != 0 )
		{
			op[ 0 ] = (float) ip[ 0 ];
			ip += ipinc;
			op += 1;
			cc--;
		}

		if( repr > 0 )
		{
			const T* const ipe = ip - ipinc;
			v0 = (float) ipe[ 0 ];

			do
			{
				op[ 0 ] = v0;
				op += 1;

			} while( --repr != 0 );
		}
	}

	template< typename T >
	static void copyScanline2v( const T* ip, const size_t ipinc, float* op,
		int cc, int repl, int repr )
	{
		float v0, v1;

		if( repl > 0 )
		{
			v0 = (float) ip[ 0 ];
			v1 = (float) ip[ 1 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op += 2;

			} while( --repl != 0 );
		}

		while( cc != 0 )
		{
			op[ 0 ] = (float) ip[ 0 ];
			op[ 1 ] = (float) ip[ 1 ];
			ip += ipinc;
			op += 2;
			cc--;
		}

		if( repr > 0 )
		{
			const T* const ipe = ip - ipinc;
			v0 = (float) ipe[ 0 ];
			v1 = (float) ipe[ 1 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op += 2;

			} while( --repr != 0 );
		}
	}

	template< typename T >
	static void copyScanline3v( const T* ip, const size_t ipinc, float* op,
		int cc, int repl, int repr )
	{
		float v0, v1, v2;

		if( repl > 0 )
		{
			v0 = (float) ip[ 0 ];
			v1 = (float) ip[ 1 ];
			v2 = (float) ip[ 2 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op[ 2 ] = v2;
				op += 3;

			} while( --repl != 0 );
		}

		while( cc != 0 )
		{
			op[ 0 ] = (float) ip[ 0 ];
			op[ 1 ] = (float) ip[ 1 ];
			op[ 2 ] = (float) ip[ 2 ];
			ip += ipinc;
			op += 3;
			cc--;
		}

		if( repr > 0 )
		{
			const T* const ipe = ip - ipinc;
			v0 = (float) ipe[ 0 ];
			v1 = (float) ipe[ 1 ];
			v2 = (float) ipe[ 2 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op[ 2 ] = v2;
				op += 3;

			} while( --repr != 0 );
		}
	}

	template< typename T >
	static void copyScanline4v( const T* ip, const size_t ipinc, float* op,
		int cc, int repl, int repr )
	{
		float v0, v1, v2, v3;

		if( repl > 0 )
		{
			v0 = (float) ip[ 0 ];
			v1 = (float) ip[ 1 ];
			v2 = (float) ip[ 2 ];
			v3 = (float) ip[ 3 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op[ 2 ] = v2;
				op[ 3 ] = v3;
				op += 4;

			} while( --repl != 0 );
		}

		while( cc != 0 )
		{
			op[ 0 ] = (float) ip[ 0 ];
			op[ 1 ] = (float) ip[ 1 ];
			op[ 2 ] = (float) ip[ 2 ];
			op[ 3 ] = (float) ip[ 3 ];
			ip += ipinc;
			op += 4;
			cc--;
		}

		if( repr > 0 )
		{
			const T* const ipe = ip - ipinc;
			v0 = (float) ipe[ 0 ];
			v1 = (float) ipe[ 1 ];
			v2 = (float) ipe[ 2 ];
			v3 = (float) ipe[ 3 ];

			do
			{
				op[ 0 ] = v0;
				op[ 1 ] = v1;
				op[ 2 ] = v2;
				op[ 3 ] = v3;
				op += 4;

			} while( --repr != 0 );
		}
	}

	/**
	 * Function pads the specified scanline buffer to the left and right by
	 * replicating its first and last available pixels, in preparation for
	 * horizontal resizing. Variants for 1-4-channel images.
	 *
	 * @param[in,out] op Scanline buffer to pad.
	 * @param rs Scanline resizing positions object.
	 * @param l Source scanline's length, in pixels.
	 */

	static void padScanline1h( float* op, CResizeScanline& rs, const int l )
	{
		const float* ip = op + rs.padl;

		float v0 = ip[ 0 ];
		int i;

		for( i = 0; i < rs.padl; i++ )
		{
			op[ i ] = v0;
		}

		ip += l;
		op += rs.padl + l;

		v0 = ip[ -1 ];

		for( i = 0; i < rs.padr; i++ )
		{
			op[ i ] = v0;
		}
	}

	static void padScanline2h( float* op, CResizeScanline& rs, const int l )
	{
		const float* ip = op + rs.padl * 2;

		float v0 = ip[ 0 ];
		float v1 = ip[ 1 ];
		int i;

		for( i = 0; i < rs.padl; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op += 2;
		}

		const int lc = l * 2;
		ip += lc;
		op += lc;

		v0 = ip[ -2 ];
		v1 = ip[ -1 ];

		for( i = 0; i < rs.padr; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op += 2;
		}
	}

	static void padScanline3h( float* op, CResizeScanline& rs, const int l )
	{
		const float* ip = op + rs.padl * 3;

		float v0 = ip[ 0 ];
		float v1 = ip[ 1 ];
		float v2 = ip[ 2 ];
		int i;

		for( i = 0; i < rs.padl; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op[ 2 ] = v2;
			op += 3;
		}

		const int lc = l * 3;
		ip += lc;
		op += lc;

		v0 = ip[ -3 ];
		v1 = ip[ -2 ];
		v2 = ip[ -1 ];

		for( i = 0; i < rs.padr; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op[ 2 ] = v2;
			op += 3;
		}
	}

	static void padScanline4h( float* op, CResizeScanline& rs, const int l )
	{
		const float* ip = op + rs.padl * 4;

		float v0 = ip[ 0 ];
		float v1 = ip[ 1 ];
		float v2 = ip[ 2 ];
		float v3 = ip[ 3 ];
		int i;

		for( i = 0; i < rs.padl; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op[ 2 ] = v2;
			op[ 3 ] = v3;
			op += 4;
		}

		const int lc = l * 4;
		ip += lc;
		op += lc;

		v0 = ip[ -4 ];
		v1 = ip[ -3 ];
		v2 = ip[ -2 ];
		v3 = ip[ -1 ];

		for( i = 0; i < rs.padr; i++ )
		{
			op[ 0 ] = v0;
			op[ 1 ] = v1;
			op[ 2 ] = v2;
			op[ 3 ] = v3;
			op += 4;
		}
	}

	/**
	 * Function rounds a value and applies clamping.
	 *
	 * @param v Value to round and clamp.
	 * @param Clamp High clamp level; low level is 0.
	 */

	static inline int roundclamp( const float v, const int Clamp )
	{
		if( v < 0.5f )
		{
			return( 0 );
		}

		const int vr = (int) ( v + 0.5f );

		return( vr > Clamp ? Clamp : vr );
	}

	/**
	 * Function performs final output of the resized scanline pixels to the
	 * destination image buffer.
	 *
	 * @param[in] ip Input resized scanline.
	 * @param[out] op Output image buffer.
	 * @param l Output scanline's size (not pixel count).
	 * @param Clamp Clamp high level, used if "IsOutFloat" is "false".
	 * @param IsOutFloat "True" if floating-point output, and no clamping is
	 * necessary.
	 * @param OutMul Output multiplier, for value range conversion.
	 * @tparam T Output buffer's element type.
	 */

	template< typename T >
	static void copyToOutput( const float* ip, T* op, int l, const int Clamp,
		const bool IsOutFloat, const float OutMul )
	{
		const bool IsUnityMul = ( OutMul == 1.0f );

		if( IsOutFloat )
		{
			if( IsUnityMul )
			{
				if( sizeof( op[ 0 ]) == sizeof( ip[ 0 ]))
				{
					memcpy( op, ip, l * sizeof( op[ 0 ]));
					return;
				}

				int l4 = l >> 2;
				l &= 3;

				while( l4 != 0 )
				{
					op[ 0 ] = (T) ip[ 0 ];
					op[ 1 ] = (T) ip[ 1 ];
					op[ 2 ] = (T) ip[ 2 ];
					op[ 3 ] = (T) ip[ 3 ];
					ip += 4;
					op += 4;
					l4--;
				}

				while( l != 0 )
				{
					*op = (T) *ip;
					ip++;
					op++;
					l--;
				}

				return;
			}

			int l4 = l >> 2;
			l &= 3;
			bool DoScalar = true;

			if( sizeof( op[ 0 ]) == sizeof( ip[ 0 ]))
			{
			#if defined( LANCIR_SSE2 )

				DoScalar = false;
				const __m128 om = _mm_set1_ps( OutMul );

				while( l4 != 0 )
				{
					_mm_storeu_ps( (float*) op,
						_mm_mul_ps( _mm_load_ps( ip ), om ));

					ip += 4;
					op += 4;
					l4--;
				}

			#elif defined( LANCIR_NEON )

				DoScalar = false;
				const float32x4_t om = vdupq_n_f32( OutMul );

				while( l4 != 0 )
				{
					vst1q_f32( (float*) op,
						vmulq_f32( vld1q_f32( ip ), om ));

					ip += 4;
					op += 4;
					l4--;
				}

			#endif // defined( LANCIR_NEON )
			}

			if( DoScalar )
			{
				while( l4 != 0 )
				{
					op[ 0 ] = (T) ( ip[ 0 ] * OutMul );
					op[ 1 ] = (T) ( ip[ 1 ] * OutMul );
					op[ 2 ] = (T) ( ip[ 2 ] * OutMul );
					op[ 3 ] = (T) ( ip[ 3 ] * OutMul );
					ip += 4;
					op += 4;
					l4--;
				}
			}

			while( l != 0 )
			{
				*op = (T) ( *ip * OutMul );
				ip++;
				op++;
				l--;
			}

			return;
		}

		int l4 = l >> 2;
		l &= 3;

	#if defined( LANCIR_SSE2 )

		const __m128 minv = _mm_setzero_ps();
		const __m128 maxv = _mm_set1_ps( (float) Clamp );
		const __m128 om = _mm_set1_ps( OutMul );

		unsigned int prevrm = _MM_GET_ROUNDING_MODE();
		_MM_SET_ROUNDING_MODE( _MM_ROUND_NEAREST );

		if( sizeof( op[ 0 ]) == 4 )
		{
			while( l4 != 0 )
			{
				const __m128 cv = _mm_max_ps( _mm_min_ps(
					_mm_mul_ps( _mm_load_ps( ip ), om ), maxv ), minv );

				_mm_storeu_si128( (__m128i*) op, _mm_cvtps_epi32( cv ));

				ip += 4;
				op += 4;
				l4--;
			}
		}
		else
		if( sizeof( op[ 0 ]) == 2 )
		{
			while( l4 != 0 )
			{
				const __m128 cv = _mm_max_ps( _mm_min_ps(
					_mm_mul_ps( _mm_load_ps( ip ), om ), maxv ), minv );

				const __m128i v32 = _mm_cvtps_epi32( cv );
				const __m128i v16s = _mm_shufflehi_epi16(
					_mm_shufflelo_epi16( v32, 0 | 2 << 2 ), 0 | 2 << 2 );

				const __m128i v16 = _mm_shuffle_epi32( v16s, 0 | 2 << 2 );

				uint64_t tmp[ 2 ];
				_mm_storeu_si128( (__m128i*) tmp, v16 );
				*(uint64_t*) op = tmp[ 0 ];

				ip += 4;
				op += 4;
				l4--;
			}
		}
		else
		{
			while( l4 != 0 )
			{
				const __m128 cv = _mm_max_ps( _mm_min_ps(
					_mm_mul_ps( _mm_load_ps( ip ), om ), maxv ), minv );

				const __m128i v32 = _mm_cvtps_epi32( cv );
				const __m128i v16s = _mm_shufflehi_epi16(
					_mm_shufflelo_epi16( v32, 0 | 2 << 2 ), 0 | 2 << 2 );

				const __m128i v16 = _mm_shuffle_epi32( v16s, 0 | 2 << 2 );
				const __m128i v8 = _mm_packus_epi16( v16, v16 );

				*(uint32_t*) op = _mm_cvtsi128_si32( v8 );

				ip += 4;
				op += 4;
				l4--;
			}
		}

		_MM_SET_ROUNDING_MODE( prevrm );

	#elif defined( LANCIR_NEON )

		const float32x4_t minv = vdupq_n_f32( 0.0f );
		const float32x4_t maxv = vdupq_n_f32( (float) Clamp );
		const float32x4_t om = vdupq_n_f32( OutMul );
		const float32x4_t v05 = vdupq_n_f32( 0.5f );

		if( sizeof( op[ 0 ]) == 4 )
		{
			while( l4 != 0 )
			{
				const float32x4_t cv = vmaxq_f32( vminq_f32(
					vmulq_f32( vld1q_f32( ip ), om ), maxv ), minv );

				vst1q_u32( (uint32_t*) op, vcvtq_u32_f32( vaddq_f32(
					cv, v05 )));

				ip += 4;
				op += 4;
				l4--;
			}
		}
		else
		if( sizeof( op[ 0 ]) == 2 )
		{
			while( l4 != 0 )
			{
				const float32x4_t cv = vmaxq_f32( vminq_f32(
					vmulq_f32( vld1q_f32( ip ), om ), maxv ), minv );

				const uint32x4_t v32 = vcvtq_u32_f32( vaddq_f32( cv, v05 ));
				const uint16x4_t v16 = vmovn_u32( v32 );

				vst1_u16( (uint16_t*) op, v16 );

				ip += 4;
				op += 4;
				l4--;
			}
		}
		else
		{
			while( l4 != 0 )
			{
				const float32x4_t cv = vmaxq_f32( vminq_f32(
					vmulq_f32( vld1q_f32( ip ), om ), maxv ), minv );

				const uint32x4_t v32 = vcvtq_u32_f32( vaddq_f32( cv, v05 ));
				const uint16x4_t v16 = vmovn_u32( v32 );
				const uint8x8_t v8 = vmovn_u16( vcombine_u16( v16, v16 ));

				*(uint32_t*) op = vget_lane_u32( (uint32x2_t) v8, 0 );

				ip += 4;
				op += 4;
				l4--;
			}
		}

	#else // defined( LANCIR_NEON )

		if( IsUnityMul )
		{
			while( l4 != 0 )
			{
				op[ 0 ] = (T) roundclamp( ip[ 0 ], Clamp );
				op[ 1 ] = (T) roundclamp( ip[ 1 ], Clamp );
				op[ 2 ] = (T) roundclamp( ip[ 2 ], Clamp );
				op[ 3 ] = (T) roundclamp( ip[ 3 ], Clamp );
				ip += 4;
				op += 4;
				l4--;
			}
		}
		else
		{
			while( l4 != 0 )
			{
				op[ 0 ] = (T) roundclamp( ip[ 0 ] * OutMul, Clamp );
				op[ 1 ] = (T) roundclamp( ip[ 1 ] * OutMul, Clamp );
				op[ 2 ] = (T) roundclamp( ip[ 2 ] * OutMul, Clamp );
				op[ 3 ] = (T) roundclamp( ip[ 3 ] * OutMul, Clamp );
				ip += 4;
				op += 4;
				l4--;
			}
		}

	#endif // defined( LANCIR_NEON )

		if( IsUnityMul )
		{
			while( l != 0 )
			{
				*op = (T) roundclamp( *ip, Clamp );
				ip++;
				op++;
				l--;
			}
		}
		else
		{
			while( l != 0 )
			{
				*op = (T) roundclamp( *ip * OutMul, Clamp );
				ip++;
				op++;
				l--;
			}
		}
	}

	#define LANCIR_LF_PRE \
			const CResizePos* const rpe = rp + DstLen; \
			while( rp != rpe ) \
			{ \
				const float* ip = (float*) ( (uintptr_t) sp + rp -> spo ); \
				const float* flt = rp -> flt;

	#define LANCIR_LF_POST \
				rp++; \
			}

	/**
	 * Function performs scanline resizing. Variants for 1-4-channel images.
	 *
	 * @param[in] sp Source scanline buffer.
	 * @param[out] op Destination buffer.
	 * @param opinc "op" increment.
	 * @param rp Source scanline offsets and resizing filters.
	 * @param kl Filter kernel's length, in taps (always an even value).
	 * @param DstLen Destination length, in pixels.
	 */

	static void resize1( const float* const sp, float* op, const size_t opinc,
		const CResizePos* rp, const int kl, const int DstLen )
	{
		const int ci = kl >> 2;

		if(( kl & 3 ) == 0 )
		{
			LANCIR_LF_PRE

			int c = ci;

		#if defined( LANCIR_SSE2 )

			__m128 sum = _mm_mul_ps( _mm_load_ps( flt ), _mm_loadu_ps( ip ));

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum = _mm_add_ps( sum, _mm_mul_ps( _mm_load_ps( flt ),
					_mm_loadu_ps( ip )));
			}

			sum = _mm_add_ps( sum, _mm_movehl_ps( sum, sum ));

			float res = _mm_cvtss_f32( _mm_add_ss( sum,
				_mm_shuffle_ps( sum, sum, 1 )));

		#elif defined( LANCIR_NEON )

			float32x4_t sum = vmulq_f32( vld1q_f32( flt ), vld1q_f32( ip ));

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum = vmlaq_f32( sum, vld1q_f32( flt ), vld1q_f32( ip ));
			}

			float res = vaddvq_f32( sum );

		#else // defined( LANCIR_NEON )

			float sum0 = flt[ 0 ] * ip[ 0 ];
			float sum1 = flt[ 1 ] * ip[ 1 ];
			float sum2 = flt[ 2 ] * ip[ 2 ];
			float sum3 = flt[ 3 ] * ip[ 3 ];

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum0 += flt[ 0 ] * ip[ 0 ];
				sum1 += flt[ 1 ] * ip[ 1 ];
				sum2 += flt[ 2 ] * ip[ 2 ];
				sum3 += flt[ 3 ] * ip[ 3 ];
			}

			float res = ( sum0 + sum1 ) + ( sum2 + sum3 );

		#endif // defined( LANCIR_NEON )

			op[ 0 ] = res;
			op += opinc;

			LANCIR_LF_POST
		}
		else
		{
			LANCIR_LF_PRE

			int c = ci;

		#if defined( LANCIR_SSE2 )

			__m128 sum = _mm_mul_ps( _mm_load_ps( flt ), _mm_loadu_ps( ip ));

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum = _mm_add_ps( sum, _mm_mul_ps( _mm_load_ps( flt ),
					_mm_loadu_ps( ip )));
			}

			sum = _mm_add_ps( sum, _mm_movehl_ps( sum, sum ));

			const __m128 sum2 = _mm_mul_ps( _mm_loadu_ps( flt + 2 ),
				_mm_loadu_ps( ip + 2 ));

			sum = _mm_add_ps( sum, _mm_movehl_ps( sum2, sum2 ));

			float res = _mm_cvtss_f32( _mm_add_ss( sum,
				_mm_shuffle_ps( sum, sum, 1 )));

		#elif defined( LANCIR_NEON )

			float32x4_t sum = vmulq_f32( vld1q_f32( flt ), vld1q_f32( ip ));

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum = vmlaq_f32( sum, vld1q_f32( flt ), vld1q_f32( ip ));
			}

			const float32x2_t sum2 = vadd_f32( vget_high_f32( sum ),
				vget_low_f32( sum ));

			float res = vaddv_f32( vmla_f32( sum2, vld1_f32( flt + 4 ),
				vld1_f32( ip + 4 )));

		#else // defined( LANCIR_NEON )

			float sum0 = flt[ 0 ] * ip[ 0 ];
			float sum1 = flt[ 1 ] * ip[ 1 ];
			float sum2 = flt[ 2 ] * ip[ 2 ];
			float sum3 = flt[ 3 ] * ip[ 3 ];

			while( --c != 0 )
			{
				flt += 4;
				ip += 4;
				sum0 += flt[ 0 ] * ip[ 0 ];
				sum1 += flt[ 1 ] * ip[ 1 ];
				sum2 += flt[ 2 ] * ip[ 2 ];
				sum3 += flt[ 3 ] * ip[ 3 ];
			}

			float res = ( sum0 + sum1 ) + ( sum2 + sum3 );

			res += flt[ 4 ] * ip[ 4 ] + flt[ 5 ] * ip[ 5 ];

		#endif // defined( LANCIR_NEON )

			op[ 0 ] = res;
			op += opinc;

			LANCIR_LF_POST
		}
	}

	static void resize2( const float* const sp, float* op, const size_t opinc,
		const CResizePos* rp, const int kl, const int DstLen )
	{
	#if LANCIR_ALIGN > 4
		const int ci = kl >> 2;
		const int cir = kl & 3;
	#else // LANCIR_ALIGN > 4
		const int ci = kl >> 1;
	#endif // LANCIR_ALIGN > 4

		LANCIR_LF_PRE

		int c = ci;

	#if defined( LANCIR_AVX )

		__m256 sum = _mm256_mul_ps( _mm256_load_ps( flt ),
			_mm256_loadu_ps( ip ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sum = _mm256_add_ps( sum, _mm256_mul_ps( _mm256_load_ps( flt ),
				_mm256_loadu_ps( ip )));
		}

		__m128 res = _mm_add_ps( _mm256_extractf128_ps( sum, 0 ),
			_mm256_extractf128_ps( sum, 1 ));

		if( cir == 2 )
		{
			res = _mm_add_ps( res, _mm_mul_ps( _mm_load_ps( flt + 8 ),
				_mm_loadu_ps( ip + 8 )));
		}

		res = _mm_add_ps( res, _mm_movehl_ps( res, res ));

		_mm_store_ss( op, res );
		_mm_store_ss( op + 1, _mm_shuffle_ps( res, res, 1 ));

	#elif defined( LANCIR_SSE2 )

		__m128 sumA = _mm_mul_ps( _mm_load_ps( flt ), _mm_loadu_ps( ip ));
		__m128 sumB = _mm_mul_ps( _mm_load_ps( flt + 4 ),
			_mm_loadu_ps( ip + 4 ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt ),
				_mm_loadu_ps( ip )));

			sumB = _mm_add_ps( sumB, _mm_mul_ps( _mm_load_ps( flt + 4 ),
				_mm_loadu_ps( ip + 4 )));
		}

		sumA = _mm_add_ps( sumA, sumB );

		if( cir == 2 )
		{
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt + 8 ),
				_mm_loadu_ps( ip + 8 )));
		}

		sumA = _mm_add_ps( sumA, _mm_movehl_ps( sumA, sumA ));

		_mm_store_ss( op, sumA );
		_mm_store_ss( op + 1, _mm_shuffle_ps( sumA, sumA, 1 ));

	#elif defined( LANCIR_NEON )

		float32x4_t sumA = vmulq_f32( vld1q_f32( flt ), vld1q_f32( ip ));
		float32x4_t sumB = vmulq_f32( vld1q_f32( flt + 4 ),
			vld1q_f32( ip + 4 ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sumA = vmlaq_f32( sumA, vld1q_f32( flt ), vld1q_f32( ip ));
			sumB = vmlaq_f32( sumB, vld1q_f32( flt + 4 ),
				vld1q_f32( ip + 4 ));
		}

		sumA = vaddq_f32( sumA, sumB );

		if( cir == 2 )
		{
			sumA = vmlaq_f32( sumA, vld1q_f32( flt + 8 ),
				vld1q_f32( ip + 8 ));
		}

		vst1_f32( op, vadd_f32( vget_high_f32( sumA ), vget_low_f32( sumA )));

	#else // defined( LANCIR_NEON )

		const float xx = flt[ 0 ];
		const float xx2 = flt[ 1 ];
		float sum0 = xx * ip[ 0 ];
		float sum1 = xx * ip[ 1 ];
		float sum2 = xx2 * ip[ 2 ];
		float sum3 = xx2 * ip[ 3 ];

		while( --c != 0 )
		{
			flt += 2;
			ip += 4;
			const float xx = flt[ 0 ];
			const float xx2 = flt[ 1 ];
			sum0 += xx * ip[ 0 ];
			sum1 += xx * ip[ 1 ];
			sum2 += xx2 * ip[ 2 ];
			sum3 += xx2 * ip[ 3 ];
		}

		op[ 0 ] = sum0 + sum2;
		op[ 1 ] = sum1 + sum3;

	#endif // defined( LANCIR_NEON )

		op += opinc;

		LANCIR_LF_POST
	}

	static void resize3( const float* const sp, float* op, const size_t opinc,
		const CResizePos* rp, const int kl, const int DstLen )
	{
	#if LANCIR_ALIGN > 4

		const int ci = kl >> 2;
		const int cir = kl & 3;

		LANCIR_LF_PRE

		float res[ 12 ];
		int c = ci;

		#if defined( LANCIR_AVX )

		__m128 sumA = _mm_mul_ps( _mm_load_ps( flt ), _mm_loadu_ps( ip ));
		__m256 sumB = _mm256_mul_ps( _mm256_loadu_ps( flt + 4 ),
			_mm256_loadu_ps( ip + 4 ));

		while( --c != 0 )
		{
			flt += 12;
			ip += 12;
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt ),
				_mm_loadu_ps( ip )));

			sumB = _mm256_add_ps( sumB, _mm256_mul_ps(
				_mm256_loadu_ps( flt + 4 ), _mm256_loadu_ps( ip + 4 )));
		}

		if( cir == 2 )
		{
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt + 12 ),
				_mm_loadu_ps( ip + 12 )));
		}

		_mm_storeu_ps( res, sumA );

		float o0 = res[ 0 ] + res[ 3 ];
		float o1 = res[ 1 ];
		float o2 = res[ 2 ];

		_mm256_storeu_ps( res + 4, sumB );

		o1 += res[ 4 ];
		o2 += res[ 5 ];

		#elif defined( LANCIR_SSE2 )

		__m128 sumA = _mm_mul_ps( _mm_load_ps( flt ), _mm_loadu_ps( ip ));
		__m128 sumB = _mm_mul_ps( _mm_load_ps( flt + 4 ),
			_mm_loadu_ps( ip + 4 ));

		__m128 sumC = _mm_mul_ps( _mm_load_ps( flt + 8 ),
			_mm_loadu_ps( ip + 8 ));

		while( --c != 0 )
		{
			flt += 12;
			ip += 12;
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt ),
				_mm_loadu_ps( ip )));

			sumB = _mm_add_ps( sumB, _mm_mul_ps( _mm_load_ps( flt + 4 ),
				_mm_loadu_ps( ip + 4 )));

			sumC = _mm_add_ps( sumC, _mm_mul_ps( _mm_load_ps( flt + 8 ),
				_mm_loadu_ps( ip + 8 )));
		}

		if( cir == 2 )
		{
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt + 12 ),
				_mm_loadu_ps( ip + 12 )));
		}

		_mm_storeu_ps( res, sumA );
		_mm_storeu_ps( res + 4, sumB );

		float o0 = res[ 0 ] + res[ 3 ];
		float o1 = res[ 1 ] + res[ 4 ];
		float o2 = res[ 2 ] + res[ 5 ];

		_mm_storeu_ps( res + 8, sumC );

		#elif defined( LANCIR_NEON )

		float32x4_t sumA = vmulq_f32( vld1q_f32( flt ), vld1q_f32( ip ));
		float32x4_t sumB = vmulq_f32( vld1q_f32( flt + 4 ),
			vld1q_f32( ip + 4 ));

		float32x4_t sumC = vmulq_f32( vld1q_f32( flt + 8 ),
			vld1q_f32( ip + 8 ));

		while( --c != 0 )
		{
			flt += 12;
			ip += 12;
			sumA = vmlaq_f32( sumA, vld1q_f32( flt ), vld1q_f32( ip ));
			sumB = vmlaq_f32( sumB, vld1q_f32( flt + 4 ),
				vld1q_f32( ip + 4 ));

			sumC = vmlaq_f32( sumC, vld1q_f32( flt + 8 ),
				vld1q_f32( ip + 8 ));
		}

		if( cir == 2 )
		{
			sumA = vmlaq_f32( sumA, vld1q_f32( flt + 12 ),
				vld1q_f32( ip + 12 ));
		}

		vst1q_f32( res, sumA );
		vst1q_f32( res + 4, sumB );

		float o0 = res[ 0 ] + res[ 3 ];
		float o1 = res[ 1 ] + res[ 4 ];
		float o2 = res[ 2 ] + res[ 5 ];

		vst1q_f32( res + 8, sumC );

		#endif // defined( LANCIR_NEON )

		o0 += res[ 6 ] + res[ 9 ];
		o1 += res[ 7 ] + res[ 10 ];
		o2 += res[ 8 ] + res[ 11 ];

		if( cir == 2 )
		{
			o1 += flt[ 16 ] * ip[ 16 ];
			o2 += flt[ 17 ] * ip[ 17 ];
		}

		op[ 0 ] = o0;
		op[ 1 ] = o1;
		op[ 2 ] = o2;

	#else // LANCIR_ALIGN > 4

		const int ci = kl >> 1;

		LANCIR_LF_PRE

		int c = ci;

		const float xx = flt[ 0 ];
		float sum0 = xx * ip[ 0 ];
		float sum1 = xx * ip[ 1 ];
		float sum2 = xx * ip[ 2 ];
		const float xx2 = flt[ 1 ];
		float sum3 = xx2 * ip[ 3 ];
		float sum4 = xx2 * ip[ 4 ];
		float sum5 = xx2 * ip[ 5 ];

		while( --c != 0 )
		{
			flt += 2;
			ip += 6;
			const float xx = flt[ 0 ];
			sum0 += xx * ip[ 0 ];
			sum1 += xx * ip[ 1 ];
			sum2 += xx * ip[ 2 ];
			const float xx2 = flt[ 1 ];
			sum3 += xx2 * ip[ 3 ];
			sum4 += xx2 * ip[ 4 ];
			sum5 += xx2 * ip[ 5 ];
		}

		op[ 0 ] = sum0 + sum3;
		op[ 1 ] = sum1 + sum4;
		op[ 2 ] = sum2 + sum5;

	#endif // LANCIR_ALIGN > 4

		op += opinc;

		LANCIR_LF_POST
	}

	static void resize4( const float* const sp, float* op, const size_t opinc,
		const CResizePos* rp, const int kl, const int DstLen )
	{
	#if LANCIR_ALIGN > 4
		const int ci = kl >> 1;
	#else // LANCIR_ALIGN > 4
		const int ci = kl;
	#endif // LANCIR_ALIGN > 4

		LANCIR_LF_PRE

		int c = ci;

	#if defined( LANCIR_AVX )

		__m256 sum = _mm256_mul_ps( _mm256_load_ps( flt ),
			_mm256_loadu_ps( ip ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sum = _mm256_add_ps( sum, _mm256_mul_ps( _mm256_load_ps( flt ),
				_mm256_loadu_ps( ip )));
		}

		_mm_store_ps( op, _mm_add_ps( _mm256_extractf128_ps( sum, 0 ),
			_mm256_extractf128_ps( sum, 1 )));

	#elif defined( LANCIR_SSE2 )

		__m128 sumA = _mm_mul_ps( _mm_load_ps( flt ), _mm_load_ps( ip ));
		__m128 sumB = _mm_mul_ps( _mm_load_ps( flt + 4 ),
			_mm_load_ps( ip + 4 ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sumA = _mm_add_ps( sumA, _mm_mul_ps( _mm_load_ps( flt ),
				_mm_load_ps( ip )));

			sumB = _mm_add_ps( sumB, _mm_mul_ps( _mm_load_ps( flt + 4 ),
				_mm_load_ps( ip + 4 )));
		}

		_mm_store_ps( op, _mm_add_ps( sumA, sumB ));

	#elif defined( LANCIR_NEON )

		float32x4_t sumA = vmulq_f32( vld1q_f32( flt ), vld1q_f32( ip ));
		float32x4_t sumB = vmulq_f32( vld1q_f32( flt + 4 ),
			vld1q_f32( ip + 4 ));

		while( --c != 0 )
		{
			flt += 8;
			ip += 8;
			sumA = vmlaq_f32( sumA, vld1q_f32( flt ), vld1q_f32( ip ));
			sumB = vmlaq_f32( sumB, vld1q_f32( flt + 4 ),
				vld1q_f32( ip + 4 ));
		}

		vst1q_f32( op, vaddq_f32( sumA, sumB ));

	#else // defined( LANCIR_NEON )

		const float xx = flt[ 0 ];
		float sum0 = xx * ip[ 0 ];
		float sum1 = xx * ip[ 1 ];
		float sum2 = xx * ip[ 2 ];
		float sum3 = xx * ip[ 3 ];

		while( --c != 0 )
		{
			flt++;
			ip += 4;
			const float xx = flt[ 0 ];
			sum0 += xx * ip[ 0 ];
			sum1 += xx * ip[ 1 ];
			sum2 += xx * ip[ 2 ];
			sum3 += xx * ip[ 3 ];
		}

		op[ 0 ] = sum0;
		op[ 1 ] = sum1;
		op[ 2 ] = sum2;
		op[ 3 ] = sum3;

	#endif // defined( LANCIR_NEON )

		op += opinc;

		LANCIR_LF_POST
	}

	#undef LANCIR_LF_PRE
	#undef LANCIR_LF_POST
};

#undef LANCIR_PI
#undef LANCIR_ALIGN

} // namespace avir

#endif // AVIR_CLANCIR_INCLUDED

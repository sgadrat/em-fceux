/* FCE Ultra - NES/Famicom Emulator - NTSC Emulation
 *
 * Copyright notice for this file:
 *  Copyright (C) 2016 Valtteri "tsone" Heikkila
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "ntsc.h"
#include <cmath>
#include <cstdlib>


// Bounds for signal level normalization
#define ATTENUATION 0.746
#define LOWEST      (0.350 * ATTENUATION)
#define HIGHEST     1.962
#define BLACK       0.518
#define WHITE       HIGHEST

// Half-width of Y and C box filter kernels.
#define YW2	6.0
#define CW2	12.0

// Square wave generator as function of NES pal chroma index and phase.
#define IN_COLOR_PHASE(color_, phase_) (((color_) + (phase_)) % 12 < 6)

// Box filter kernel.
#define BOX_FILTER(w2_, center_, x_) (fabs((x_) - (center_)) < (w2_) ? 1.0 : 0.0)


static double *s_yiqs = 0;
static unsigned char *s_lookup_tex = 0;
static NTSCControls s_c = {
	.yiq_mins = {  1e9f,  1e9f,  1e9f },
	.yiq_maxs = { -1e9f, -1e9f, -1e9f }
};


// TODO: tsone: following must match with shaders common
static const double c_convMat[] = {
	1.0,        1.0,        1.0,       // Y
	0.946882,   -0.274788,  -1.108545, // I
	0.623557,   -0.635691,  1.709007   // Q
};
void ntscYIQ2RGB(double *rgb, const double *yiq)
{
	double y = yiq[0];
	double i = yiq[1] * s_c.color;
	double q = yiq[2] * s_c.color;

	double r = c_convMat[0]*y + c_convMat[3]*i + c_convMat[6]*q;
	double g = c_convMat[1]*y + c_convMat[4]*i + c_convMat[7]*q;
	double b = c_convMat[2]*y + c_convMat[5]*i + c_convMat[8]*q;

	r = s_c.contrast * pow((r < 0) ? 0 : r, s_c.gamma) + s_c.brightness;
	g = s_c.contrast * pow((g < 0) ? 0 : g, s_c.gamma) + s_c.brightness;
	b = s_c.contrast * pow((b < 0) ? 0 : b, s_c.gamma) + s_c.brightness;

	rgb[0] = (r > 1) ? 1 : (r < 0) ? 0 : r;
	rgb[1] = (g > 1) ? 1 : (g < 0) ? 0 : g;
	rgb[2] = (b > 1) ? 1 : (b < 0) ? 0 : b;
}

void ntscSetControls(double brightness, double contrast, double color, double gamma)
{
	s_c.brightness = brightness;
	s_c.contrast = contrast;
	s_c.color = color;
	s_c.gamma = gamma;
}

const NTSCControls &ntscGetControls()
{
	return s_c;
}

// This source code is modified from original at:
// http://wiki.nesdev.com/w/index.php/NTSC_video
// This version includes a fix to emphasis and applies normalization.
// Inputs:
//   pixel = Pixel color (9-bit) given as input. Bitmask format: "eeellcccc".
//   phase = Signal phase. It is a variable that increases by 8 each pixel.
static double ntscSignalLevel(int pixel, int phase)
{
	// Voltage levels, relative to synch voltage
	const float levels[8] = {
		0.350, 0.518, 0.962, 1.550, // Signal low
		1.094, 1.506, 1.962, 1.962  // Signal high
	};

	// Decode the NES color.
	int color = (pixel & 0x0F);	// 0..15 "cccc"
	int level = (pixel >> 4) & 3;  // 0..3  "ll"
	int emphasis = (pixel >> 6);   // 0..7  "eee"
	if (color > 0x0D) { level = 1; } // Level 1 forced for colors $0E..$0F

	// The square wave for this color alternates between these two voltages:
	double low  = levels[0 + level];
	double high = levels[4 + level];
	if (color == 0) { low = high; } // For color 0, only high level is emitted
	if (color > 0x0C) { high = low; } // For colors $0D..$0F, only low level is emitted

	double signal = IN_COLOR_PHASE(color, phase) ? high : low;

	// When de-emphasis bits are set, some parts of the signal are attenuated:
	if ((color < 0x0E) && ( // Not for colors $0E..$0F (Wiki sample code doesn't have this)
			((emphasis & 1) && IN_COLOR_PHASE(0, phase))
			|| ((emphasis & 2) && IN_COLOR_PHASE(4, phase))
			|| ((emphasis & 4) && IN_COLOR_PHASE(8, phase)))) {
		signal = signal * ATTENUATION;
	}

	// Normalize to desired black and white range. This is a linear operation.
	signal = ((signal-BLACK) / (WHITE-BLACK));

	return signal;
}

// 1D comb filter which relies on the inverse phase of the adjacent even and odd fields.
// Ideally this allows extracting accurate luma and chroma. With NES PPU however, phase of
// the fields is slightly off. More specifically, we'd need phase difference of 6 samples
// (=half chroma wavelength), but we get 8 which comes from the discarded 1st pixel of
// the odd field. The error is 2 samples or 60 degrees. The error is fixed by adjusting
// the phase of the cos-sin (demodulator, not done in this function...).
static void comb1D(double *result, double level0, double level1, double x)
{
	// Apply the 1D comb to separate luma and chroma.
	double y = (level0+level1) / 2.0;
	double c = (level0-level1) / 2.0;
	double a = (2.0*M_PI/12.0) * x;
	// Demodulate and store YIQ result.
	result[0] = y;
// TODO: tsone: battling with scalers... weird
	// This scaler (8/6) was found with experimentation. Something with inadequate sampling?
//	result[1] = 1.333*c * cos(a);
//	result[2] = 1.333*c * sin(a);
// TODO: tsone: old scalers? seem to over-saturate. no idea where these from
	result[1] = 1.400*c * cos(a); // <- Reduce this scaler to make image less "warm".
	result[2] = 1.400*c * sin(a);
}

static void adjustYIQLimits(double *yiq)
{
	for (int i = 0; i < 3; i++) {
		s_c.yiq_mins[i] = fmin(s_c.yiq_mins[i], yiq[i]);
		s_c.yiq_maxs[i] = fmax(s_c.yiq_maxs[i], yiq[i]);
	}
}

// Generate NTSC YIQ lookup table. Does it only once at startup.
static void ntscPrepareLookupInternal()
{
	if (s_yiqs) {
		return;
	}

	double *ys = (double*) calloc(3*8 * NUM_PHASES*NUM_COLORS, sizeof(double));
	s_yiqs = (double*) calloc(3 * LOOKUP_W * NUM_COLORS, sizeof(double));

	// Generate temporary lookup containing samplings of separated and normalized YIQ components
	// for each phase and color combination. Separation is performed using a simulated 1D comb filter.
	int i = 0;
	for (int phase = 0; phase < NUM_PHASES; phase++) {
		// PPU color generator outputs 8 samples per pixel, and we have 3 different phases.
		const int phase0 = 8 * phase;
		// While even field is normal, PPU skips 1st pixel of the odd field, causing offset of 8 samples.
		const int phase1 = phase0 + 8;
		// Phase (hue) shift for demodulation. 180-33 degree shift from NTSC standard.
		const double shift = phase0 + (12.0/360.0) * (180.0-33.0);

		for (int color = 0; color < NUM_COLORS; color++) {
			// Here we store the eight (8) generated YIQ samples for the pixel.
			for (int s = 0; s < 8; s++) {
				// Obtain NTSC signal level from PPU color generator for both fields.
				double level0 = ntscSignalLevel(color, phase0+s);
				double level1 = ntscSignalLevel(color, phase1+s);
				comb1D(&ys[i], level0, level1, shift + s);
				i += 3;
			}
		}
	}

	// Generate an exhausting lookup texture for every color, phase, tap and subpixel combination.
	// ...Looks horrid, and yes it's complex, but computation is quite fast.
	for (int color = 0; color < NUM_COLORS; color++) {
		for (int phase = 0; phase < NUM_PHASES; phase++) {
			for (int subp = 0; subp < NUM_SUBPS; subp++) {
				for (int tap = 0; tap < NUM_TAPS; tap++) {
					const int k = 3 * (color*LOOKUP_W + phase*NUM_SUBPS*NUM_TAPS + subp*NUM_TAPS + tap);
					double *yiq = &s_yiqs[k];

					// Because of half subpixel accuracy (4 vs 8), filter twice and average.
					for (int side = 0; side < 2; side++) { // 0:left, 1: right
						// Calculate filter kernel center.
						const double kernel_center = (side + 2*subp + 8*(NUM_TAPS/2)) + 0.5;

						// Accumulate filter sum over all 8 samples of the pixel.
						for (int s = 0; s < 8; s++) {
							// Calculate x in kernel.
							const double x = s + 8.0*tap;
							// Filter luma and chroma with different filter widths.
							double my = BOX_FILTER(YW2, kernel_center, x) / (2.0*8.0);
							double mc = BOX_FILTER(CW2, kernel_center, x) / (2.0*8.0);
							// Lookup YIQ signal level and accumulate.
							i = 3 * (8*(color + phase*NUM_COLORS) + s);
							yiq[0] += my * ys[i  ];
							yiq[1] += mc * ys[i+1];
							yiq[2] += mc * ys[i+2];
						}
					}

					adjustYIQLimits(yiq);
				}
			}
		}
	}

	// Make RGB PPU palette similarly but having 12 samples per color.
	for (int color = 0; color < NUM_COLORS; color++) {
		// For some reason we need additional shift of 1 sample (-30 degrees).
		const double shift = (12.0/360.0) * (180.0-30.0-33.0);
		double yiq[3] = {0, 0, 0};

		for (int s = 0; s < 12; s++) {
			double level0 = ntscSignalLevel(color, s  ) / 12.0;
			double level1 = ntscSignalLevel(color, s+6) / 12.0; // Perfect chroma cancellation.
			double t[3];
			comb1D(t, level0, level1, shift + s);
			yiq[0] += t[0];
			yiq[1] += t[1];
			yiq[2] += t[2];
		}

		adjustYIQLimits(yiq);

		const int k = 3 * (color*LOOKUP_W + LOOKUP_W-1);
		s_yiqs[k  ] = yiq[0];
		s_yiqs[k+1] = yiq[1];
		s_yiqs[k+2] = yiq[2];
	}

	free(ys);
}

const double *ntscGetLookup()
{
        ntscPrepareLookupInternal();
	return s_yiqs;
}

const unsigned char *ntscGetLookupTex()
{
	if (s_lookup_tex) {
		return s_lookup_tex;
	}
        ntscPrepareLookupInternal();

	unsigned char *result = (unsigned char*) calloc(3 * LOOKUP_W * NUM_COLORS, sizeof(unsigned char));
	s_lookup_tex = result;

	// Create lookup texture RGB as bytes by mapping voltages to the min-max range.
	// The conversion to bytes will lose some precision, which is unnoticeable however.
	for (int k = 0; k < 3 * LOOKUP_W * NUM_COLORS; k+=3) {
		for (int i = 0; i < 3; i++) {
			const double v = (s_yiqs[k+i]-s_c.yiq_mins[i]) / (s_c.yiq_maxs[i]-s_c.yiq_mins[i]);
			result[k+i] = (unsigned char) (255.0*v + 0.5);
		}
	}

	return result;
}


/* FCE Ultra - NES/Famicom Emulator - Emscripten OpenGL ES 2.0 driver
 *
 * Copyright notice for this file:
 *  Copyright (C) 2015 Valtteri "tsone" Heikkila
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
#include "em.h"
#include "es2.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <emscripten.h>
#include <emscripten/html5.h>
#include "es2util.h"
#include "meshes.h"

#define STR(s_) _STR(s_)
#define _STR(s_) #s_
#define ARRAY_SIZE(a_) (sizeof(a_) / sizeof(*(a_)))

// TODO: tsone: set elsewhere?
#define DBG_MODE 0
#if DBG_MODE
#define DBG(x_) x_;
#else
#define DBG(x_)
#endif

#define IDX_I		0
#define DEEMP_I		1
#define LOOKUP_I	2
#define RGB_I		3
#define STRETCH_I	4
// NOTE: tsone: tv and downsample texture must be in incremental order
#define TV_I		5
#define DOWNSAMPLE0_I	6
#define DOWNSAMPLE1_I	7
#define DOWNSAMPLE2_I	8
#define DOWNSAMPLE3_I	9
#define DOWNSAMPLE4_I	10
#define DOWNSAMPLE5_I	11
#define NOISE_I		12
#define SHARPEN_I	13

#define TEX(i_) (GL_TEXTURE0+(i_))

#define PERSISTENCE_R	0.165 // Red phosphor persistence.
#define PERSISTENCE_G	0.205 // Green "
#define PERSISTENCE_B	0.225 // Blue "

// Bounds for signal level normalization
#define ATTENUATION 0.746
#define LOWEST      (0.350 * ATTENUATION)
#define HIGHEST     1.962
#define BLACK       0.518
#define WHITE       HIGHEST

#define NUM_PHASES	3
#define NUM_SUBPS	4
#define NUM_TAPS	5
#define NOISE_W		256
#define NOISE_H		256
#define RGB_W		(NUM_SUBPS * IDX_W)
#define SCREEN_W	(NUM_SUBPS * INPUT_W)
#define SCREEN_H	(4 * IDX_H)
// Half-width of Y and C box filter kernels.
#define YW2	6.0
#define CW2	12.0

// Square wave generator as function of NES pal chroma index and phase.
#define IN_COLOR_PHASE(color_, phase_) (((color_) + (phase_)) % 12 < 6)

static es2 s_p;
static es2_uniforms s_u;

static const char common_src[] = "precision mediump float;\n";

static const GLint mesh_quad_vert_num = 4;
static const GLint mesh_quad_face_num = 2;
static const GLfloat mesh_quad_verts[] = {
	-1.0f, -1.0f, 0.0f,
	 1.0f, -1.0f, 0.0f,
	-1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f, 0.0f
};
static const GLfloat mesh_quad_uvs[] = {
	 0.0f,  0.0f,
	 1.0f,  0.0f,
	 0.0f,  1.0f,
	 1.0f,  1.0f
};
static const GLfloat mesh_quad_norms[] = {
	 0.0f,  0.0f, 1.0f,
	 0.0f,  0.0f, 1.0f,
	 0.0f,  0.0f, 1.0f,
	 0.0f,  0.0f, 1.0f
};
static es2_varray mesh_quad_varrays[] = {
	{ 3, GL_FLOAT, 0, (const void*) mesh_quad_verts },
	{ 3, GL_FLOAT, 0, (const void*) mesh_quad_norms },
	{ 2, GL_FLOAT, 0, (const void*) mesh_quad_uvs }
};

static es2_varray mesh_screen_varrays[] = {
	{ 3, GL_FLOAT, 0, (const void*) mesh_screen_verts },
	{ 3, GL_FLOAT, VARRAY_ENCODED_NORMALS, (const void*) mesh_screen_norms },
	{ 2, GL_FLOAT, 0, (const void*) mesh_screen_uvs }
};

static es2_varray mesh_rim_varrays[] = {
	{ 3, GL_FLOAT, 0, (const void*) mesh_rim_verts },
	{ 3, GL_FLOAT, VARRAY_ENCODED_NORMALS, (const void*) mesh_rim_norms },
	{ 3, GL_FLOAT, 0, 0 },
//	{ 3, GL_FLOAT, (const void*) mesh_rim_vcols }
	{ 3, GL_UNSIGNED_BYTE, 0, (const void*) mesh_rim_vcols }
};

// Texture sample offsets and weights for gaussian w/ radius=8, sigma=4.
// Eliminates aliasing and blurs for "faked" glossy reflections and AO.
static const GLfloat s_downsample_offs[] = { -6.892337f, -4.922505f, -2.953262f, -0.98438f, 0.98438f, 2.953262f, 4.922505f, 6.892337f };
static const GLfloat s_downsample_ws[] = { 0.045894f, 0.096038f, 0.157115f, 0.200954f, 0.200954f, 0.157115f, 0.096038f, 0.045894f };

static const int s_downsample_widths[]  = { SCREEN_W, SCREEN_W/4, SCREEN_W/4,  SCREEN_W/16, SCREEN_W/16, SCREEN_W/64, SCREEN_W/64 };
static const int s_downsample_heights[] = { SCREEN_H, SCREEN_H,   SCREEN_H/4,  SCREEN_H/4,  SCREEN_H/16, SCREEN_H/16, SCREEN_H/64 };


static void updateSharpenKernel()
{
	glUseProgram(s_p.sharpen_prog);
	double v = GetController(FCEM_NTSC_EMU) * 0.4 * (GetController(FCEM_SHARPNESS)+0.5);
	GLfloat sharpen_kernel[] = {
		-v, -v, -v,
		1, 0, 0,
		2*v, 1+2*v, 2*v,
		0, 0, 1,
		-v, -v, -v
	};
	glUniform3fv(s_u._sharpen_kernel_loc, 5, sharpen_kernel);
}

// This source code is modified from original at:
// http://wiki.nesdev.com/w/index.php/NTSC_video
// This version includes a fix to emphasis and applies normalization.
// Inputs:
//   pixel = Pixel color (9-bit) given as input. Bitmask format: "eeellcccc".
//   phase = Signal phase. It is a variable that increases by 8 each pixel.
static double NTSCsignal(int pixel, int phase)
{
	// Voltage levels, relative to synch voltage
	const GLfloat levels[8] = {
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
		s_p.yiq_mins[i] = fmin(s_p.yiq_mins[i], yiq[i]);
		s_p.yiq_maxs[i] = fmax(s_p.yiq_maxs[i], yiq[i]);
	}
}

// Box filter kernel.
#define BOX_FILTER(w2_, center_, x_) (fabs((x_) - (center_)) < (w2_) ? 1.0 : 0.0)

double *yiqs = 0;

// Generate NTSC YIQ lookup table
void genNTSCLookup()
{
	if (yiqs) {
		return;
	}

	double *ys = (double*) calloc(3*8 * NUM_PHASES*NUM_COLORS, sizeof(double));
	yiqs = (double*) calloc(3 * LOOKUP_W * NUM_COLORS, sizeof(double));

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
				double level0 = NTSCsignal(color, phase0+s);
				double level1 = NTSCsignal(color, phase1+s);
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
					double *yiq = &yiqs[k];

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
							yiq[0] += my * ys[i+0];
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
			double level0 = NTSCsignal(color, s) / 12.0;
			double level1 = NTSCsignal(color, s+6) / 12.0; // Perfect chroma cancellation.
			double t[3];
			comb1D(t, level0, level1, shift + s);
			yiq[0] += t[0];
			yiq[1] += t[1];
			yiq[2] += t[2];
		}

		adjustYIQLimits(yiq);

		const int k = 3 * (color*LOOKUP_W + LOOKUP_W-1);
		yiqs[k+0] = yiq[0];
		yiqs[k+1] = yiq[1];
		yiqs[k+2] = yiq[2];
	}

	free(ys);
}

// Generate lookup texture.
static void genLookupTex()
{
	genNTSCLookup();

	unsigned char *result = (unsigned char*) calloc(3 * LOOKUP_W * NUM_COLORS, sizeof(unsigned char));

	// Create lookup texture RGB as bytes by mapping voltages to the min-max range.
	// The conversion to bytes will lose some precision, which is unnoticeable however.
	for (int k = 0; k < 3 * LOOKUP_W * NUM_COLORS; k+=3) {
		for (int i = 0; i < 3; i++) {
			const double v = (yiqs[k+i]-s_p.yiq_mins[i]) / (s_p.yiq_maxs[i]-s_p.yiq_mins[i]);
			result[k+i] = (unsigned char) (255.0*v + 0.5);
		}
	}

	glActiveTexture(TEX(LOOKUP_I));
	createTex(&s_p.lookup_tex, LOOKUP_W, NUM_COLORS, GL_RGB, GL_NEAREST, GL_CLAMP_TO_EDGE, result);

	free(result);
}

// Get uniformly distributed random number in [0,1] range.
static double rand01()
{
	return emscripten_random();
}

static void genNoiseTex()
{
	GLubyte *noise = (GLubyte*) malloc(NOISE_W * NOISE_H);

	// Box-Muller method gaussian noise.
	// Results are clamped to 0..255 range, which skews the distribution slightly.
	const double SIGMA = 0.5/2.0; // Set 95% of noise values in [-0.5,0.5] range.
	const double MU = 0.5; // Offset range by +0.5 to map to [0,1].
	for (int i = 0; i < NOISE_W*NOISE_H; i++) {
		double x;
		do {
			x = rand01();
		} while (x < 1e-7); // Epsilon to avoid log(0).

		double r = SIGMA * sqrt(-2.0 * log10(x));
		r = MU + r*sin(2.0*M_PI * rand01()); // Take real part only, discard complex part as it's related.
		// Clamp result to [0,1].
		noise[i] = (r <= 0) ? 0 : (r >= 1) ? 255 : (int) (0.5 + 255.0*r);
	}

	glActiveTexture(TEX(NOISE_I));
	createTex(&s_p.noise_tex, NOISE_W, NOISE_H, GL_LUMINANCE, GL_LINEAR, GL_REPEAT, noise);

	free(noise);
}

#if DBG_MODE
static void updateUniformsDebug()
{
	GLint prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
	int k = glGetUniformLocation(prog, "u_mouse");
	GLfloat mouse[3] = { MouseData[0], MouseData[1], MouseData[2] };
	//printf("!!!!! mouse: %f, %f, %f\n", mouse[0], mouse[1], mouse[2]);
	glUniform3fv(k, 1, mouse);
}
#endif

static void setUnifRGB1i(es2_unif id, GLint v)
{
	glUseProgram(s_p.rgb_prog);
	glUniform1i(s_u.u[id], v);
	glUseProgram(s_p.ntsc_prog);
	glUniform1i(s_u.u[U_COUNT + id], v);
}

static void setUnifRGB3fv(es2_unif id, GLfloat *v)
{
	glUseProgram(s_p.rgb_prog);
	glUniform3fv(s_u.u[id], 1, v);
	glUseProgram(s_p.ntsc_prog);
	glUniform3fv(s_u.u[U_COUNT + id], 1, v);
}

static void setUnifRGB1f(es2_unif id, double v)
{
	glUseProgram(s_p.rgb_prog);
	glUniform1f(s_u.u[id], v);
	glUseProgram(s_p.ntsc_prog);
	glUniform1f(s_u.u[U_COUNT + id], v);
}

static void setUnifRGB2f(es2_unif id, double a, double b)
{
	glUseProgram(s_p.rgb_prog);
	glUniform2f(s_u.u[id], a, b);
	glUseProgram(s_p.ntsc_prog);
	glUniform2f(s_u.u[U_COUNT + id], a, b);
}

static void updateUniformsRGB()
{
	DBG(updateUniformsDebug())
	setUnifRGB2f(U_NOISE_RND, rand01(), rand01());
}

static void updateUniformsSharpen()
{
	DBG(updateUniformsDebug())
}

static void updateUniformsStretch()
{
	DBG(updateUniformsDebug())
}

static void updateUniformsScreen(int video_changed)
{
	DBG(updateUniformsDebug())

	if (video_changed) {
		glUniform2f(s_u._screen_uv_scale_loc, 1.0 + 25.0/INPUT_W,
			(em_scanlines/240.0) * (1.0 + 15.0/INPUT_H));
		glUniform2f(s_u._screen_border_uv_offs_loc, 0.5 * (1.0 - 9.5/INPUT_W),
			0.5 * (1.0 - (7.0 + INPUT_H - em_scanlines) / INPUT_H));
	}
}

static void updateUniformsDownsample(int w, int h, int texIdx, int isHorzPass)
{
	DBG(updateUniformsDebug())
	GLfloat offsets[2*8];
	if (isHorzPass) {
		for (int i = 0; i < 8; ++i) {
			offsets[2*i  ] = s_downsample_offs[i] / w;
			offsets[2*i+1] = 0;
		}
	} else {
		for (int i = 0; i < 8; ++i) {
			offsets[2*i  ] = 0;
			offsets[2*i+1] = s_downsample_offs[i] / h;
		}
	}
	glUniform2fv(s_u._downsample_offsets_loc, 8, offsets);
	glUniform1i(s_u._downsample_downsampleTex_loc, texIdx);
}

static void updateUniformsTV()
{
	DBG(updateUniformsDebug())
}

static void updateUniformsCombine()
{
	DBG(updateUniformsDebug())
}

static void updateUniformsDirect(int video_changed)
{
	DBG(updateUniformsDebug())
	if (video_changed) {
		glUniform1f(s_u._direct_v_scale_loc, em_scanlines / 240.0f);
	}
}

static const char* s_unif_names[] =
{
#define U(prog_, id_, name_) "u_" # name_,
#include "es2unif.inc.hpp"
#undef U
};


static void initUniformsRGB()
{
	for (int i = 0; i < U_COUNT; i++) {
		s_u.u[i] = glGetUniformLocation(s_p.rgb_prog, s_unif_names[i]);
		s_u.u[U_COUNT + i] = glGetUniformLocation(s_p.ntsc_prog, s_unif_names[i]);
	}

	setUnifRGB1i(U_IDX_TEX, IDX_I);
	setUnifRGB1i(U_DEEMP_TEX, DEEMP_I);
	setUnifRGB1i(U_LOOKUP_TEX, LOOKUP_I);
	setUnifRGB1i(U_NOISE_TEX, NOISE_I);

	setUnifRGB3fv(U_MINS, s_p.yiq_mins);
	setUnifRGB3fv(U_MAXS, s_p.yiq_maxs);

	updateUniformsRGB();
}

static void initUniformsSharpen()
{
	GLint k;
	GLuint prog = s_p.sharpen_prog;

	k = glGetUniformLocation(prog, "u_rgbTex");
	glUniform1i(k, RGB_I);

	s_u._sharpen_convergence_loc = glGetUniformLocation(prog, "u_convergence");
	s_u._sharpen_kernel_loc = glGetUniformLocation(prog, "u_sharpenKernel");
	updateUniformsSharpen();
}

static void initUniformsStretch()
{
	GLint k;
	GLuint prog = s_p.stretch_prog;

	k = glGetUniformLocation(prog, "u_sharpenTex");
	glUniform1i(k, SHARPEN_I);

	s_u._stretch_scanlines_loc = glGetUniformLocation(prog, "u_scanlines");
	s_u._stretch_smoothenOffs_loc = glGetUniformLocation(prog, "u_smoothenOffs");
	updateUniformsStretch();
}

// Generated with following python oneliners:
// from math import *
// def rot(a,b): return [sin(a)*sin(b), -sin(a)*cos(b), -cos(a)]
// def rad(d): return pi*d/180
// rot(rad(90-65),rad(15))
//static GLfloat s_lightDir[] = { -0.109381654946615, 0.40821789367673483, 0.9063077870366499 }; // 90-65, 15
//static GLfloat s_lightDir[] = { -0.1830127018922193, 0.6830127018922193, 0.7071067811865476 }; // 90-45, 15
//static GLfloat s_lightDir[] = { -0.22414386804201336, 0.8365163037378078, 0.5000000000000001 }; // 90-30, 15
static const GLfloat s_lightDir[] = { 0.0, 0.866025, 0.5 }; // 90-30, 0
static const GLfloat s_viewPos[] = { 0, 0, 2.5 };
static const GLfloat s_xAxis[] = { 1, 0, 0 };
static const GLfloat s_shadowPoint[] = { 0, 0.7737, 0.048 };

static void initShading(GLuint prog, float intensity, float diff, float fill, float spec, float m, float fr0, float frexp)
{
	int k = glGetUniformLocation(prog, "u_lightDir");
	glUniform3fv(k, 1, s_lightDir);
	k = glGetUniformLocation(prog, "u_viewPos");
	glUniform3fv(k, 1, s_viewPos);
	k = glGetUniformLocation(prog, "u_material");
	glUniform4f(k, intensity*diff / M_PI, intensity*spec * (m+8.0) / (8.0*M_PI), m, intensity*fill / M_PI);
	k = glGetUniformLocation(prog, "u_fresnel");
	glUniform3f(k, fr0, 1-fr0, frexp);

	GLfloat shadowPlane[4];
	vec3Cross(shadowPlane, s_xAxis, s_lightDir);
	vec3Normalize(shadowPlane, shadowPlane);
	shadowPlane[3] = vec3Dot(shadowPlane, s_shadowPoint);
	k = glGetUniformLocation(prog, "u_shadowPlane");
//	printf("!!!! shadowPlane: %f,%f,%f,%f\n", shadowPlane[0], shadowPlane[1], shadowPlane[2], shadowPlane[3]);
	glUniform4fv(k, 1, shadowPlane);
}

static void initUniformsScreen()
{
	GLint k;
	GLuint prog = s_p.screen_prog;

	k = glGetUniformLocation(prog, "u_stretchTex");
	glUniform1i(k, STRETCH_I);
	k = glGetUniformLocation(prog, "u_noiseTex");
	glUniform1i(k, NOISE_I);
	k = glGetUniformLocation(prog, "u_mvp");
	glUniformMatrix4fv(k, 1, GL_FALSE, s_p.mvp_mat);

	initShading(prog, 4.0, 0.001, 0.0, 0.065, 41, 0.04, 4);

	s_u._screen_uv_scale_loc = glGetUniformLocation(prog, "u_uvScale");
	s_u._screen_border_uv_offs_loc = glGetUniformLocation(prog, "u_borderUVOffs");
	updateUniformsScreen(1);
}

static void initUniformsTV()
{
	GLint k;
	GLuint prog = s_p.tv_prog;

	k = glGetUniformLocation(prog, "u_downsample1Tex");
	glUniform1i(k, DOWNSAMPLE1_I);
	k = glGetUniformLocation(prog, "u_downsample3Tex");
	glUniform1i(k, DOWNSAMPLE3_I);
	k = glGetUniformLocation(prog, "u_downsample5Tex");
	glUniform1i(k, DOWNSAMPLE5_I);
	k = glGetUniformLocation(prog, "u_noiseTex");
	glUniform1i(k, NOISE_I);

	k = glGetUniformLocation(prog, "u_mvp");
	glUniformMatrix4fv(k, 1, GL_FALSE, s_p.mvp_mat);

	initShading(prog, 4.0, 0.0038, 0.0035, 0.039, 49, 0.03, 4);

	updateUniformsTV();
}

static void initUniformsDownsample()
{
	GLint k;
	GLuint prog = s_p.downsample_prog;

	k = glGetUniformLocation(prog, "u_weights");
	glUniform1fv(k, 8, s_downsample_ws);

	s_u._downsample_offsets_loc = glGetUniformLocation(prog, "u_offsets");
	s_u._downsample_downsampleTex_loc = glGetUniformLocation(prog, "u_downsampleTex");
	updateUniformsDownsample(280, 240, DOWNSAMPLE0_I, 1);
}

static void initUniformsCombine()
{
	GLint k;
	GLuint prog = s_p.combine_prog;

	k = glGetUniformLocation(prog, "u_tvTex");
	glUniform1i(k, TV_I);
	k = glGetUniformLocation(prog, "u_downsample3Tex");
	glUniform1i(k, DOWNSAMPLE3_I);
	k = glGetUniformLocation(prog, "u_downsample5Tex");
	glUniform1i(k, DOWNSAMPLE5_I);
	k = glGetUniformLocation(prog, "u_noiseTex");
	glUniform1i(k, NOISE_I);

	s_u._combine_glow_loc = glGetUniformLocation(prog, "u_glow");
}

static void initUniformsDirect()
{
	GLint k;
	GLuint prog = s_p.direct_prog;

	k = glGetUniformLocation(prog, "u_tex");
	glUniform1i(k, STRETCH_I);

	s_u._direct_v_scale_loc = glGetUniformLocation(prog, "u_vScale");
	updateUniformsDirect(1);
}

static void passRGB()
{
	glBindFramebuffer(GL_FRAMEBUFFER, s_p.rgb_fb);
	glViewport(0, 0, RGB_W, IDX_H);
	updateUniformsRGB();

	if (GetController(FCEM_CRT_ENABLED)) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_COLOR);
	}

	if (GetController(FCEM_NTSC_EMU)) {
		glUseProgram(s_p.ntsc_prog);
	} else {
		glUseProgram(s_p.rgb_prog);
	}
	meshRender(&s_p.quad_mesh);

	glDisable(GL_BLEND);
}

static void passSharpen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, s_p.sharpen_fb);
	glViewport(0, 0, SCREEN_W, IDX_H);
	glUseProgram(s_p.sharpen_prog);
	updateUniformsSharpen();
	meshRender(&s_p.quad_mesh);
}

static void passStretch()
{
	glBindFramebuffer(GL_FRAMEBUFFER, s_p.stretch_fb);
	glViewport(0, 0, SCREEN_W, SCREEN_H);
	glUseProgram(s_p.stretch_prog);
	updateUniformsStretch();
	meshRender(&s_p.quad_mesh);
}

static void passDownsample()
{
	glUseProgram(s_p.downsample_prog);

	for (int i = 0; i < 6; ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, s_p.downsample_fb[i]);
		glViewport(0, 0, s_downsample_widths[i+1], s_downsample_heights[i+1]);
		updateUniformsDownsample(s_downsample_widths[i], s_downsample_heights[i], TV_I + i, !(i & 1));
		meshRender(&s_p.quad_mesh);
	}
}

static void passScreen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, s_p.tv_fb);
	glViewport(0, 0, SCREEN_W, SCREEN_H);
	glUseProgram(s_p.screen_prog);
	updateUniformsScreen(0);
	meshRender(&s_p.screen_mesh);
}

static void passTV()
{
	glBindFramebuffer(GL_FRAMEBUFFER, s_p.tv_fb);
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, SCREEN_W, SCREEN_H);

	glUseProgram(s_p.tv_prog);
	updateUniformsTV();
	meshRender(&s_p.tv_mesh);
}

static void passCombine()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(s_p.viewport[0], s_p.viewport[1], s_p.viewport[2], s_p.viewport[3]);
	glUseProgram(s_p.combine_prog);
	updateUniformsCombine();
	meshRender(&s_p.quad_mesh);
}

static void passDirect()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(s_p.viewport[0], s_p.viewport[1], s_p.viewport[2], s_p.viewport[3]);
	glUseProgram(s_p.direct_prog);
	updateUniformsDirect(0);
	meshRender(&s_p.quad_mesh);
}

void es2UpdateController(int idx, double v)
{
	switch (idx) {
	case FCEM_BRIGHTNESS:
		v = 0.15 * v;
		setUnifRGB1f(U_BRIGHTNESS, v);
		break;
	case FCEM_CONTRAST:
		v = 1.0 + 0.4*GetController(FCEM_CONTRAST);
		setUnifRGB1f(U_CONTRAST, v);
		break;
	case FCEM_COLOR:
		v = 1.0 + GetController(FCEM_COLOR);
		setUnifRGB1f(U_COLOR, v);
		break;
	case FCEM_NTSC_EMU:
		v = GetController(FCEM_NTSC_EMU);
		updateSharpenKernel();
		// Stretch pass smoothen UV offset; smoothen if NTSC emulation is enabled.
		glUseProgram(s_p.stretch_prog);
		glUniform2f(s_u._stretch_smoothenOffs_loc, 0, v * -0.25/IDX_H);
		break;
	case FCEM_GAMMA:
		v = GAMMA_NTSC/GAMMA_SRGB + 0.3*GetController(FCEM_GAMMA);
		setUnifRGB1f(U_GAMMA, v);
		break;
	case FCEM_NOISE:
		v = GetController(FCEM_CRT_ENABLED) * 0.08 * GetController(FCEM_NOISE)*GetController(FCEM_NOISE);
		setUnifRGB1f(U_NOISE_AMP, v);
		break;
	case FCEM_CONVERGENCE:
		glUseProgram(s_p.sharpen_prog);
		v = GetController(FCEM_CRT_ENABLED) * 2.0 * GetController(FCEM_CONVERGENCE);
		glUniform1f(s_u._sharpen_convergence_loc, v);
		break;
	case FCEM_SHARPNESS:
		updateSharpenKernel();
		break;
	case FCEM_SCANLINES:
		glUseProgram(s_p.stretch_prog);
		v = GetController(FCEM_CRT_ENABLED) * 0.45 * GetController(FCEM_SCANLINES);
		glUniform1f(s_u._stretch_scanlines_loc, v);
		break;
	case FCEM_GLOW:
		glUseProgram(s_p.combine_prog);
		v = 0.1 * GetController(FCEM_GLOW);
		glUniform3f(s_u._combine_glow_loc, v, v*v, v + v*v);
		break;
	case FCEM_CRT_ENABLED:
		// Enable CRT, update dependent uniforms. (Without modifying stored control values.)
		FCEM_SetController(FCEM_NOISE, GetController(FCEM_NOISE));
		FCEM_SetController(FCEM_SCANLINES, GetController(FCEM_SCANLINES));
		FCEM_SetController(FCEM_CONVERGENCE, GetController(FCEM_CONVERGENCE));
		break;
	default:
// TODO: tsone: warning message?
		break;
	}
}

// On failure, return value < 0, otherwise success.
static int es2CreateWebGLContext()
{
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	attr.alpha = attr.antialias = attr.premultipliedAlpha = 0;
	attr.depth = attr.stencil = attr.preserveDrawingBuffer = attr.preferLowPowerToHighPerformance = attr.failIfMajorPerformanceCaveat = 0;
	attr.enableExtensionsByDefault = 0;
	attr.majorVersion = 1;
	attr.minorVersion = 0;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attr);
	if (ctx > 0) {
		emscripten_webgl_make_context_current(ctx);
	}
	return ctx;
}

int es2Init(double aspect)
{
	if (es2CreateWebGLContext() <= 0) {
		return 0;
	}

	// Build perspective MVP matrix.
	GLfloat trans[3] = { 0, 0, -2.5 };
	GLfloat proj[4*4];
	GLfloat view[4*4];
	// Stretch aspect slightly as the TV mesh is wee bit vertically squished.
	aspect *= 1.04;
	mat4Persp(proj, 0.25*M_PI / aspect, aspect, 0.125, 16.0);
	mat4Trans(view, trans);
	mat4Mul(s_p.mvp_mat, proj, view);

	s_p.overscan_pixels = (GLubyte*) malloc(OVERSCAN_W*IDX_H);
	s_p.overscan_color = 0xFE; // Set bogus value to ensure overscan update.

	glGetIntegerv(GL_VIEWPORT, s_p.viewport);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glBlendColor(PERSISTENCE_R, PERSISTENCE_G, PERSISTENCE_B, 0);
	glClearColor(0, 0, 0, 0);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);
	glStencilMask(GL_FALSE);
	glEnableVertexAttribArray(0);

	createMesh(&s_p.quad_mesh, mesh_quad_vert_num, ARRAY_SIZE(mesh_quad_varrays), mesh_quad_varrays, 2*mesh_quad_face_num, 0);

	// Setup input pixels texture.
	glActiveTexture(TEX(IDX_I));
	createTex(&s_p.idx_tex, IDX_W, IDX_H, GL_LUMINANCE, GL_NEAREST, GL_CLAMP_TO_EDGE, 0);

	// Setup input de-emphasis rows texture.
	glActiveTexture(TEX(DEEMP_I));
	createTex(&s_p.deemp_tex, IDX_H, 1, GL_LUMINANCE, GL_NEAREST, GL_CLAMP_TO_EDGE, 0);

	genLookupTex();
	genNoiseTex();

	// Configure RGB framebuffer.
	glActiveTexture(TEX(RGB_I));
	createFBTex(&s_p.rgb_tex, &s_p.rgb_fb, RGB_W, IDX_H, GL_RGB, GL_NEAREST, GL_CLAMP_TO_EDGE);
	s_p.rgb_prog = buildShaderFile("/data/rgb.vert", "/data/rgb.frag", common_src);
	s_p.ntsc_prog = buildShaderFile("/data/ntsc.vert", "/data/ntsc.frag", common_src);
	initUniformsRGB();

	// Setup sharpen framebuffer.
	glActiveTexture(TEX(SHARPEN_I));
	createFBTex(&s_p.sharpen_tex, &s_p.sharpen_fb, SCREEN_W, IDX_H, GL_RGB, GL_NEAREST, GL_CLAMP_TO_EDGE);
	s_p.sharpen_prog = buildShaderFile("/data/sharpen.vert", "/data/sharpen.frag", common_src);
	initUniformsSharpen();

	// Setup stretch framebuffer.
	glActiveTexture(TEX(STRETCH_I));
	createFBTex(&s_p.stretch_tex, &s_p.stretch_fb, SCREEN_W, SCREEN_H, GL_RGB, GL_LINEAR, GL_CLAMP_TO_EDGE);
	s_p.stretch_prog = buildShaderFile("/data/stretch.vert", "/data/stretch.frag", common_src);
	initUniformsStretch();

	// Setup screen/TV framebuffer.
	glActiveTexture(TEX(TV_I));
	createFBTex(&s_p.tv_tex, &s_p.tv_fb, SCREEN_W, SCREEN_H, GL_RGB, GL_LINEAR, GL_CLAMP_TO_EDGE);

	// Setup downsample framebuffers.
	for (int i = 0; i < 6; ++i) {
		glActiveTexture(TEX(DOWNSAMPLE0_I + i));
		createFBTex(&s_p.downsample_tex[i], &s_p.downsample_fb[i],
			s_downsample_widths[i+1], s_downsample_heights[i+1],
			GL_RGB, GL_LINEAR, GL_CLAMP_TO_EDGE);
	}

	// Setup downsample shader.
	s_p.downsample_prog = buildShaderFile("/data/downsample.vert", "/data/downsample.frag", common_src);
	initUniformsDownsample();

	// Setup screen shader.
	s_p.screen_prog = buildShaderFile("/data/screen.vert", "/data/screen.frag", common_src);
	createMesh(&s_p.screen_mesh, mesh_screen_vert_num, ARRAY_SIZE(mesh_screen_varrays), mesh_screen_varrays, 3*mesh_screen_face_num, mesh_screen_faces);
	initUniformsScreen();

	// Setup TV shader.
	s_p.tv_prog = buildShaderFile("/data/tv.vert", "/data/tv.frag", common_src);
// TODO: tsone: generate distances to crt screen edges
	int num_edges = 0;
	int *edges = createUniqueEdges(&num_edges, mesh_screen_vert_num, 3*mesh_screen_face_num, mesh_screen_faces);
	num_edges *= 2;

	GLfloat *rim_extra = (GLfloat*) malloc(3*sizeof(GLfloat) * mesh_rim_vert_num);
	for (int i = 0; i < mesh_rim_vert_num; ++i) {
		GLfloat p[3];
		vec3Set(p, &mesh_rim_verts[3*i]);
		GLfloat shortest[3] = { 0, 0, 0 };
		double shortestDist = 1000000;
		for (int j = 0; j < num_edges; j += 2) {
			int ai = 3*edges[j];
			int bi = 3*edges[j+1];
			GLfloat diff[3];
			vec3ClosestOnSegment(diff, p, &mesh_screen_verts[ai], &mesh_screen_verts[bi]);
			vec3Sub(diff, diff, p);
			double dist = vec3Length2(diff);
			if (dist < shortestDist) {
				shortestDist = dist;
				vec3Set(shortest, diff);
			}
		}
// TODO: tsone: could interpolate uv with vert normal here, and not in vertex shader
		rim_extra[3*i] = shortest[0];
		rim_extra[3*i+1] = shortest[1];
		rim_extra[3*i+2] = shortest[2];
	}
	mesh_rim_varrays[2].data = rim_extra;
	createMesh(&s_p.tv_mesh, mesh_rim_vert_num, ARRAY_SIZE(mesh_rim_varrays), mesh_rim_varrays, 3*mesh_rim_face_num, mesh_rim_faces);
	free(edges);
	free(rim_extra);
	initUniformsTV();

	// Setup combine shader.
	s_p.combine_prog = buildShaderFile("/data/combine.vert", "/data/combine.frag", common_src);
	initUniformsCombine();

	// Setup direct shader.
	s_p.direct_prog = buildShaderFile("/data/direct.vert", "/data/direct.frag", common_src);
	initUniformsDirect();

	return 1;
}

void es2Deinit()
{
	deleteFBTex(&s_p.rgb_tex, &s_p.rgb_fb);
	deleteFBTex(&s_p.sharpen_tex, &s_p.sharpen_fb);
	deleteFBTex(&s_p.stretch_tex, &s_p.stretch_fb);
	deleteFBTex(&s_p.tv_tex, &s_p.tv_fb);
	for (int i = 0; i < 6; ++i) {
		deleteFBTex(&s_p.downsample_tex[i], &s_p.downsample_fb[i]);
	}
	deleteTex(&s_p.idx_tex);
	deleteTex(&s_p.deemp_tex);
	deleteTex(&s_p.lookup_tex);
	deleteTex(&s_p.noise_tex);
	deleteShader(&s_p.rgb_prog);
	deleteShader(&s_p.ntsc_prog);
	deleteShader(&s_p.sharpen_prog);
	deleteShader(&s_p.stretch_prog);
	deleteShader(&s_p.screen_prog);
	deleteShader(&s_p.downsample_prog);
	deleteShader(&s_p.tv_prog);
	deleteShader(&s_p.combine_prog);
	deleteMesh(&s_p.screen_mesh);
	deleteMesh(&s_p.quad_mesh);
	deleteMesh(&s_p.tv_mesh);
	free(s_p.overscan_pixels);
}

void es2SetViewport(int width, int height)
{
	s_p.viewport[2] = width;
	s_p.viewport[3] = height;
}

void es2VideoChanged()
{
	glUseProgram(s_p.screen_prog);
	updateUniformsScreen(1);

	glUseProgram(s_p.direct_prog);
	updateUniformsDirect(1);
}

void es2Render(GLubyte *pixels, GLubyte *row_deemp, GLubyte overscan_color)
{
	// Update input pixels.
	glActiveTexture(TEX(IDX_I));
	glBindTexture(GL_TEXTURE_2D, s_p.idx_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, OVERSCAN_W, 0, IDX_W-2*OVERSCAN_W, IDX_H, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);
	if (s_p.overscan_color != overscan_color) {
		s_p.overscan_color = overscan_color;
//		printf("overscan: %02X\n", overscan_color);

		memset(s_p.overscan_pixels, overscan_color, OVERSCAN_W * IDX_H);

		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, OVERSCAN_W, IDX_H, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_p.overscan_pixels);
		glTexSubImage2D(GL_TEXTURE_2D, 0, IDX_W-OVERSCAN_W, 0, OVERSCAN_W, IDX_H, GL_LUMINANCE, GL_UNSIGNED_BYTE, s_p.overscan_pixels);
	}

	// Update input de-emphasis rows.
	glActiveTexture(TEX(DEEMP_I));
	glBindTexture(GL_TEXTURE_2D, s_p.deemp_tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, IDX_H, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, row_deemp);

	passRGB();
	passSharpen();
	passStretch();
	if (GetController(FCEM_CRT_ENABLED)) {
		passScreen();
		passDownsample();
		passTV();
		passCombine();
	} else {
		passDirect();
	}
}

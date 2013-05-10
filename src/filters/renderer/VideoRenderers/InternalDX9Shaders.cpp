/*
 * (C) 2012-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"

namespace DSObjects
{
// note: when adding or removing strings here, also update the string pointer and length tables at the bottom
// layout adapted for readability of the shaders themselves
// keep pre-processor commands (# lines) in internally seperate lines with \n at the end of the previous line, and \n at the end of the current line
// neither the formatter nor the compiler have Unicode support

// two-letter macros are reserved: these are formatted as majuscule M and a minuscule latin letter
// taken care of after device creation
// 0, Ml = PS 3.0 level compile: 0 or 1
// 1, Mr = surface color intervals: 0 for [0, 1], 1 for [16384/65535, 49151/65535]
// 2, Mq = maximum quantized integer value of the display color format, 255 is for 8-bit, 1023 is for 10-bit, function: pow(2, [display bits per component])-1
// partially initialized in initializer, taken care of after device creation and in the legacy swapchain resize routine
// 3, Mw = pre-resize width
// 4, Mh = pre-resize height
// 5, Ma = post-resize width
// 6, Mv = post-resize height
// partially initialized in initializer, taken care of in the resizer routine (the resizer pixel shaders should be the first to initialize in the renderer)
// 7, Mb = resize output width (not limited by the post-resize width)
// 8, Mu = resize output height (not limited by the post-resize height)
// 9, Me = resizer pass context-specific width (as the parameter can be either pre- or post-resize width for two-pass resizers)
// initialized in initializer, taken care of in the final pass routine
// 10, Mm = XYZ to RGB matrix
// 11, Mc = color management: disabled == 0, Little CMS == 1, Limited Color Ranges == 2
// 12, Ms = LUT3D samples in each U, V and W dimension
// 13, Md = dithering levels: no dithering == 0, static ordered dithering == 1, random ordered dithering == 2, adaptive random dithering >= 3
// 14, Mt = dithering test: 0 or 1
// initialized in initializer, set by the mixer, taken care of in the final pass routine
// 15, My = Y'CbCr chroma cositing: 0 for horizontal chroma cositing, 1 for no horizontal chroma cositing
// initialized in initializer, taken care of in the frame interpolation routine
// 16, Mf = area factor for adaptive frame interpolation

extern char const gk_szHorizontalBlurShader[] =
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"#define sp(a) tex2D(s0, tex+float2((a)/float(Mw), 0))\n"
"sampler s0 : register(s0);"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float rf = max(1/31., Mb/float(Mw));"// resizing factor
	"float ws = (min(31, Mw/float(Mb))-1.)*.5;"// samples on each side, factor 31 is the maximum supported for ps_2_0
	"float fo = frac(ws);"// outer pixels weight
	"float ns = ws-fo;"// full samples on each side
	"float4 ac = tex2D(s0, tex);"// central pixel

	"int i = ns;"// loop counter, eliminated by the compiler
	"[unroll] while (i) {"// add full samples
		"ac += sp(-i)+sp(i);"
		"--i;"
	"}"
	"ac *= rf;"

	"if (fo) {"// add partial samples
		"ac += (sp(-ns-1.)+sp(ns+1.))*fo*rf;"
	"}"
	"return ac;"
"}";

extern char const gk_szVerticalBlurShader[] =
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"#define sp(a) tex2D(s0, tex+float2(0, (a)/float(Mh)))\n"
"sampler s0 : register(s0);"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float rf = max(1/31., Mu/float(Mh));"// resizing factor
	"float ws = (min(31, Mh/float(Mu))-1.)*.5;"// samples on each side, factor 31 is the maximum supported for ps_2_0
	"float fo = frac(ws);"// outer pixels weight
	"float ns = ws-fo;"// full samples on each side
	"float4 ac = tex2D(s0, tex);"// central pixel

	"int i = ns;"// loop counter, eliminated by the compiler
	"[unroll] while (i) {"// add full samples
		"ac += sp(-i)+sp(i);"
		"--i;"
	"}"
	"ac *= rf;"

	"if (fo) {"// add partial samples
		"ac += (sp(-ns-1.)+sp(ns+1.))*fo*rf;"
	"}"
	"return ac;"
"}";

/* The list for resizers is offset by two; shaders 0 and 1 are never used
static char const gk_szResizerShader0[] =
// Nearest neighbor
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"return tex2D(s0, (tex+.5)*dxdy);"// output nearest neighbor
"}";

static char const gk_szResizerShader1[] =
// Bilinear
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float2 t = frac(tex);"
	"float2 pos = tex-t;"
	"return lerp(lerp(tex2D(s0, (pos+.5)*dxdy), tex2D(s0, (pos+float2(1.5, .5))*dxdy), dd.x), lerp(tex2D(s0, (pos+float2(.5, 1.5))*dxdy), tex2D(s0, (pos+1.5)*dxdy), t.x), t.y);"// interpolate and output
"}";
*/

static char const gk_szResizerShader2[] =
// Perlin Smootherstep
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float2 t = frac(tex);"
	"float2 pos = tex-t;"

	// weights
	"float2 w1 = ((6*t-15.)*t+10.)*pow(t, 3), w0 = 1.-w1;"
	"float4 M0 = tex2D(s0, (pos+.5)*dxdy), M1 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy), L0 = tex2D(s0, (pos+float2(1.5, .5))*dxdy), L1 = tex2D(s0, (pos+1.5)*dxdy);"// original pixels

	// vertical interpolation
	"float4 Q0 = M0*w0.y+M1*w1.y, Q1 = L0*w0.y+L1*w1.y;"
	"return Q0*w0.x+Q1*w1.x;"// horizontal interpolation and output
"}";

static char const gk_szResizerShader3[] =
// Bicubic A=-0.6
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"static const float4x4 tco = {0, -.6, 1.2, -.6, 1, 0, -2.4, 1.4, 0, .6, 1.8, -1.4, 0, 0, -.6, .6};"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}";

static char const gk_szResizerShader4[] =
// Bicubic A=-0.8
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"static const float4x4 tco = {0, -.8, 1.6, -.8, 1, 0, -2.2, 1.2, 0, .8, 1.4, -1.2, 0, 0, -.8, .8};"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}";

static char const gk_szResizerShader5[] =
// Bicubic A=-1.0
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"static const float4x4 tco = {0, -1, 2, -1, 1, 0, -2, 1, 0, 1, 1, -1, 0, 0, -1, 1};"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	"return mul(mul(tco, float4(1, t, t*t, t*t*t)), float4x4(Q0, Q1, Q2, Q3));"
"}";

static char const gk_szResizerShader6[] =
// B-spline4
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(1, 4, 1, 0)/6.+float4(-.5, 0, .5, 0)*t+float4(.5, -1, .5, 0)*t2+float4(-1, 3, -3, 1)/6.*t3;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(1, 4, 1, 0)/6.+float4(-.5, 0, .5, 0)*t+float4(.5, -1, .5, 0)*t2+float4(-1, 3, -3, 1)/6.*t3;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}";

static char const gk_szResizerShader7[] =
// Mitchell-Netravali spline4
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(1, 16, 1, 0)/18.+float4(-.5, 0, .5, 0)*t+float4(5, -12, 9, -2)/6.*t2+float4(-7, 21, -21, 7)/18.*t3;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(1, 16, 1, 0)/18.+float4(-.5, 0, .5, 0)*t+float4(5, -12, 9, -2)/6.*t2+float4(-7, 21, -21, 7)/18.*t3;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}";

static char const gk_szResizerShader8[] =
// Catmull-Rom spline4
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(-.5, 0, .5, 0)*t+float4(1, -2.5, 2, -.5)*t2+float4(-.5, 1.5, -1.5, .5)*t3;"
	"w0123.y += 1.;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"

	// calculate weights
	"float t2 = t*t, t3 = t*t2;"
	"float4 w0123 = float4(-.5, 0, .5, 0)*t+float4(1, -2.5, 2, -.5)*t2+float4(-.5, 1.5, -1.5, .5)*t3;"
	"w0123.y += 1.;"
	"return w0123.x*Q0+w0123.y*Q1+w0123.z*Q2+w0123.w*Q3;"// interpolation output
"}";

static char const gk_szResizerShader9[] =
// B-spline6
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-1.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(3.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float t5 = t34.x*t2;"
	"float4 w0134 = float4(1, 26, 26, 1)/120.+float4(-1, -10, 10, 1)/24.*t+float4(1, 2, 2, 1)/12.*t2+float4(-1, 2, -2, 1)/12.*t34.x+float4(1, -4, -4, 1)/24.*t34.y+float4(-1, 5, 10, -5)/120.*t5;"
	"float w2 = .55-.5*t2+.25*t34.y;"
	"float2 we5 = float2(1, -10)/120.*t5;"
	"w2 += we5.y;"
	"return w0134.x*Q0+w0134.y*Q1+w2*Q2+w0134.z*Q3+w0134.w*Q4+we5.x*Q5;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -1.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(.5, 3.5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float t5 = t34.x*t2;"
	"float4 w0134 = float4(1, 26, 26, 1)/120.+float4(-1, -10, 10, 1)/24.*t+float4(1, 2, 2, 1)/12.*t2+float4(-1, 2, -2, 1)/12.*t34.x+float4(1, -4, -4, 1)/24.*t34.y+float4(-1, 5, 10, -5)/120.*t5;"
	"float w2 = .55-.5*t2+.25*t34.y;"
	"float2 we5 = float2(1, -10)/120.*t5;"
	"w2 += we5.y;"
	"return w0134.x*Q0+w0134.y*Q1+w2*Q2+w0134.z*Q3+w0134.w*Q4+we5.x*Q5;"// interpolation output
"}";

static char const gk_szResizerShader10[] =
// Catmull-Rom spline6
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-1.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(3.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float t5 = t34.x*t2;"
	"float3 w024 = float3(.125, -.25, .125)*t2+float3(-.375, -7.25, -2.375)*t34.x+float3(.375, 10.75, 3.875)*t34.y+float3(-.125, -4.25, -1.625)*t5;"
	"float3 w135 = float3(-.5, .5, 0)*t+float3(3.125, 6.75, .125)*t34.x+float3(-4.25, -10.5, -.25)*t34.y+float3(1.625, 4.25, .125)*t5;"
	"w024.y += 1.;"
	"return w024.x*Q0+w135.x*Q1+w024.y*Q2+w135.y*Q3+w024.z*Q4+w135.z*Q5;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -1.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(.5, 3.5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float t5 = t34.x*t2;"
	"float3 w024 = float3(.125, -.25, .125)*t2+float3(-.375, -7.25, -2.375)*t34.x+float3(.375, 10.75, 3.875)*t34.y+float3(-.125, -4.25, -1.625)*t5;"
	"float3 w135 = float3(-.5, .5, 0)*t+float3(3.125, 6.75, .125)*t34.x+float3(-4.25, -10.5, -.25)*t34.y+float3(1.625, 4.25, .125)*t5;"
	"w024.y += 1.;"
	"return w024.x*Q0+w135.x*Q1+w024.y*Q2+w135.y*Q3+w024.z*Q4+w135.z*Q5;"// interpolation output
"}";

static char const gk_szResizerShader11[] =
// Catmull-Rom spline8
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(-2.5, .5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(-1.5, .5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
	"float4 Q6 = tex2D(s0, (pos+float2(3.5, .5))*dxdy);"
	"float4 Q7 = tex2D(s0, (pos+float2(4.5, .5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float3 t567 = float3(t2, t34)*t34.x;"
	"float4 w0246 = float4(0, -.5, .5, 0)*t+float4(-1/48., .0625, -.0625, 1/48.)*t34.x+float4(1/12., 10.375, 24, 13/24.)*t34.y+float4(-.125, -23.875, -58.375, -1.625)*t567.x+float4(1/12., 19.375, 49, 37/24.)*t567.y+float4(-1/48., -5.4375, -14.0625, -23/48.)*t567.z;"
	"float4 w1357 = float4(.125, -.25, .125, 0)*t2+float4(-59/48., -25.0625, -8.6875, -1/48.)*t34.y+float4(2.4375, 59.6875, 21.8125, .0625)*t567.x+float4(-1.8125, -49.4375, -18.6875, -.0625)*t567.y+float4(23/48., 14.0625, 5.4375, 1/48.)*t567.z;"
	"w1357.y += 1.;"
	"return w0246.x*Q0+w1357.x*Q1+w0246.y*Q2+w1357.y*Q3+w0246.z*Q4+w1357.z*Q5+w0246.w*Q6+w1357.w*Q7;"// interpolation output
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"
	// original pixels
	"float4 Q0 = tex2D(s0, (pos+float2(.5, -2.5))*dxdy);"
	"float4 Q1 = tex2D(s0, (pos+float2(.5, -1.5))*dxdy);"
	"float4 Q2 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
	"float4 Q3 = tex2D(s0, (pos+.5)*dxdy);"
	"float4 Q4 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
	"float4 Q5 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
	"float4 Q6 = tex2D(s0, (pos+float2(.5, 3.5))*dxdy);"
	"float4 Q7 = tex2D(s0, (pos+float2(.5, 4.5))*dxdy);"

	// calculate weights
	"float t2 = t*t; float2 t34 = float2(t, t2)*t2; float3 t567 = float3(t2, t34)*t34.x;"
	"float4 w0246 = float4(0, -.5, .5, 0)*t+float4(-1/48., .0625, -.0625, 1/48.)*t34.x+float4(1/12., 10.375, 24, 13/24.)*t34.y+float4(-.125, -23.875, -58.375, -1.625)*t567.x+float4(1/12., 19.375, 49, 37/24.)*t567.y+float4(-1/48., -5.4375, -14.0625, -23/48.)*t567.z;"
	"float4 w1357 = float4(.125, -.25, .125, 0)*t2+float4(-59/48., -25.0625, -8.6875, -1/48.)*t34.y+float4(2.4375, 59.6875, 21.8125, .0625)*t567.x+float4(-1.8125, -49.4375, -18.6875, -.0625)*t567.y+float4(23/48., 14.0625, 5.4375, 1/48.)*t567.z;"
	"w1357.y += 1.;"
	"return w0246.x*Q0+w1357.x*Q1+w0246.y*Q2+w1357.y*Q3+w0246.z*Q4+w1357.z*Q5+w0246.w*Q6+w1357.w*Q7;"// interpolation output
"}";

static char const gk_szResizerShader12[] =
// compensated Lanczos2
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"#define PI acos(-1)\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"

	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the left
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
		"float4 Q2 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
		"float4 Q3 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
		"float4 wset = float3(0, 1, 2).yxyz+float2(t, -t).xxyy;"
		"float4 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"

		"float wc = 1.-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w.y += wc*(1.-t);"
		"w.z += wc*t;"
		"return w.x*Q0+w.y*Q1+w.z*Q2+w.w*Q3;}"// interpolation output

	"return Q1;"// case t == 0 is required to return sample Q1, because of a possible division by 0
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"

	"float4 Q1 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the top
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
		"float4 Q2 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
		"float4 Q3 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
		"float4 wset = float3(0, 1, 2).yxyz+float2(t, -t).xxyy;"
		"float4 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"

		"float wc = 1.-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w.y += wc*(1.-t);"
		"w.z += wc*t;"
		"return w.x*Q0+w.y*Q1+w.z*Q2+w.w*Q3;}"// interpolation output

	"return Q1;"// case t == 0 is required to return sample Q1, because of a possible division by 0
"}";

static char const gk_szResizerShader13[] =
// compensated Lanczos3
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"#define PI acos(-1)\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"

	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the left
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(-1.5, .5))*dxdy);"
		"float4 Q1 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
		"float4 Q3 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
		"float4 Q4 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
		"float4 Q5 = tex2D(s0, (pos+float2(3.5, .5))*dxdy);"
		"float3 wset0 = float3(2, 1, 0)+t;"
		"float3 wset1 = float3(1, 2, 3)-t;"
		"float3 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
		"float3 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"

		"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w0.z += wc*(1.-t);"
		"w1.x += wc*t;"
		"return w0.x*Q0+w0.y*Q1+w0.z*Q2+w1.x*Q3+w1.y*Q4+w1.z*Q5;}"// interpolation output

	"return Q2;"// case t == 0 is required to return sample Q2, because of a possible division by 0
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"

	"float4 Q2 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the top
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(.5, -1.5))*dxdy);"
		"float4 Q1 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
		"float4 Q3 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
		"float4 Q4 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
		"float4 Q5 = tex2D(s0, (pos+float2(.5, 3.5))*dxdy);"
		"float3 wset0 = float3(2, 1, 0)+t;"
		"float3 wset1 = float3(1, 2, 3)-t;"
		"float3 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
		"float3 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"

		"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w0.z += wc*(1.-t);"
		"w1.x += wc*t;"
		"return w0.x*Q0+w0.y*Q1+w0.z*Q2+w1.x*Q3+w1.y*Q4+w1.z*Q5;}"// interpolation output

	"return Q2;"// case t == 0 is required to return sample Q2, because of a possible division by 0
"}";

static char const gk_szResizerShader14[] =
// compensated Lanczos4
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"#define PI acos(-1)\n"
"sampler s0 : register(s0);"

"float4 mainH(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Mw, Mh);"
	"float t = frac(tex.x);"
	"float2 pos = tex-float2(t, 0);"

	"float4 Q3 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the left
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(-2.5, .5))*dxdy);"
		"float4 Q1 = tex2D(s0, (pos+float2(-1.5, .5))*dxdy);"
		"float4 Q2 = tex2D(s0, (pos+float2(-.5, .5))*dxdy);"
		"float4 Q4 = tex2D(s0, (pos+float2(1.5, .5))*dxdy);"
		"float4 Q5 = tex2D(s0, (pos+float2(2.5, .5))*dxdy);"
		"float4 Q6 = tex2D(s0, (pos+float2(3.5, .5))*dxdy);"
		"float4 Q7 = tex2D(s0, (pos+float2(4.5, .5))*dxdy);"
		"float4 wset0 = float4(3, 2, 1, 0)+t;"
		"float4 wset1 = float4(1, 2, 3, 4)-t;"
		"float4 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
		"float4 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"

		"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w0.w += wc*(1.-t);"
		"w1.x += wc*t;"
		"return w0.x*Q0+w0.y*Q1+w0.z*Q2+w0.w*Q3+w1.x*Q4+w1.y*Q5+w1.z*Q6+w1.w*Q7;}"// interpolation output

	"return Q3;"// case t == 0 is required to return sample Q3, because of a possible division by 0
"}"

"float4 mainV(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float2 dxdy = 1/float2(Me, Mh);"
	"float t = frac(tex.y);"
	"float2 pos = tex-float2(0, t);"

	"float4 Q3 = tex2D(s0, (pos+.5)*dxdy);"// nearest original pixel to the top
	"if(t) {"
		// original pixels
		"float4 Q0 = tex2D(s0, (pos+float2(.5, -2.5))*dxdy);"
		"float4 Q1 = tex2D(s0, (pos+float2(.5, -1.5))*dxdy);"
		"float4 Q2 = tex2D(s0, (pos+float2(.5, -.5))*dxdy);"
		"float4 Q4 = tex2D(s0, (pos+float2(.5, 1.5))*dxdy);"
		"float4 Q5 = tex2D(s0, (pos+float2(.5, 2.5))*dxdy);"
		"float4 Q6 = tex2D(s0, (pos+float2(.5, 3.5))*dxdy);"
		"float4 Q7 = tex2D(s0, (pos+float2(.5, 4.5))*dxdy);"
		"float4 wset0 = float4(3, 2, 1, 0)+t;"
		"float4 wset1 = float4(1, 2, 3, 4)-t;"
		"float4 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
		"float4 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"

		"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w0.w += wc*(1.-t);"
		"w1.x += wc*t;"
		"return w0.x*Q0+w0.y*Q1+w0.z*Q2+w0.w*Q3+w1.x*Q4+w1.y*Q5+w1.z*Q6+w1.w*Q7;}"// interpolation output

	"return Q3;"// case t == 0 is required to return sample Q3, because of a possible division by 0
"}";

extern char const gk_szFinalpassShader[] =
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#define tex3D(s, t) tex3Dlod(s, float4(t, 0))\n"
"#endif\n"
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"
"#define sa(a, b, c) float3 a = tex2D(s0, tex+float2(b/(Width+1.), c/(Height+1.))).rgb;\n"

"sampler s0 : register(s0);\n"

"#if Md >= 3\n"
"float4 c0 : register(c0);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c0 : register(c0);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float2 c1 : register(c1);"// r0b, r0b, ign, ign,
"float4 c2 : register(c2);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c3 : register(c3);"// p0g, p0g, p0g, p0g,
"float4 c4 : register(c4);"// p0b, p0b, p0b, p0b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"sa(s1, 0, 0)\n"// original pixel
"#if Mr\n"
	"s1 = s1*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
	"if(Mv == 288 || Mv == 576) s1 = mul(s1, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
	"else s1 = mul(s1, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
	"s1 = mul(s1, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
	"s1 = sign(s1)*pow(abs(s1), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
	"s1 = sign(s1)*pow(abs(s1), 1/3.);\n"// apply linear gamma correction, negative input compatible
	"s1 = tex3D(LUT3D, (s1*LUT3Dsize+.5)/(LUT3Dsize+1.)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
	"s1 = s1.rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+s1.ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+s1.bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
	"s1 = s1.rrr+float3(0, -.202008/.587, 1.772)*s1.ggg+float3(1.402, -.419198/.587, 0)*s1.bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
	"s1 = s1.rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+s1.ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+s1.bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
	"s1 = s1.rrr+float3(0, -.1674679/.894, 1.8556)*s1.ggg+float3(1.5748, -.4185031/.894, 0)*s1.bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2D(DitherMatrix, tex*float2(Width, Height)/128.).rrr;"// sample dither matrix
	"s1 += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"s1 = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue = float3(tex2D(DitherMatrix, dtt*c2.xy+dtt.yx*c2.zw+c0.xy).x, tex2D(DitherMatrix, dtt*c3.xy+dtt.yx*c3.zw+c0.zw).x, tex2D(DitherMatrix, dtt*c4.xy+dtt.yx*c4.zw+c1).x);"// sample randomized dither matrix
	"s1 += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"s1 = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
	// adaptive random
	"sa(s2, -1, -1) sa(s3, 0, -1) sa(s4, 1, -1) sa(s5, -1, 0) sa(s6, 1, 0) sa(s7, -1, 1) sa(s8, 0, 1) sa(s9, 1, 1)"// sample surrounding pixels
	"float3 dv = smoothstep(.125, 0, abs(s2+s3+s4-s7-s8-s9)+abs(s2+s5+s7-s4-s6-s9)+abs(s2+s3+s5-s6-s8-s9)+abs(s3+s4+s6-s5-s7-s8))*Md/float(Mq);"// color contour detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c0;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"s1 += (rn.rgb-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
	"s1 = (rn.rgb-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
	"return s1.rgbb;"// processed output
"}";

static char const gk_szBasicFrameInterpolationShader0[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// intra-frame time
"float t : register(c0);\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float2 c2 : register(c2);"// r0b, r0b, ign, ign,
"float4 c3 : register(c3);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c4 : register(c4);"// p0g, p0g, p0g, p0g,
"float4 c5 : register(c5);"// p0b, p0b, p0b, p0b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"float3 Q0 = tex2Dlod(s0, float4(tex, 0, 0)).rgb, Q1 = tex2Dlod(s5, float4(tex, 0, 0)).rgb, Q2 = tex2Dlod(s4, float4(tex, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex, 0, 0)).rgb;"
	"float3 PO = ((((Q1-Q2)*3+Q3-Q0)*t+Q0*2+Q2*4-Q1*5-Q3)*t+Q2-Q0)*t*.5+Q1;\n"// interpolation output

"#if Mr\n"
	"PO = PO*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
	"if(Mv == 288 || Mv == 576) PO = mul(PO, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
	"else PO = mul(PO, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
	"PO = mul(PO, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
	"PO = sign(PO)*pow(abs(PO), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
	"PO = sign(PO)*pow(abs(PO), 1/3.);\n"// apply linear gamma correction, negative input compatible
	"PO = tex3Dlod(LUT3D, float4((PO.rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
	"PO = PO.rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
	"PO = PO.rrr+float3(0, -.202008/.587, 1.772)*PO.ggg+float3(1.402, -.419198/.587, 0)*PO.bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
	"PO = PO.rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
	"PO = PO.rrr+float3(0, -.1674679/.894, 1.8556)*PO.ggg+float3(1.5748, -.4185031/.894, 0)*PO.bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;"// sample dither matrix
	"PO += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"PO = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue = float3(tex2Dlod(DitherMatrix, float4(dtt*c3.xy+dtt.yx*c3.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c4.xy+dtt.yx*c4.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c5.xy+dtt.yx*c5.zw+c2, 0, 0)).x);"// sample randomized dither matrix
	"PO += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"PO = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv = smoothstep(.125, 0, abs(Q1-Q2))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"PO += (rn.rgb-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
	"PO = (rn.rgb-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
	"return PO.rgbb;"
"}";

static char const gk_szBasicFrameInterpolationShader1[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// intra-frame times
"float2 t : register(c0);"
"struct PS_OUTPUT {float4 Color[2] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c5 : register(c5);"// p0g, p0g, p0g, p0g,
"float4 c6 : register(c6);"// p0b, p0b, p0b, p0b,
"float4 c7 : register(c7);"// p1r, p1r, p1r, p1r,
"float4 c8 : register(c8);"// p1g, p1g, p1g, p1g,
"float4 c9 : register(c9);"// p1b, p1b, p1b, p1b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"float3 Q0 = tex2Dlod(s0, float4(tex, 0, 0)).rgb, Q1 = tex2Dlod(s5, float4(tex, 0, 0)).rgb, Q2 = tex2Dlod(s4, float4(tex, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex, 0, 0)).rgb;"
	"PS_OUTPUT PO;"
	"[unroll] for(uint i = 0; i < 2; ++i) PO.Color[i].rgb = ((((Q1-Q2)*3+Q3-Q0)*t[i]+Q0*2+Q2*4-Q1*5-Q3)*t[i]+Q2-Q0)*t[i]*.5+Q1;\n"// interpolation output

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[2] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c4.xy+dtt.yx*c4.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c5.xy+dtt.yx*c5.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c6.xy+dtt.yx*c6.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c3.zw, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv = smoothstep(.125, 0, abs(Q1-Q2))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn2[2];"
	"rn2[0] = rn.rgb; rd(z) rn2[1] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 2; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn2[i]-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn2[i]-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

static char const gk_szBasicFrameInterpolationShader2[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// intra-frame times
"float3 t : register(c0);"
"struct PS_OUTPUT {float4 Color[3] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// r2r, r2r, r2g, r2g,
"float2 c5 : register(c5);"// r2b, r2b, ign, ign,
"float4 c6 : register(c6);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c7 : register(c7);"// p0g, p0g, p0g, p0g,
"float4 c8 : register(c8);"// p0b, p0b, p0b, p0b,
"float4 c9 : register(c9);"// p1r, p1r, p1r, p1r,
"float4 c10 : register(c10);"// p1g, p1g, p1g, p1g,
"float4 c11 : register(c11);"// p1b, p1b, p1b, p1b,
"float4 c12 : register(c12);"// p2r, p2r, p2r, p2r,
"float4 c13 : register(c13);"// p2g, p2g, p2g, p2g,
"float4 c14 : register(c14);"// p2b, p2b, p2b, p2b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"float3 Q0 = tex2Dlod(s0, float4(tex, 0, 0)).rgb, Q1 = tex2Dlod(s5, float4(tex, 0, 0)).rgb, Q2 = tex2Dlod(s4, float4(tex, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex, 0, 0)).rgb;"
	"PS_OUTPUT PO;"
	"[unroll] for(uint i = 0; i < 3; ++i) PO.Color[i].rgb = ((((Q1-Q2)*3+Q3-Q0)*t[i]+Q0*2+Q2*4-Q1*5-Q3)*t[i]+Q2-Q0)*t[i]*.5+Q1;\n"// interpolation output

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[3] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c6.xy+dtt.yx*c6.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c10.xy+dtt.yx*c10.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c11.xy+dtt.yx*c11.zw+c3.zw, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c12.xy+dtt.yx*c12.zw+c4.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c13.xy+dtt.yx*c13.zw+c4.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c14.xy+dtt.yx*c14.zw+c5, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv = smoothstep(.125, 0, abs(Q1-Q2))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn3[3];"
	"rn3[0] = rn.rgb; rd(z) rn3[1] = rn.rgb; rd(y) rn3[2] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 3; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn3[i]-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn3[i]-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

static char const gk_szBasicFrameInterpolationShader3[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// intra-frame times
"float4 t : register(c0);"
"struct PS_OUTPUT {float4 Color[4] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// r2r, r2r, r2g, r2g,
"float4 c5 : register(c5);"// r2b, r2b, r3r, r3r,
"float4 c6 : register(c6);"// r3g, r3g, r3b, r3b,
"float4 c7 : register(c7);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c8 : register(c8);"// p0g, p0g, p0g, p0g,
"float4 c9 : register(c9);"// p0b, p0b, p0b, p0b,
"float4 c10 : register(c10);"// p1r, p1r, p1r, p1r,
"float4 c11 : register(c11);"// p1g, p1g, p1g, p1g,
"float4 c12 : register(c12);"// p1b, p1b, p1b, p1b,
"float4 c13 : register(c13);"// p2r, p2r, p2r, p2r,
"float4 c14 : register(c14);"// p2g, p2g, p2g, p2g,
"float4 c15 : register(c15);"// p2b, p2b, p2b, p2b,
"float4 c16 : register(c16);"// p3r, p3r, p3r, p3r,
"float4 c17 : register(c17);"// p3g, p3g, p3g, p3g,
"float4 c18 : register(c18);"// p3b, p3b, p3b, p3b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"float3 Q0 = tex2Dlod(s0, float4(tex, 0, 0)).rgb, Q1 = tex2Dlod(s5, float4(tex, 0, 0)).rgb, Q2 = tex2Dlod(s4, float4(tex, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex, 0, 0)).rgb;"
	"PS_OUTPUT PO;"
	"[unroll] for(uint i = 0; i < 4; ++i) PO.Color[i].rgb = ((((Q1-Q2)*3+Q3-Q0)*t[i]+Q0*2+Q2*4-Q1*5-Q3)*t[i]+Q2-Q0)*t[i]*.5+Q1;\n"// interpolation output

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[4] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c10.xy+dtt.yx*c10.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c11.xy+dtt.yx*c11.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c12.xy+dtt.yx*c12.zw+c3.zw, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c13.xy+dtt.yx*c13.zw+c4.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c14.xy+dtt.yx*c14.zw+c4.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c15.xy+dtt.yx*c15.zw+c5.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c16.xy+dtt.yx*c16.zw+c5.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c17.xy+dtt.yx*c17.zw+c6.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c18.xy+dtt.yx*c18.zw+c6.zw, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv = smoothstep(.125, 0, abs(Q1-Q2))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn4[4];"
	"rn4[0] = rn.rgb; rd(z) rn4[1] = rn.rgb; rd(y) rn4[2] = rn.rgb; rd(x) rn4[3] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 4; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn4[i]-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn4[i]-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

static char const gk_szPreAdaptiveFrameInterpolationShader0[] =
"#define sp(n, a, b) \
float n = dot(1., abs(tex2Dlod(s4, float4(clamp(tex+float2(a, b)/float2(Ma, Mv), lim.xy, lim.zw), 0, 0)).rgb-Q));\
x = max((n*-di)+1., 0.);\
w += x;\
v += float2(a, b)/float2(Ma, Mv)/3.*x;\n"
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"float4 c0 : register(c0);"// the active video area

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// discard if outside of video area
	"[branch] if(tex.x < c0.x) return 0;"
	"[branch] if(tex.x > c0.z) return 0;"
	"[branch] if(tex.y < c0.y) return 0;"
	"[branch] if(tex.y > c0.w) return 0;"

	"const float dl = (2.-Mr)*9./1024., di = 1024./(9.*(2.-Mr));"// discard level, depending on color range: 18/1024 [0, 1], 9/1024 for [16384/65535, 49151/65535]
	"float4 lim = c0+float2(.5, -.5).xxyy/float2(Ma, Mv).xyxy;"// shrink the video area rectangle to match the outer pixel limits, used as sampling area limits
	// do motion detection
	"float2 v = 0; float w = 0, x;"
	"float3 Q = tex2Dlod(s3, float4(tex, 0, 0)).rgb;"
	"sp(O0, 0, 0)"// previous pixel on the same location
	"[branch] if(O0 < dl) return 0;"// no motion detected

	// neighboring pixels motion detection
	"sp(H1, -1, -1)"
	"sp(H2,  0, -1)"
	"sp(H3,  1, -1)"
	"sp(H4, -1,  0)"
	"sp(H5,  1,  0)"
	"sp(H6, -1,  1)"
	"sp(H7,  0,  1)"
	"sp(H8,  1,  1)"

	"[branch] if(H1 < dl || H2 < dl || H3 < dl || H4 < dl || H5 < dl || H6 < dl || H7 < dl || H8 < dl) return (v/w).xyyy;"

	// lowest motion detection
	// smallest hexagon
	// x+4/2, x+8/2, x+4/2
	//  -- --  y
	// |     | y+7/2
	// |     |
	//  -- --
	"sp(I1, -2, -3)"
	"sp(I2,  2, -3)"

	"sp(I3, -4,  0)"
	"sp(I4,  4,  0)"

	"sp(I5, -2,  3)"
	"sp(I6,  2,  3)"

	"[branch] if(I1 < dl || I2 < dl || I3 < dl || I4 < dl || I5 < dl || I6 < dl) return (v/w).xyyy;"// lowest motion detected

	// lower motion detection
	// smaller hexagon (90 degrees turned relative to the smallest one)
	// y+4, y+8, y+4
	//  -- --  x
	// |     | x+7
	// |     |
	//  -- --
	"sp(J1,  0, -8)"

	"sp(J2, -7, -4)"
	"sp(J3,  7, -4)"

	"sp(J4, -7,  4)"
	"sp(J5,  7,  4)"

	"sp(J6,  0,  8)"

	"[branch] if(J1 < dl || J2 < dl || J3 < dl || J4 < dl || J5 < dl || J6 < dl) return (v/w).xyyy;"// lower motion detected

	// low motion detection
	// small hexagon
	"sp(K1,  -8, -14)"
	"sp(K2,   8, -14)"

	"sp(K3, -16,   0)"
	"sp(K4,  16,   0)"

	"sp(K5,  -8,  14)"
	"sp(K6,   8,  14)"

	"[branch] if(K1 < dl || K2 < dl || K3 < dl || K4 < dl || K5 < dl || K6 < dl) return (v/w).xyyy;"// low motion detected

	// high motion detection
	// large hexagon
	"sp(L1,   0, -32)"

	"sp(L2, -28, -16)"
	"sp(L3,  28, -16)"

	"sp(L4, -28,  16)"
	"sp(L5,  28,  16)"

	"sp(L6,   0,  32)"

	"[branch] if(L1 < dl || L2 < dl || L3 < dl || L4 < dl || L5 < dl || L6 < dl) return (v/w).xyyy;"// high motion detected

	// higher motion detection
	// larger hexagon
	"sp(M1, -32, -56)"
	"sp(M2,  32, -56)"

	"sp(M3, -64,   0)"
	"sp(M4,  64,   0)"

	"sp(M5, -32,  56)"
	"sp(M6,  32,  56)\n"
"#if Mf == 8\n"
	"[branch] if(M1 < dl || M2 < dl || M3 < dl || M4 < dl || M5 < dl || M6 < dl) return (v/w).xyyy;"// higher motion detected

	// highest motion detection
	// largest hexagon
	"sp(N1,    0, -128)"

	"sp(N2, -112,  -64)"
	"sp(N3,  112,  -64)"

	"sp(N4, -112,   64)"
	"sp(N5,  112,   64)"

	"sp(N6,    0,  128)\n"
"#endif\n"
	"if(!w) return 0;"// no similarities detected
	"return (v/w).xyyy;"
"}";

// gk_szPreAdaptiveFrameInterpolationShaders note: linear filtering is active on for the sampler, so we sample at a .5 location to get a bit of blur with each sampling as well
static char const gk_szPreAdaptiveFrameInterpolationShader1[] =
"sampler s6 : register(s6);"
"float4 c0 : register(c0);"// the active video area

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// discard if outside of video area
	"[branch] if(tex.x < c0.x) return 0;"
	"[branch] if(tex.x > c0.z) return 0;"
	"[branch] if(tex.y < c0.y) return 0;"
	"[branch] if(tex.y > c0.w) return 0;"

	"float w = Ma;"
	"return (tex2Dlod(s6, float4(tex-float2(2.5/w, 0), 0, 0))+tex2Dlod(s6, float4(tex-float2(.5/w, 0), 0, 0))+tex2Dlod(s6, float4(tex+float2(.5/w, 0), 0, 0))+tex2Dlod(s6, float4(tex+float2(2.5/w, 0), 0, 0)))*.25;"
"}";

static char const gk_szPreAdaptiveFrameInterpolationShader2[] =
"sampler s6 : register(s6);"
"float4 c0 : register(c0);"// the active video area

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// discard if outside of video area
	"[branch] if(tex.x < c0.x) return 0;"
	"[branch] if(tex.x > c0.z) return 0;"
	"[branch] if(tex.y < c0.y) return 0;"
	"[branch] if(tex.y > c0.w) return 0;"

	"float h = Mv;"
	"return (tex2Dlod(s6, float4(tex-float2(0, 2.5/h), 0, 0))+tex2Dlod(s6, float4(tex-float2(0, .5/h), 0, 0))+tex2Dlod(s6, float4(tex+float2(0, .5/h), 0, 0))+tex2Dlod(s6, float4(tex+float2(0, 2.5/h), 0, 0)))*.25;"
"}";

static char const gk_szAdaptiveFrameInterpolationShader0[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// vector maps
"sampler s6 : register(s6);"// old map
"sampler s7 : register(s7);"// previous map
"sampler s8 : register(s8);"// next map
// intra-frame time
"float t : register(c0);\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float2 c2 : register(c2);"// r0b, r0b, ign, ign,
"float4 c3 : register(c3);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c4 : register(c4);"// p0g, p0g, p0g, p0g,
"float4 c5 : register(c5);"// p0b, p0b, p0b, p0b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"float3 PO;"
	"float ti = 1.-t;"

	// get sample coordinates
	"float2 V1 = tex2Dlod(s7, float4(tex, 0, 0)).xy;"

	// lower, relates to previous sample, use as positive offset
	"float2 lower = V1*t;"
	// upper, relates to next sample, use as negative offset
	"float2 upper = V1*ti;"
	// outer two samples
	"float2 V0 = tex2Dlod(s6, float4(tex+lower, 0, 0)).xy, V2 = tex2Dlod(s8, float4(tex-upper, 0, 0)).xy;"
	"float3 Q1 = tex2Dlod(s5, float4(tex+lower, 0, 0)).rgb;"
	"float3 Q2 = tex2Dlod(s4, float4(tex-upper, 0, 0)).rgb;"

	"float Ve = dot(1., abs(V1-V0)+abs(V1-V2));"
	"[flatten] if(Ve > 1/64.) {\n"// discard in case of incoherency
"#if Mf == 4\n"
		"PO = 0;}"
	"else PO = float3(V1*(2.-Mr)*64.+.5, Ve*(2.-Mr)*32+Mr*.25);\n"// test output
"#else\n"
		"PO = Q1*ti+Q2*t;}"
	"else {"
		"float3 Q0 = tex2Dlod(s0, float4(tex+lower+V0, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex-upper-V2, 0, 0)).rgb;"
		"PO = ((((Q1-Q2)*3+Q3-Q0)*t+Q0*2+Q2*4-Q1*5-Q3)*t+Q2-Q0)*t*.5+Q1;}\n"// interpolation output
"#endif\n"

"#if Mr\n"
	"PO = PO*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
	"if(Mv == 288 || Mv == 576) PO = mul(PO, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
	"else PO = mul(PO, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
	"PO = mul(PO, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
	"PO = sign(PO.rgb)*pow(abs(PO), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
	"PO = sign(PO.rgb)*pow(abs(PO), 1/3.);\n"// apply linear gamma correction, negative input compatible
	"PO = tex3Dlod(LUT3D, float4((PO*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
	"PO = PO.rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
	"PO = PO.rrr+float3(0, -.202008/.587, 1.772)*PO.ggg+float3(1.402, -.419198/.587, 0)*PO.bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
	"float3 sw = PO.rgb;"
	"PO = PO.rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
	"PO = PO.rrr+float3(0, -.1674679/.894, 1.8556)*PO.ggg+float3(1.5748, -.4185031/.894, 0)*PO.bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;"// sample dither matrix
	"PO += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"PO = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue = float3(tex2Dlod(DitherMatrix, float4(dtt*c3.xy+dtt.yx*c3.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c4.xy+dtt.yx*c4.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c5.xy+dtt.yx*c5.zw+c2, 0, 0)).x);"// sample randomized dither matrix
	"PO += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
	"PO = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv = smoothstep(.125, 0, abs(Q1-Q2))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"PO += (rn.rgb-.5)*dv;\n"// minimize the dithering on contours
 "#if Mt == 1\n"
	"PO = (rn.rgb-.5)*dv*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
	"return PO.rgbb;"
"}";

static char const gk_szAdaptiveFrameInterpolationShader1[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// vector maps
"sampler s6 : register(s6);"// old map
"sampler s7 : register(s7);"// previous map
"sampler s8 : register(s8);"// next map
// intra-frame times
"float2 t : register(c0);\n"
"struct PS_OUTPUT {float4 Color[2] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c5 : register(c5);"// p0g, p0g, p0g, p0g,
"float4 c6 : register(c6);"// p0b, p0b, p0b, p0b,
"float4 c7 : register(c7);"// p1r, p1r, p1r, p1r,
"float4 c8 : register(c8);"// p1g, p1g, p1g, p1g,
"float4 c9 : register(c9);"// p1b, p1b, p1b, p1b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"PS_OUTPUT PO;"
	"float2 ti = 1.-t;"

	// get sample coordinates
	"float2 V1 = tex2Dlod(s7, float4(tex, 0, 0)).xy;"

	"float3 Q1[2], Q2[2];"
	"[unroll] for(uint k = 0; k < 2; ++k) {"
		// lower, relates to previous sample, use as positive offset
		"float2 lower = V1*t[k];"
		// upper, relates to next sample, use as negative offset
		"float2 upper = V1*ti[k];"
		// vectors to outer two samples
		"float2 V0 = tex2Dlod(s6, float4(tex+lower, 0, 0)).xy, V2 = tex2Dlod(s8, float4(tex-upper, 0, 0)).xy;"
		"Q1[k] = tex2Dlod(s5, float4(tex+lower, 0, 0)).rgb;"
		"Q2[k] = tex2Dlod(s4, float4(tex-upper, 0, 0)).rgb;"

		"float Ve = dot(1., abs(V1-V0)+abs(V1-V2));"
		"[flatten] if(Ve > 1/64.) {\n"// discard in case of incoherency
"#if Mf == 4\n"
			"PO.Color[k].rgb = 0;}"
		"else PO.Color[k].rgb = float3(V1*(2.-Mr)*64.+.5, Ve*(2.-Mr)*32+Mr*.25);}\n"// test output
"#else\n"
			"PO.Color[k].rgb = Q1[k]*ti[k]+Q2[k]*t[k];}"
		"else {"
			"float3 Q0 = tex2Dlod(s0, float4(tex+lower+V0, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex-upper-V2, 0, 0)).rgb;"
			"PO.Color[k].rgb = ((((Q1[k]-Q2[k])*3+Q3-Q0)*t[k]+Q0*2+Q2[k]*4-Q1[k]*5-Q3)*t[k]+Q2[k]-Q0)*t[k]*.5+Q1[k];}}\n"// interpolation output
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[2] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c4.xy+dtt.yx*c4.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c5.xy+dtt.yx*c5.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c6.xy+dtt.yx*c6.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c3.zw, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv[2];"
	"[unroll] for(uint h = 0; h < 2; ++h) dv[h] = smoothstep(.125, 0, abs(Q1[h]-Q2[h]))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn2[2];"
	"rn2[0] = rn.rgb; rd(z) rn2[1] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 2; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn2[i]-.5)*dv[i];\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn2[i]-.5)*dv[i]*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

static char const gk_szAdaptiveFrameInterpolationShader2[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// vector maps
"sampler s6 : register(s6);"// old map
"sampler s7 : register(s7);"// previous map
"sampler s8 : register(s8);"// next map
// intra-frame times
"float3 t : register(c0);\n"
"struct PS_OUTPUT {float4 Color[3] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// r2r, r2r, r2g, r2g,
"float2 c5 : register(c5);"// r2b, r2b, ign, ign,
"float4 c6 : register(c6);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c7 : register(c7);"// p0g, p0g, p0g, p0g,
"float4 c8 : register(c8);"// p0b, p0b, p0b, p0b,
"float4 c9 : register(c9);"// p1r, p1r, p1r, p1r,
"float4 c10 : register(c10);"// p1g, p1g, p1g, p1g,
"float4 c11 : register(c11);"// p1b, p1b, p1b, p1b,
"float4 c12 : register(c12);"// p2r, p2r, p2r, p2r,
"float4 c13 : register(c13);"// p2g, p2g, p2g, p2g,
"float4 c14 : register(c14);"// p2b, p2b, p2b, p2b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"PS_OUTPUT PO;"
	"float3 ti = 1.-t;"

	// get sample coordinates
	"float2 V1 = tex2Dlod(s7, float4(tex, 0, 0)).xy;"

	"float3 Q1[3], Q2[3];"
	"[unroll] for(uint k = 0; k < 3; ++k) {"
		// lower, relates to previous sample, use as positive offset
		"float2 lower = V1*t[k];"
		// upper, relates to next sample, use as negative offset
		"float2 upper = V1*ti[k];"
		// vectors to outer two samples
		"float2 V0 = tex2Dlod(s6, float4(tex+lower, 0, 0)).xy, V2 = tex2Dlod(s8, float4(tex-upper, 0, 0)).xy;"
		"Q1[k] = tex2Dlod(s5, float4(tex+lower, 0, 0)).rgb;"
		"Q2[k] = tex2Dlod(s4, float4(tex-upper, 0, 0)).rgb;"

		"float Ve = dot(1., abs(V1-V0)+abs(V1-V2));"
		"[flatten] if(Ve > 1/64.) {\n"// discard in case of incoherency
"#if Mf == 4\n"
			"PO.Color[k].rgb = 0;}"
		"else PO.Color[k].rgb = float3(V1*(2.-Mr)*64.+.5, Ve*(2.-Mr)*32+Mr*.25);}\n"// test output
"#else\n"
			"PO.Color[k].rgb = Q1[k]*ti[k]+Q2[k]*t[k];}"
		"else {"
			"float3 Q0 = tex2Dlod(s0, float4(tex+lower+V0, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex-upper-V2, 0, 0)).rgb;"
			"PO.Color[k].rgb = ((((Q1[k]-Q2[k])*3+Q3-Q0)*t[k]+Q0*2+Q2[k]*4-Q1[k]*5-Q3)*t[k]+Q2[k]-Q0)*t[k]*.5+Q1[k];}}\n"// interpolation output
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[3] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c6.xy+dtt.yx*c6.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c10.xy+dtt.yx*c10.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c11.xy+dtt.yx*c11.zw+c3.zw, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c12.xy+dtt.yx*c12.zw+c4.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c13.xy+dtt.yx*c13.zw+c4.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c14.xy+dtt.yx*c14.zw+c5, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv[3];"
	"[unroll] for(uint h = 0; h < 3; ++h) dv[h] = smoothstep(.125, 0, abs(Q1[h]-Q2[h]))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn3[3];"
	"rn3[0] = rn.rgb; rd(z) rn3[1] = rn.rgb; rd(y) rn3[2] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 3; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn3[i]-.5)*dv[i];\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn3[i]-.5)*dv[i]*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

static char const gk_szAdaptiveFrameInterpolationShader3[] =
// RandomFactors, randomization factors, almost every type of input for each of the four components is allowed
"static const float4 RandomFactors = float4(pow(acos(-1), 4), exp(5), pow(13, acos(-1)*.5), sqrt(1997));\n"
"#define rd(a) rn.a = frac(dot(rn, RandomFactors));\n"

"sampler s0 : register(s0);"// old frame
"sampler s3 : register(s3);"// new frame
"sampler s4 : register(s4);"// next frame
"sampler s5 : register(s5);"// previous frame
// vector maps
"sampler s6 : register(s6);"// old map
"sampler s7 : register(s7);"// previous map
"sampler s8 : register(s8);"// next map
// intra-frame times
"float4 t : register(c0);\n"
"struct PS_OUTPUT {float4 Color[4] : COLOR0;};\n"

"#if Md >= 3\n"
"float4 c1 : register(c1);\n"// rnd, rnd, rnd, rnd : random numbers in the interval [0, 1)
"#elif Md == 2\n"
"float4 c1 : register(c1);"// r0r, r0r, r0g, r0g, : random numbers in the interval [0, 1)
"float4 c2 : register(c2);"// r0b, r0b, r1r, r1r,
"float4 c3 : register(c3);"// r1g, r1g, r1b, r1b,
"float4 c4 : register(c4);"// r2r, r2r, r2g, r2g,
"float4 c5 : register(c5);"// r2b, r2b, r3r, r3r,
"float4 c6 : register(c6);"// r3g, r3g, r3b, r3b,
"float4 c7 : register(c7);"// p0r, p0r, p0r, p0r, : ±1, 0 projection data for sampling directions
"float4 c8 : register(c8);"// p0g, p0g, p0g, p0g,
"float4 c9 : register(c9);"// p0b, p0b, p0b, p0b,
"float4 c10 : register(c10);"// p1r, p1r, p1r, p1r,
"float4 c11 : register(c11);"// p1g, p1g, p1g, p1g,
"float4 c12 : register(c12);"// p1b, p1b, p1b, p1b,
"float4 c13 : register(c13);"// p2r, p2r, p2r, p2r,
"float4 c14 : register(c14);"// p2g, p2g, p2g, p2g,
"float4 c15 : register(c15);"// p2b, p2b, p2b, p2b,
"float4 c16 : register(c16);"// p3r, p3r, p3r, p3r,
"float4 c17 : register(c17);"// p3g, p3g, p3g, p3g,
"float4 c18 : register(c18);"// p3b, p3b, p3b, p3b
"sampler DitherMatrix : register(s1);\n"
"#elif Md == 1\n"
"sampler DitherMatrix : register(s1);\n"
"#endif\n"

"#if Mc == 1\n"
"sampler LUT3D : register(s2);\n"
"#endif\n"

"PS_OUTPUT main(float2 tex : TEXCOORD0)"
"{"
	"float Width = Ma, Height = Mv, LUT3Dsize = Ms;"
	"PS_OUTPUT PO;"
	"float4 ti = 1.-t;"

	// get sample coordinates
	"float2 V1 = tex2Dlod(s7, float4(tex, 0, 0)).xy;"

	"float3 Q1[4], Q2[4];"
	"[unroll] for(uint k = 0; k < 4; ++k) {"
		// lower, relates to previous sample, use as positive offset
		"float2 lower = V1*t[k];"
		// upper, relates to next sample, use as negative offset
		"float2 upper = V1*ti[k];"
		// vectors to outer two samples
		"float2 V0 = tex2Dlod(s6, float4(tex+lower, 0, 0)).xy, V2 = tex2Dlod(s8, float4(tex-upper, 0, 0)).xy;"
		"Q1[k] = tex2Dlod(s5, float4(tex+lower, 0, 0)).rgb;"
		"Q2[k] = tex2Dlod(s4, float4(tex-upper, 0, 0)).rgb;"

		"float Ve = dot(1., abs(V1-V0)+abs(V1-V2));"
		"[flatten] if(Ve > 1/64.) {\n"// discard in case of incoherency
"#if Mf == 4\n"
			"PO.Color[k].rgb = 0;}"
		"else PO.Color[k].rgb = float3(V1*(2.-Mr)*64.+.5, Ve*(2.-Mr)*32+Mr*.25);}\n"// test output
"#else\n"
			"PO.Color[k].rgb = Q1[k]*ti[k]+Q2[k]*t[k];}"
		"else {"
			"float3 Q0 = tex2Dlod(s0, float4(tex+lower+V0, 0, 0)).rgb, Q3 = tex2Dlod(s3, float4(tex-upper-V2, 0, 0)).rgb;"
			"PO.Color[k].rgb = ((((Q1[k]-Q2[k])*3+Q3-Q0)*t[k]+Q0*2+Q2[k]*4-Q1[k]*5-Q3)*t[k]+Q2[k]-Q0)*t[k]*.5+Q1[k];}}\n"// interpolation output
"#endif\n"

"#if Md == 1\n"
	// static ordered
	"float3 DitherValue = tex2Dlod(DitherMatrix, float4(tex*float2(Width, Height)/128., 0, 0)).rrr;\n"// sample dither matrix
"#elif Md == 2\n"
	// random ordered
	"float2 dtt = tex*float2(Width, Height)/128.;"
	"float3 DitherValue[4] = {"// sample randomized dither matrix
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c7.xy+dtt.yx*c7.zw+c1.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c8.xy+dtt.yx*c8.zw+c1.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c9.xy+dtt.yx*c9.zw+c2.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c10.xy+dtt.yx*c10.zw+c2.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c11.xy+dtt.yx*c11.zw+c3.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c12.xy+dtt.yx*c12.zw+c3.zw, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c13.xy+dtt.yx*c13.zw+c4.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c14.xy+dtt.yx*c14.zw+c4.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c15.xy+dtt.yx*c15.zw+c5.xy, 0, 0)).x),"
		"float3(tex2Dlod(DitherMatrix, float4(dtt*c16.xy+dtt.yx*c16.zw+c5.zw, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c17.xy+dtt.yx*c17.zw+c6.xy, 0, 0)).x, tex2Dlod(DitherMatrix, float4(dtt*c18.xy+dtt.yx*c18.zw+c6.zw, 0, 0)).x)};\n"
"#elif Md >= 3\n"
	// adaptive random
	"float3 dv[4];"
	"[unroll] for(uint h = 0; h < 4; ++h) dv[h] = smoothstep(.125, 0, abs(Q1[h]-Q2[h]))*Md/float(Mq);"// color change detection, quantization of dithering noise

	"float4 rn = tex.xyyx+c1;"// input coordinates and time factors
	"[unroll] for(uint j = 0; j < 3; ++j) {rd(w) rd(z) rd(y) rd(x)}"// randomize, a low run count will make patterned noise
	"float3 rn4[4];"
	"rn4[0] = rn.rgb; rd(z) rn4[1] = rn.rgb; rd(y) rn4[2] = rn.rgb; rd(x) rn4[3] = rn.rgb;\n"
"#endif\n"
	"[unroll] for(uint i = 0; i < 4; ++i) {\n"
"#if Mr\n"
		"PO.Color[i].rgb = PO.Color[i].rgb*65535/32767.-16384/32767.;\n"// restore to full range
"#endif\n"
"#if Mc == 2\n"
		"if(Mv == 288 || Mv == 576) PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.3361771385, -1.0555622945, .0739045998, -1.5174823698, 2.0430369477, -.2491756179, -.5181995299, .0452558574, 1.1643003348));"// convert to PAL/SECAM display RGB
		"else PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(3.8182400492, -1.1642545310, .0613211301, -1.8947325752, 2.1539154215, -.2145178636, -.5925108740, .0383037068, 1.1434587210));\n"// convert to NTSC display RGB
"#else\n"
		"PO.Color[i].rgb = mul(PO.Color[i].rgb, float3x3(Mm));\n"// convert to display RGB
"#endif\n"
"#if Mc != 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/2.4);\n"// apply linear gamma correction, negative input compatible
"#endif\n"
"#if Mc == 1\n"
		"PO.Color[i].rgb = sign(PO.Color[i].rgb)*pow(abs(PO.Color[i].rgb), 1/3.);\n"// apply linear gamma correction, negative input compatible
		"PO.Color[i].rgb = tex3Dlod(LUT3D, float4((PO.Color[i].rgb*LUT3Dsize+.5)/(LUT3Dsize+1.), 0)).rgb;\n"// make the sampling position line up with an exact pixel coordinate and sample it
"#elif Mc == 2\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+PO.Color[i].bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.202008/.587, 1.772)*PO.Color[i].ggg+float3(1.402, -.419198/.587, 0)*PO.Color[i].bbb;\n"// SD Y'CbCr to R'G'B'
"#elif Mc == 3\n"
		"PO.Color[i].rgb = PO.Color[i].rrr*float3(.2126*219/255., -.1063/.9278*224/255., .5*224/255.)+PO.Color[i].ggg*float3(.7152*219/255., -.3576/.9278*224/255., -.3576/.7874*224/255.)+PO.Color[i].bbb*float3(.0722*219/255., .5*224/255., -.0361/.7874*224/255.)+float3(16/255., .5/255., .5/255.);"// HD R'G'B' to Y'CbCr and compress ranges
		"PO.Color[i].rgb = PO.Color[i].rrr+float3(0, -.1674679/.894, 1.8556)*PO.Color[i].ggg+float3(1.5748, -.4185031/.894, 0)*PO.Color[i].bbb;\n"// HD Y'CbCr to R'G'B'
"#endif\n"

"#if Md == 1\n"
		"PO.Color[i].rgb += DitherValue;\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md == 2\n"
		"PO.Color[i].rgb += DitherValue[i];\n"// apply dither
 "#if Mt == 1\n"
		"PO.Color[i].rgb = DitherValue[i]*Mq+.5;\n"// view the dithering noise directly
 "#endif\n"
"#elif Md >= 3\n"
		"PO.Color[i].rgb += (rn4[i]-.5)*dv[i];\n"// minimize the dithering on contours
 "#if Mt == 1\n"
		"PO.Color[i].rgb = (rn4[i]-.5)*dv[i]*Mq/float(Md)+.5;\n"// view the dithering noise directly
 "#endif\n"
"#endif\n"
		"PO.Color[i].a = PO.Color[i].b;"
	"}"
	"return PO;"
"}";

extern char const gk_szSubtitlePassShader[] =
// subtitle blending fix, the subtitle renderer outputs textures in a fundamentally flawed format, on top of the next statement:
// The current subtitle renderer uses pre-multiplied alpha (multiplication by 1.0-alpha on source, for D3DBLEND_ONE), this is very bad for blending colors. Change this characteristic of the subtitle renderer ASAP. D3DRS_DESTBLEND is fine as it is.
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float4 s1 = tex2D(s0, tex);"// original pixel
	"float av = 1.-s1.a;"
	"s1.rgb /= av;"// remove pre-multiplied alpha
	// more junk left by VSFilter legacy; it doesn't support the HD matrix at all, so we always have to use the SD matrix, and the input R'G'B' isn't clean either, compression to Y'CbCr limited ranges is required
	"s1.rgb = s1.rrr*float3(.299*219/255., -.1495/.886*224/255., .5*224/255.)+s1.ggg*float3(.587*219/255., -.2935/.886*224/255., -.2935/.701*224/255.)+s1.bbb*float3(.114*219/255., .5*224/255., -.057/.701*224/255.)+float3(16/255., .5/255., .5/255.);"// SD R'G'B' to Y'CbCr and compress ranges

	"if(Mw < 1120 && Mh < 630) s1.rgb = s1.rrr+float3(0, -.202008/.587, 1.772)*s1.ggg+float3(1.402, -.419198/.587, 0)*s1.bbb;"// SD Y'CbCr to R'G'B'
	"else s1.rgb = s1.rrr+float3(0, -.1674679/.894, 1.8556)*s1.ggg+float3(1.5748, -.4185031/.894, 0)*s1.bbb;"// HD Y'CbCr to R'G'B'
	"s1.rgb = sign(s1.rgb)*pow(abs(s1.rgb), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1.rgb = mul(s1.rgb, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1.rgb = s1.rgb*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1;"
"}";

extern char const gk_szOSDPassShader[] =
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float4 s1 = tex2D(s0, tex);"// original pixel
	"s1.rgb = pow(s1.rgb, 2.4);"// to linear RGB, the source is guaranteed without negative values

	"s1.rgb = mul(s1.rgb, float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391));\n"// convert sRGB/HD RGB to XYZ
"#if Mr\n"
	"s1.rgb = s1.rgb*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1;"
"}";

extern char const gk_szInitialGammaShader[] =
// linearize gamma, for video input
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float3 s1 = tex2D(s0, tex).rgb;"// original pixel
	"s1 = sign(s1)*pow(abs(s1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1 = mul(s1, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1 = s1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

extern char const gk_szRGBconvYCCShader[] =
"#if Ml\n"
"#define tex2D(s, t) tex2Dlod(s, float4(t, 0, 0))\n"
"#endif\n"
"sampler s0 : register(s0);\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"float3 s1 = tex2D(s0, tex).rgb;"// original pixel
	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr*float3(.299, -.1495/.886, .5)+s1.ggg*float3(.587, -.2935/.886, -.2935/.701)+s1.bbb*float3(.114, .5, -.057/.701);"// SD R'G'B' to Y'CbCr
	"else s1 = s1.rrr*float3(.2126, -.1063/.9278, .5)+s1.ggg*float3(.7152, -.3576/.9278, -.3576/.7874)+s1.bbb*float3(.0722, .5, -.0361/.7874);\n"// HD R'G'B' to Y'CbCr
"#if Mr\n"
	"s1 = s1*32767/65535.+float3(16384/65535., 32767/65535., 32767/65535.);\n"// convert to alternative limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

static char const gk_szInitialPassShader0[] =
// 4:2:2 Bilinear
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(s1, 0)"// original pixel
	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr*float3(.299, -.1495/.886, .5)+s1.ggg*float3(.587, -.2935/.886, -.2935/.701)+s1.bbb*float3(.114, .5, -.057/.701);"// SD R'G'B' to Y'CbCr
	"else s1 = s1.rrr*float3(.2126, -.1063/.9278, .5)+s1.ggg*float3(.7152, -.3576/.9278, -.3576/.7874)+s1.bbb*float3(.0722, .5, -.0361/.7874);"// HD R'G'B' to Y'CbCr

	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(col, n)"// original pixels
	"float2 col2;"
	"if(Mw < 1120 && Mh < 630) col2 = col.rr*float2(-.1495/.886, .5)+col.gg*float2(-.2935/.886, -.2935/.701)+col.bb*float2(.5, -.057/.701);"// SD R'G'B' to Y'CbCr
	"else col2 = col.rr*float2(-.1063/.9278, .5)+col.gg*float2(-.3576/.9278, -.3576/.7874)+col.bb*float2(.5, -.0361/.7874);"// HD R'G'B' to Y'CbCr
	"s1.yz = col2*.25+s1.yz*.75;\n"// blur the chroma with the adjacent pixel
"#else\n"
	"if(n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(col, 1)"// sample additional pixel
		"float2 col2;"
		"if(Mw < 1120 && Mh < 630) col2 = col.rr*float2(-.1495/.886, .5)+col.gg*float2(-.2935/.886, -.2935/.701)+col.bb*float2(.5, -.057/.701);"// SD R'G'B' to Y'CbCr
		"else col2 = col.rr*float2(-.1063/.9278, .5)+col.gg*float2(-.3576/.9278, -.3576/.7874)+col.bb*float2(.5, -.0361/.7874);"// HD R'G'B' to Y'CbCr
		"s1.yz = (col2+s1.yz)*.5;"// blur the chroma with the adjacent pixel
	"}\n"
"#endif\n"

	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr+float3(0, -.202008/.587, 1.772)*s1.ggg+float3(1.402, -.419198/.587, 0)*s1.bbb;"// SD Y'CbCr to R'G'B'
	"else s1 = s1.rrr+float3(0, -.1674679/.894, 1.8556)*s1.ggg+float3(1.5748, -.4185031/.894, 0)*s1.bbb;"// HD Y'CbCr to R'G'B'

	"s1 = sign(s1)*pow(abs(s1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1 = mul(s1, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1 = s1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

static char const gk_szInitialPassShader1[] =
// 4:2:2 Perlin Smootherstep
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(s1, 0)"// original pixel
	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr*float3(.299, -.1495/.886, .5)+s1.ggg*float3(.587, -.2935/.886, -.2935/.701)+s1.bbb*float3(.114, .5, -.057/.701);"// SD R'G'B' to Y'CbCr
	"else s1 = s1.rrr*float3(.2126, -.1063/.9278, .5)+s1.ggg*float3(.7152, -.3576/.9278, -.3576/.7874)+s1.bbb*float3(.0722, .5, -.0361/.7874);"// HD R'G'B' to Y'CbCr

	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(col, n)"// original pixels
	"float2 col2;"
	"if(Mw < 1120 && Mh < 630) col2 = col.rr*float2(-.1495/.886, .5)+col.gg*float2(-.2935/.886, -.2935/.701)+col.bb*float2(.5, -.057/.701);"// SD R'G'B' to Y'CbCr
	"else col2 = col.rr*float2(-.1063/.9278, .5)+col.gg*float2(-.3576/.9278, -.3576/.7874)+col.bb*float2(.5, -.0361/.7874);"// HD R'G'B' to Y'CbCr
	"s1.yz = col2*.103515625+s1.yz*.896484375;\n"// blur the chroma with the adjacent pixel
"#else\n"
	"if(n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(col, 1)"// sample additional pixel
		"float2 col2;"
		"if(Mw < 1120 && Mh < 630) col2 = col.rr*float2(-.1495/.886, .5)+col.gg*float2(-.2935/.886, -.2935/.701)+col.bb*float2(.5, -.057/.701);"// SD R'G'B' to Y'CbCr
		"else col2 = col.rr*float2(-.1063/.9278, .5)+col.gg*float2(-.3576/.9278, -.3576/.7874)+col.bb*float2(.5, -.0361/.7874);"// HD R'G'B' to Y'CbCr
		"s1.yz = (col2+s1.yz)*.5;"// blur the chroma with the adjacent pixel
	"}\n"
"#endif\n"

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1 = mul(s1, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1 = s1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

static char const gk_szInitialPassShader2[] =
// 4:2:2 Bicubic A=-0.6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = mul(float4(-.084375, .871875, .240625, -.028125), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz));\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -1) sp(Q2, 1) sp(Q3, 2)"// sample additional pixels
		"Q1.yz = (Q0.yz+Q3.yz)*-.075+(Q1.yz+Q2.yz)*.575;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader3[] =
// 4:2:2 Bicubic A=-0.8
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = mul(float4(-.1125, .88125, .26875, -.0375), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz));\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -1) sp(Q2, 1) sp(Q3, 2)"// sample additional pixels
		"Q1.yz = (Q0.yz+Q3.yz)*-.1+(Q1.yz+Q2.yz)*.6;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader4[] =
// 4:2:2 Bicubic A=-1.0
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = mul(float4(-.140625, .890625, .296875, -.046875), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz));\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -1) sp(Q2, 1) sp(Q3, 2)"// sample additional pixels
		"Q1.yz = (Q0.yz+Q3.yz)*-.125+(Q1.yz+Q2.yz)*.625;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader5[] =
// 4:2:2 B-spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = Q0.yz*9/128.+Q1.yz*235/384.+Q2.yz*121/384.+Q3.yz/384.;\n"// chroma interpolation
"#else\n"
	"sp(Q0, -1) sp(Q2, 1)"// original pixels
	"if (n > .5) {"// even samples
		"sp(Q3, 2)"// sample additional pixel
		"Q1.yz = (Q0.yz+Q3.yz)/48.+(Q1.yz+Q2.yz)*23/48.;}"// chroma interpolation
	"else {"// odd samples
		"Q1.yz = (Q0.yz+Q2.yz)/6.+Q1.yz*2/3.;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader6[] =
// 4:2:2 Mitchell-Netravali spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = Q1.yz*901/1152.+Q2.yz*295/1152.-Q0.yz*3/128.-Q3.yz*17/1152.;\n"// chroma interpolation
"#else\n"
	"sp(Q0, -1) sp(Q2, 1)"// original pixels
	"if (n > .5) {"// even samples
		"sp(Q3, 2)"// sample additional pixel
		"Q1.yz = (Q0.yz+Q3.yz)*-5/144.+(Q1.yz+Q2.yz)*77/144.;}"// chroma interpolation
	"else {"// odd samples
		"Q1.yz = (Q0.yz+Q2.yz)/18.+Q1.yz*16/18.;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader7[] =
// 4:2:2 Catmull-Rom spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"Q1.yz = Q1.yz*.8671875+Q2.yz*.2265625-Q0.yz*.0703125-Q3.yz*.0234375;\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -1) sp(Q2, 1) sp(Q3, 2)"// sample additional pixels
		"Q1.yz = (Q0.yz+Q3.yz)*-.0625+(Q1.yz+Q2.yz)*.5625;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader8[] =
// 4:2:2 B-spline6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q2, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -2*n) sp(Q1, -n) sp(Q3, n) sp(Q4, 2*n) sp(Q5, 3*n)"// original pixels
	"Q2.yz = Q0.yz*81/40960.+Q1.yz*15349/122880.+Q2.yz*31927/61440.+Q3.yz*6719/20480.+Q4.yz*3119/122880.+Q5.yz/122880.;\n"// chroma interpolation
"#else\n"
	"sp(Q0, -2) sp(Q1, -1) sp(Q3, 1) sp(Q4, 2)"// original pixels
	"if (n > .5) {"// even samples
		"sp(Q5, 3)"// sample additional pixel
		"Q2.yz = (Q0.yz+Q5.yz)/3840.+(Q1.yz+Q4.yz)*.06171875+(Q2.yz+Q3.yz)*841/1920.;}"// chroma interpolation
	"else {"// odd samples
		"Q2.yz = (Q0.yz+Q4.yz)/120.+(Q1.yz+Q3.yz)*13/60.+Q2.yz*.55;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q2 = Q2*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q2 = Q2.rrr+float3(0, -.202008/.587, 1.772)*Q2.ggg+float3(1.402, -.419198/.587, 0)*Q2.bbb;"// SD Y'CbCr to R'G'B'
	"else Q2 = Q2.rrr+float3(0, -.1674679/.894, 1.8556)*Q2.ggg+float3(1.5748, -.4185031/.894, 0)*Q2.bbb;"// HD Y'CbCr to R'G'B'

	"Q2 = sign(Q2)*pow(abs(Q2), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q2 = mul(Q2, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q2 = Q2*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q2.rgbb;"
"}";

static char const gk_szInitialPassShader9[] =
// 4:2:2 Catmull-Rom spline6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q2, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -2*n) sp(Q1, -n) sp(Q3, n) sp(Q4, 2*n) sp(Q5, 3*n)"// original pixels
	"Q2.yz = Q0.yz*.0032958984375+Q2.yz*.908935546875+Q3.yz*.193603515625+Q5.yz*.0010986328125-Q4.yz*.0157470703125-Q1.yz*.0911865234375\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -2) sp(Q1, -1) sp(Q3, 1) sp(Q4, 2) sp(Q5, 3)"// sample additional pixels
		"Q2.yz = (Q0.yz+Q5.yz)*.00390625+(Q1.yz+Q4.yz)*-.07421875+(Q2.yz+Q3.yz)*.5703125;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q2 = Q2*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q2 = Q2.rrr+float3(0, -.202008/.587, 1.772)*Q2.ggg+float3(1.402, -.419198/.587, 0)*Q2.bbb;"// SD Y'CbCr to R'G'B'
	"else Q2 = Q2.rrr+float3(0, -.1674679/.894, 1.8556)*Q2.ggg+float3(1.5748, -.4185031/.894, 0)*Q2.bbb;"// HD Y'CbCr to R'G'B'

	"Q2 = sign(Q2)*pow(abs(Q2), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q2 = mul(Q2, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q2 = Q2*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q2.rgbb;"
"}";

static char const gk_szInitialPassShader10[] =
// 4:2:2 Catmull-Rom spline8
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q3, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -3*n) sp(Q1, -2*n) sp(Q2, -n) sp(Q4, n) sp(Q5, 2*n) sp(Q6, 3*n) sp(Q7, 4*n)"// original pixels
	"Q3.yz = Q0.yz*-.000102996826171875+Q1.yz*.004978179931640625+Q2.yz*-.102413177490234375+Q3.yz*.933551788330078125+Q4.yz*.171871185302734375+Q5.yz*-.009052276611328125+Q6.yz*.001201629638671875+Q7.yz*-.000034332275390625;\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -3) sp(Q1, -2) sp(Q2, -1) sp(Q4, 1) sp(Q5, 2) sp(Q6, 3) sp(Q7, 4)"// sample additional pixels
		"Q3.yz = (Q0.yz+Q7.yz)/-6144.+(Q1.yz+Q6.yz)*37/6144.+(Q2.yz+Q5.yz)*-.07958984375+(Q3.yz+Q4.yz)*.57373046875;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q3 = Q3*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q3 = Q3.rrr+float3(0, -.202008/.587, 1.772)*Q3.ggg+float3(1.402, -.419198/.587, 0)*Q3.bbb;"// SD Y'CbCr to R'G'B'
	"else Q3 = Q3.rrr+float3(0, -.1674679/.894, 1.8556)*Q3.ggg+float3(1.5748, -.4185031/.894, 0)*Q3.bbb;"// HD Y'CbCr to R'G'B'

	"Q3 = sign(Q3)*pow(abs(Q3), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q3 = mul(Q3, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q3 = Q3*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q3.rgbb;"
"}";

static char const gk_szInitialPassShader11[] =
// 4:2:2 compensated Lanczos2
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q1, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -n) sp(Q2, n) sp(Q3, 2*n)"// original pixels
	"float4 wset = float4(1.25, .25, .75, 1.75);"
	"float4 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"
	"float wc = 1.-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w.y += wc*.75;"
	"w.z += wc*.25;"
	"Q1.yz = w.x*Q0.yz+w.y*Q1.yz+w.z*Q2.yz+w.w*Q3.yz;\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -1) sp(Q2, 1) sp(Q3, 2)"// sample additional pixels
		"float2 wset = float2(1.5, .5);"
		"float2 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"
		"float wc = .5-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w.y += wc;"
		"Q1.yz = (Q0.yz+Q3.yz)*w.x+(Q1.yz+Q2.yz)*w.y;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q1 = Q1*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q1 = Q1.rrr+float3(0, -.202008/.587, 1.772)*Q1.ggg+float3(1.402, -.419198/.587, 0)*Q1.bbb;"// SD Y'CbCr to R'G'B'
	"else Q1 = Q1.rrr+float3(0, -.1674679/.894, 1.8556)*Q1.ggg+float3(1.5748, -.4185031/.894, 0)*Q1.bbb;"// HD Y'CbCr to R'G'B'

	"Q1 = sign(Q1)*pow(abs(Q1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q1 = mul(Q1, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q1 = Q1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q1.rgbb;"
"}";

static char const gk_szInitialPassShader12[] =
// 4:2:2 compensated Lanczos3
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q2, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -2*n) sp(Q1, -n) sp(Q3, n) sp(Q4, 2*n) sp(Q5, 3*n)"// original pixels
	"float3 wset0 = float3(2.25, 1.25, .25);"
	"float3 wset1 = float3(.75, 1.75, 2.75);"
	"float3 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
	"float3 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"
	"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w0.z += wc*.75;"
	"w1.x += wc*.25;"
	"Q2.yz = w0.x*Q0.yz+w0.y*Q1.yz+w0.z*Q2.yz+w1.x*Q3.yz+w1.y*Q4.yz+w1.z*Q5.yz;\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -2) sp(Q1, -1) sp(Q3, 1) sp(Q4, 2) sp(Q5, 3)"// sample additional pixels
		"float3 wset = float3(2.5, 1.5, .5);"
		"float3 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"
		"float wc = .5-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w.z += wc;"
		"Q2.yz = (Q0.yz+Q5.yz)*w.x+(Q1.yz+Q4.yz)*w.y+(Q2.yz+Q3.yz)*w.z;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q2 = Q2*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q2 = Q2.rrr+float3(0, -.202008/.587, 1.772)*Q2.ggg+float3(1.402, -.419198/.587, 0)*Q2.bbb;"// SD Y'CbCr to R'G'B'
	"else Q2 = Q2.rrr+float3(0, -.1674679/.894, 1.8556)*Q2.ggg+float3(1.5748, -.4185031/.894, 0)*Q2.bbb;"// HD Y'CbCr to R'G'B'

	"Q2 = sign(Q2)*pow(abs(Q2), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q2 = mul(Q2, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q2 = Q2*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q2.rgbb;"
"}";

static char const gk_szInitialPassShader13[] =
// 4:2:2 compensated Lanczos4
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float3 a = tex2Dlod(s0, float4(tex+float2(b, 0)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b) float3 a = tex2D(s0, tex+float2(b, 0)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	"sp(Q3, 0)"// original pixel
	// detect even or odd coordinates for 4:2:2 sub-sampled chroma
	"float n = frac(tex.x*Mw*.5);\n"
"#if My\n"// no horizontal cositing
	"n = (n > .5)? 1 : -1;"// even x positive, odd x negative
	"sp(Q0, -3*n) sp(Q1, -2*n) sp(Q2, -n) sp(Q4, n) sp(Q5, 2*n) sp(Q6, 3*n) sp(Q7, 4*n)"// original pixels
	"float4 wset0 = float4(3.25, 2.25, 1.25, .25);"
	"float4 wset1 = float4(.75, 1.75, 2.75, 3.75);"
	"float4 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
	"float4 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"
	"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w0.w += wc*.75;"
	"w1.x += wc*.25;"
	"Q3.yz = w0.x*Q0.yz+w0.y*Q1.yz+w0.z*Q2.yz+w0.w*Q3.yz+w1.x*Q4.yz+w1.y*Q5.yz+w1.z*Q6.yz+w1.w*Q7.yz;\n"// chroma interpolation
"#else\n"
	"if (n > .5) {"// only even samples, this method just returns the original pixel at t=0 (odd samples)
		"sp(Q0, -3) sp(Q1, -2) sp(Q2, -1) sp(Q4, 1) sp(Q5, 2) sp(Q6, 3) sp(Q7, 4)"// sample additional pixels
		"float4 wset = float4(3.5, 2.5, 1.5, .5);"
		"float4 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"
		"float wc = .5-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
		"w.w += wc;"
		"Q3.yz = (Q0.yz+Q7.yz)*w.x+(Q1.yz+Q6.yz)*w.y+(Q2.yz+Q5.yz)*w.z+(Q3.yz+Q4.yz)*w.w;}\n"// chroma interpolation
"#endif\n"
"#if Mr\n"
	"Q3 = Q3*65535/32767.-float3(16384/32767., 1, 1);\n"// expand to full range
"#endif\n"

	"if(Mw < 1120 && Mh < 630) Q3 = Q3.rrr+float3(0, -.202008/.587, 1.772)*Q3.ggg+float3(1.402, -.419198/.587, 0)*Q3.bbb;"// SD Y'CbCr to R'G'B'
	"else Q3 = Q3.rrr+float3(0, -.1674679/.894, 1.8556)*Q3.ggg+float3(1.5748, -.4185031/.894, 0)*Q3.bbb;"// HD Y'CbCr to R'G'B'

	"Q3 = sign(Q3)*pow(abs(Q3), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"Q3 = mul(Q3, mat);\n"// convert to XYZ
"#if Mr\n"
	"Q3 = Q3*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return Q3.rgbb;"
"}";

static char const gk_szInitialPassShader14[] =
// 4:2:0 Bicubic A=-0.6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, mul(float4(-.084375, .871875, .240625, -.028125), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz)), Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader15[] =
// 4:2:0 Bicubic A=-0.8
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, mul(float4(-.1125, .88125, .26875, -.0375), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz)), Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader16[] =
// 4:2:0 Bicubic A=-1.0
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, mul(float4(-.140625, .890625, .296875, -.046875), float4x2(Q0.yz, Q1.yz, Q2.yz, Q3.yz)), Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader17[] =
// 4:2:0 B-spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, Q0.yz*9/128.+Q1.yz*235/384.+Q2.yz*121/384.+Q3.yz/384., Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader18[] =
// 4:2:0 Mitchell-Netravali spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, Q1.yz*901/1152.+Q2.yz*295/1152.-Q0.yz*3/128.-Q3.yz*17/1152., Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader19[] =
// 4:2:0 Catmull-Rom spline4
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"return float4(Q1.x, Q1.yz*.8671875+Q2.yz*.2265625-Q0.yz*.0703125-Q3.yz*.0234375, Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader20[] =
// 4:2:0 B-spline6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -2) sp(Q1, -1) sp(Q2, 0) sp(Q3, 1) sp(Q4, 2) sp(Q5, 3)"// original pixels
	"return float4(Q2.x, Q0.yz*81/40960.+Q1.yz*15349/122880.+Q2.yz*31927/61440.+Q3.yz*6719/20480.+Q4.yz*3119/122880.+Q5.yz/122880., Q2.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader21[] =
// 4:2:0 Catmull-Rom spline6
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -2) sp(Q1, -1) sp(Q2, 0) sp(Q3, 1) sp(Q4, 2) sp(Q5, 3)"// original pixels
	"return float4(Q2.x, Q0.yz*.0032958984375+Q2.yz*.908935546875+Q3.yz*.193603515625+Q5.yz*.0010986328125-Q1.yz*.0911865234375-Q4.yz*.0157470703125, Q2.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader22[] =
// 4:2:0 Catmull-Rom spline8
"sampler s0 : register(s0);\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -3) sp(Q1, -2) sp(Q2, -1) sp(Q3, 0) sp(Q4, 1) sp(Q5, 2) sp(Q6, 3) sp(Q7, 4)"// original pixels
	"return float4(Q3.x, Q0.yz*-.000102996826171875+Q1.yz*.004978179931640625+Q2.yz*-.102413177490234375+Q3.yz*.933551788330078125+Q4.yz*.171871185302734375+Q5.yz*-.009052276611328125+Q6.yz*.001201629638671875+Q7.yz*-.000034332275390625, Q3.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader23[] =
// 4:2:0 compensated Lanczos2
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -1) sp(Q1, 0) sp(Q2, 1) sp(Q3, 2)"// original pixels
	"float4 wset = float4(1.25, .25, .75, 1.75);"
	"float4 w = sin(wset*PI)*sin(wset*PI*.5)/(wset*wset*PI*PI*.5);"
	"float wc = 1.-dot(1, w);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w.y += wc*.75;"
	"w.z += wc*.25;"
	"return float4(Q1.x, w.x*Q0.yz+w.y*Q1.yz+w.z*Q2.yz+w.w*Q3.yz, Q1.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader24[] =
// 4:2:0 compensated Lanczos3
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -2) sp(Q1, -1) sp(Q2, 0) sp(Q3, 1) sp(Q4, 2) sp(Q5, 3)"// original pixels
	"float3 wset0 = float3(2.25, 1.25, .25);"
	"float3 wset1 = float3(.75, 1.75, 2.75);"
	"float3 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
	"float3 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"
	"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w0.z += wc*.75;"
	"w1.x += wc*.25;"
	"return float4(Q2.x, w0.x*Q0.yz+w0.y*Q1.yz+w0.z*Q2.yz+w1.x*Q3.yz+w1.y*Q4.yz+w1.z*Q5.yz, Q2.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader25[] =
// 4:2:0 compensated Lanczos4
"sampler s0 : register(s0);\n"
"#define PI acos(-1)\n"
"#if Ml\n"
"#define sp(a, b) float4 a = tex2Dlod(s0, float4(tex+float2(0, b)*2/float2(Mw, Mh), 0, 0));\n"
"#else\n"
"#define sp(a, b) float4 a = tex2D(s0, tex+float2(0, b)*2/float2(Mw, Mh));\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect half of the even or odd coordinates for 4:2:0 sub-sampled chroma
	"float n = frac(tex.y*Mh*.5);"
	"n = (n > .5)? 1 : -1;"// even y positive, odd y negative
	"sp(Q0, -3) sp(Q1, -2) sp(Q2, -1) sp(Q3, 0) sp(Q4, 1) sp(Q5, 2) sp(Q6, 3) sp(Q7, 4)"// original pixels
	"float4 wset0 = float4(3.25, 2.25, 1.25, .25);"
	"float4 wset1 = float4(.75, 1.75, 2.75, 3.75);"
	"float4 w0 = sin(wset0*PI)*sin(wset0*PI*.5)/(wset0*wset0*PI*PI*.5);"
	"float4 w1 = sin(wset1*PI)*sin(wset1*PI*.5)/(wset1*wset1*PI*PI*.5);"
	"float wc = 1.-dot(1, w0+w1);"// compensate truncated window factor by bilinear factoring on the two nearest samples
	"w0.w += wc*.75;"
	"w1.x += wc*.25;"
	"return float4(Q3.x, w0.x*Q0.yz+w0.y*Q1.yz+w0.z*Q2.yz+w0.w*Q3.yz+w1.x*Q4.yz+w1.y*Q5.yz+w1.z*Q6.yz+w1.w*Q7.yz, Q3.a);"// interpolated Y'CbCr output
"}";

static char const gk_szInitialPassShader26[] =
// 4:2:0 Bilinear
"sampler s0 : register(s0);\n"
"#define nh(a) = a.rr*float2(-.1063/.9278, .5)+a.gg*float2(-.3576/.9278, -.3576/.7874)+a.bb*float2(.5, -.0361/.7874);\n"
"#define ns(a) = a.rr*float2(-.1495/.886, .5)+a.gg*float2(-.2935/.886, -.2935/.701)+a.bb*float2(.5, -.057/.701);\n"
"#if Ml\n"
"#define sp(a, b, c) float3 a = tex2Dlod(s0, float4(tex+float2(b, c)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b, c) float3 a = tex2D(s0, tex+float2(b, c)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect even or odd coordinates for 4:2:0 sub-sampled chroma
	"float2 n = frac(tex*float2(Mw, Mh)*.5);"
	"n.x = (n.x > .5)? 1 : -1;"// even x positive, odd x negative
	"n.y = (n.y > .5)? 1 : -1;"// even y positive, odd y negative

	"sp(s1, 0, 0) sp(row, 0, n.y) sp(col, n.x, 0) sp(dia, n.x, n.y)"// original pixels
	"float2 row2, col2, dia2;"
	"if(Mw < 1120 && Mh < 630) {"// SD R'G'B' to Y'CbCr
		"s1 = s1.rrr*float3(.299, -.1495/.886, .5)+s1.ggg*float3(.587, -.2935/.886, -.2935/.701)+s1.bbb*float3(.114, .5, -.057/.701);"
		"row2 ns(row) col2 ns(col) dia2 ns(dia)}"
	"else {"// HD R'G'B' to Y'CbCr
		"s1 = s1.rrr*float3(.2126, -.1063/.9278, .5)+s1.ggg*float3(.7152, -.3576/.9278, -.3576/.7874)+s1.bbb*float3(.0722, .5, -.0361/.7874);"
		"row2 nh(row) col2 nh(col) dia2 nh(dia)}"
	"s1.yz = dia2*.0625+(col2+row2)*.1875+s1.yz*.5625;"// blur the chroma with the adjacent pixels

	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr+float3(0, -.202008/.587, 1.772)*s1.ggg+float3(1.402, -.419198/.587, 0)*s1.bbb;"// SD Y'CbCr to R'G'B'
	"else s1 = s1.rrr+float3(0, -.1674679/.894, 1.8556)*s1.ggg+float3(1.5748, -.4185031/.894, 0)*s1.bbb;"// HD Y'CbCr to R'G'B'

	"s1 = sign(s1)*pow(abs(s1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1 = mul(s1, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1 = s1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

static char const gk_szInitialPassShader27[] =
// 4:2:0 Perlin Smootherstep
"sampler s0 : register(s0);\n"
"#define nh(a) = a.rr*float2(-.1063/.9278, .5)+a.gg*float2(-.3576/.9278, -.3576/.7874)+a.bb*float2(.5, -.0361/.7874);\n"
"#define ns(a) = a.rr*float2(-.1495/.886, .5)+a.gg*float2(-.2935/.886, -.2935/.701)+a.bb*float2(.5, -.057/.701);\n"
"#if Ml\n"
"#define sp(a, b, c) float3 a = tex2Dlod(s0, float4(tex+float2(b, c)*2/float2(Mw, Mh), 0, 0)).rgb;\n"
"#else\n"
"#define sp(a, b, c) float3 a = tex2D(s0, tex+float2(b, c)*2/float2(Mw, Mh)).rgb;\n"
"#endif\n"

"float4 main(float2 tex : TEXCOORD0) : COLOR"
"{"
	// detect even or odd coordinates for 4:2:0 sub-sampled chroma
	"float2 n = frac(tex*float2(Mw, Mh)*.5);"
	"n.x = (n.x > .5)? 1 : -1;"// even x positive, odd x negative
	"n.y = (n.y > .5)? 1 : -1;"// even y positive, odd y negative

	"sp(s1, 0, 0) sp(row, 0, n.y) sp(col, n.x, 0) sp(dia, n.x, n.y)"// original pixels
	"float2 row2, col2, dia2;"
	"if(Mw < 1120 && Mh < 630) {"// SD R'G'B' to Y'CbCr
		"s1 = s1.rrr*float3(.299, -.1495/.886, .5)+s1.ggg*float3(.587, -.2935/.886, -.2935/.701)+s1.bbb*float3(.114, .5, -.057/.701);"
		"row2 ns(row) col2 ns(col) dia2 ns(dia)}"
	"else {"// HD R'G'B' to Y'CbCr
		"s1 = s1.rrr*float3(.2126, -.1063/.9278, .5)+s1.ggg*float3(.7152, -.3576/.9278, -.3576/.7874)+s1.bbb*float3(.0722, .5, -.0361/.7874);"
		"row2 nh(row) col2 nh(col) dia2 nh(dia)}"
	"s1.yz = dia2*.010715484619140625+(col2+row2)*.092800140380859375+s1.yz*.803684234619140625;"// blur the chroma with the adjacent pixels

	"if(Mw < 1120 && Mh < 630) s1 = s1.rrr+float3(0, -.202008/.587, 1.772)*s1.ggg+float3(1.402, -.419198/.587, 0)*s1.bbb;"// SD Y'CbCr to R'G'B'
	"else s1 = s1.rrr+float3(0, -.1674679/.894, 1.8556)*s1.ggg+float3(1.5748, -.4185031/.894, 0)*s1.bbb;"// HD Y'CbCr to R'G'B'

	"s1 = sign(s1)*pow(abs(s1), 2.4);"// to linear RGB, negative input compatible

	"float3x3 mat;"
	"if(Mw < 1120 && Mh < 630) {"
		"if(Mh == 288 || Mh == 576) mat = float3x3(.3953452542, .2038498967, .0185318088, .3136195517, .6488680379, .1189591403, .1637675413, .0655070165, .8625090509);"// PAL/SECAM RGB to XYZ
		"else mat = float3x3(.3613407125, .1950092734, .0172067006, .3353890797, .6437306529, .1027805244, .1760025550, .0794850248, .8800127750);}"// NTSC RGB to XYZ
	"else mat = float3x3(.3786675215, .1952504408, .0177500401, .3283428626, .6566857251, .1094476209, .1657219631, .0662887852, .8728023391);"// HD RGB to XYZ
	"s1 = mul(s1, mat);\n"// convert to XYZ
"#if Mr\n"
	"s1 = s1*32767/65535.+16384/65535.;\n"// convert to limited ranges
"#endif\n"
	"return s1.rgbb;"
"}";

// The list for resizers is offset by two; shaders 0 and 1 are never used
extern char const *const gk_aszResizerShader[13] = {gk_szResizerShader2, gk_szResizerShader3, gk_szResizerShader4, gk_szResizerShader5, gk_szResizerShader6, gk_szResizerShader7, gk_szResizerShader8, gk_szResizerShader9, gk_szResizerShader10, gk_szResizerShader11, gk_szResizerShader12, gk_szResizerShader13, gk_szResizerShader14},
	*const gk_aszBasicFrameInterpolationShader[4] = {gk_szBasicFrameInterpolationShader0, gk_szBasicFrameInterpolationShader1, gk_szBasicFrameInterpolationShader2, gk_szBasicFrameInterpolationShader3},
	*const gk_aszPreAdaptiveFrameInterpolationShader[3] = {gk_szPreAdaptiveFrameInterpolationShader0, gk_szPreAdaptiveFrameInterpolationShader1, gk_szPreAdaptiveFrameInterpolationShader2},
	*const gk_aszAdaptiveFrameInterpolationShader[4] = {gk_szAdaptiveFrameInterpolationShader0, gk_szAdaptiveFrameInterpolationShader1, gk_szAdaptiveFrameInterpolationShader2, gk_szAdaptiveFrameInterpolationShader3},
	*const gk_aszInitialPassShader[28] = {gk_szInitialPassShader0, gk_szInitialPassShader1, gk_szInitialPassShader2, gk_szInitialPassShader3, gk_szInitialPassShader4, gk_szInitialPassShader5, gk_szInitialPassShader6, gk_szInitialPassShader7, gk_szInitialPassShader8, gk_szInitialPassShader9, gk_szInitialPassShader10, gk_szInitialPassShader11, gk_szInitialPassShader12, gk_szInitialPassShader13, gk_szInitialPassShader14, gk_szInitialPassShader15, gk_szInitialPassShader16, gk_szInitialPassShader17, gk_szInitialPassShader18, gk_szInitialPassShader19, gk_szInitialPassShader20, gk_szInitialPassShader21, gk_szInitialPassShader22, gk_szInitialPassShader23, gk_szInitialPassShader24, gk_szInitialPassShader25, gk_szInitialPassShader26, gk_szInitialPassShader27};

// exclude the end 0 characters
extern unsigned __int32 const gk_u32LenHorizontalBlurShader = sizeof(gk_szHorizontalBlurShader)-1,
	gk_u32LenVerticalBlurShader = sizeof(gk_szVerticalBlurShader)-1,
	gk_au32LenResizerShader[13] = {sizeof(gk_szResizerShader2)-1, sizeof(gk_szResizerShader3)-1, sizeof(gk_szResizerShader4)-1, sizeof(gk_szResizerShader5)-1, sizeof(gk_szResizerShader6)-1, sizeof(gk_szResizerShader7)-1, sizeof(gk_szResizerShader8)-1, sizeof(gk_szResizerShader9)-1, sizeof(gk_szResizerShader10)-1, sizeof(gk_szResizerShader11)-1, sizeof(gk_szResizerShader12)-1, sizeof(gk_szResizerShader13)-1, sizeof(gk_szResizerShader14)-1},
	gk_u32LenFinalpassShader = sizeof(gk_szFinalpassShader)-1,
	gk_au32LenBasicFrameInterpolationShader[4] = {sizeof(gk_szBasicFrameInterpolationShader0)-1, sizeof(gk_szBasicFrameInterpolationShader1)-1, sizeof(gk_szBasicFrameInterpolationShader2)-1, sizeof(gk_szBasicFrameInterpolationShader3)-1},
	gk_au32LenPreAdaptiveFrameInterpolationShader[3] = {sizeof(gk_szPreAdaptiveFrameInterpolationShader0)-1, sizeof(gk_szPreAdaptiveFrameInterpolationShader1)-1, sizeof(gk_szPreAdaptiveFrameInterpolationShader2)-1},
	gk_au32LenAdaptiveFrameInterpolationShader[4] = {sizeof(gk_szAdaptiveFrameInterpolationShader0)-1, sizeof(gk_szAdaptiveFrameInterpolationShader1)-1, sizeof(gk_szAdaptiveFrameInterpolationShader2)-1, sizeof(gk_szAdaptiveFrameInterpolationShader3)-1},
	gk_u32LenSubtitlePassShader = sizeof(gk_szSubtitlePassShader)-1,
	gk_u32LenOSDPassShader = sizeof(gk_szOSDPassShader)-1,
	gk_u32LenInitialGammaShader = sizeof(gk_szInitialGammaShader)-1,
	gk_u32LenRGBconvYCCShader = sizeof(gk_szRGBconvYCCShader)-1,
	gk_au32LenInitialPassShader[28] = {sizeof(gk_szInitialPassShader0)-1, sizeof(gk_szInitialPassShader1)-1, sizeof(gk_szInitialPassShader2)-1, sizeof(gk_szInitialPassShader3)-1, sizeof(gk_szInitialPassShader4)-1, sizeof(gk_szInitialPassShader5)-1, sizeof(gk_szInitialPassShader6)-1, sizeof(gk_szInitialPassShader7)-1, sizeof(gk_szInitialPassShader8)-1, sizeof(gk_szInitialPassShader9)-1, sizeof(gk_szInitialPassShader10)-1, sizeof(gk_szInitialPassShader11)-1, sizeof(gk_szInitialPassShader12)-1, sizeof(gk_szInitialPassShader13)-1, sizeof(gk_szInitialPassShader14)-1, sizeof(gk_szInitialPassShader15)-1, sizeof(gk_szInitialPassShader16)-1, sizeof(gk_szInitialPassShader17)-1, sizeof(gk_szInitialPassShader18)-1, sizeof(gk_szInitialPassShader19)-1, sizeof(gk_szInitialPassShader20)-1, sizeof(gk_szInitialPassShader21)-1, sizeof(gk_szInitialPassShader22)-1, sizeof(gk_szInitialPassShader23)-1, sizeof(gk_szInitialPassShader24)-1, sizeof(gk_szInitialPassShader25)-1, sizeof(gk_szInitialPassShader26)-1, sizeof(gk_szInitialPassShader27)-1};
}

/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012 see Authors.txt
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
#include <inttypes.h>
#include "libmpeg2.h"
#include "attributes.h"
#include "../../../DSUtil/simd.h"

// Intel's SSE2 implementation of iDCT
// AP-945
// http://cache-www.intel.com/cd/00/00/01/76/17680_w_idct.pdf

static int const BITS_INV_ACC = 4;
static int const SHIFT_INV_ROW = 16 - BITS_INV_ACC;
static int const SHIFT_INV_COL = 1 + BITS_INV_ACC;
static int const RND_INV_ROW = 1024 * (6 - BITS_INV_ACC);
static int const RND_INV_COL = 16 * (BITS_INV_ACC - 3);
static int const RND_INV_CORR = RND_INV_COL - 1;

static __declspec(align(16)) __int16 const M128_round_inv_row[8] = {RND_INV_ROW, 0, RND_INV_ROW, 0, RND_INV_ROW, 0, RND_INV_ROW, 0};
static __declspec(align(16)) __int16 const M128_one_corr[8] = {1, 1, 1, 1, 1, 1, 1, 1};
static __declspec(align(16)) __int16 const M128_round_inv_col[8] = {RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL};
static __declspec(align(16)) __int16 const M128_round_inv_corr[8] = {RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR};
static __declspec(align(16)) __int16 const M128_tg_1_16[8] = {13036, 13036, 13036, 13036, 13036, 13036, 13036, 13036}; // tg *(2<<16)+ 0.5
static __declspec(align(16)) __int16 const M128_tg_2_16[8] = {27146, 27146, 27146, 27146, 27146, 27146, 27146, 27146}; // tg *(2<<16)+ 0.5
static __declspec(align(16)) __int16 const M128_tg_3_16[8] = { -21746, -21746, -21746, -21746, -21746, -21746, -21746, -21746}; // tg *(2<<16)+ 0.5
static __declspec(align(16)) __int16 const M128_cos_4_16[8] = { -19195, -19195, -19195, -19195, -19195, -19195, -19195, -19195}; // cos *(2<<16)+ 0.5

static __declspec(align(16)) __int16 const M128_tab_i_04[32] = {16384, 21407, 16384, 8867, 16384, -8867, 16384, -21407, 16384, 8867, -16384, -21407, -16384, 21407, 16384, -8867, 22725, 19266, 19266, -4520, 12873, -22725, 4520, -12873, 12873, 4520, -22725, -12873, 4520, 19266, 19266, -22725};
static __declspec(align(16)) __int16 const M128_tab_i_17[32] = {22725, 29692, 22725, 12299, 22725, -12299, 22725, -29692, 22725, 12299, -22725, -29692, -22725, 29692, 22725, -12299, 31521, 26722, 26722, -6270, 17855, -31521, 6270, -17855, 17855, 6270, -31521, -17855, 6270, 26722, 26722, -31521};
static __declspec(align(16)) __int16 const M128_tab_i_26[32] = {21407, 27969, 21407, 11585, 21407, -11585, 21407, -27969, 21407, 11585, -21407, -27969, -21407, 27969, 21407, -11585, 29692, 25172, 25172, -5906, 16819, -29692, 5906, -16819, 16819, 5906, -29692, -16819, 5906, 25172, 25172, -29692};
static __declspec(align(16)) __int16 const M128_tab_i_35[32] = {19266, 25172, 19266, 10426, 19266, -10426, 19266, -25172, 19266, 10426, -19266, -25172, -19266, 25172, 19266, -10426, 26722, 22654, 22654, -5315, 15137, -26722, 5315, -15137, 15137, 5315, -26722, -15137, 5315, 22654, 22654, -26722};

static __declspec(nothrow noalias) __forceinline void DCT_8_INV_ROW(uint8_t const* ecx, uint8_t const* esi, __m128i& xmm0, __m128i& xmm1, __m128i& xmm2, __m128i& xmm3, __m128i& xmm4, __m128i& xmm5, __m128i& xmm6, __m128i& xmm7)
{
    xmm0 = _mm_shufflelo_epi16(xmm0, 0xD8);
    xmm1 = _mm_shuffle_epi32(xmm0, 0);
    pmaddwd(xmm1, esi);
    xmm3 = _mm_shuffle_epi32(xmm0, 0x55);
    xmm0 = _mm_shufflehi_epi16(xmm0, 0xD8);
    pmaddwd(xmm3, esi + 32);
    xmm2 = _mm_shuffle_epi32(xmm0, 0xAA);
    xmm0 = _mm_shuffle_epi32(xmm0, 0xFF);
    pmaddwd(xmm2, esi + 16);
    xmm4 = _mm_shufflehi_epi16(xmm4, 0xD8);
    paddd(xmm1, M128_round_inv_row);
    xmm4 = _mm_shufflelo_epi16(xmm4, 0xD8);
    pmaddwd(xmm0, esi + 48);
    xmm5 = _mm_shuffle_epi32(xmm4, 0);
    xmm6 = _mm_shuffle_epi32(xmm4, 0xAA);
    pmaddwd(xmm5, ecx);
    paddd(xmm1, xmm2);
    movdqa(xmm2, xmm1);
    xmm7 = _mm_shuffle_epi32(xmm4, 0x55);
    pmaddwd(xmm6, ecx + 16);
    paddd(xmm0, xmm3);
    xmm4 = _mm_shuffle_epi32(xmm4, 0xFF);
    psubd(xmm2, xmm0);
    pmaddwd(xmm7, ecx + 32);
    paddd(xmm0, xmm1);
    psrad(xmm2, 12);
    paddd(xmm5, M128_round_inv_row);
    pmaddwd(xmm4, ecx + 48);
    paddd(xmm5, xmm6);
    movdqa(xmm6, xmm5);
    psrad(xmm0, 12);
    xmm2 = _mm_shuffle_epi32(xmm2, 0x1B);
    packssdw(xmm0, xmm2);
    paddd(xmm4, xmm7);
    psubd(xmm6, xmm4);
    paddd(xmm4, xmm5);
    psrad(xmm6, 12);
    psrad(xmm4, 12);
    xmm6 = _mm_shuffle_epi32(xmm6, 0x1B);
    packssdw(xmm4, xmm6);
}
static __declspec(nothrow noalias) __forceinline void DCT_8_INV_COL_8(__m128i& src0, __m128i& src1, __m128i& src2, __m128i& src3, __m128i& src4, __m128i& src5, __m128i& src6, __m128i& src7,
        __m128i& xmm0, __m128i& xmm1, __m128i& xmm2, __m128i& xmm3, __m128i& xmm4, __m128i& xmm5, __m128i& xmm6, __m128i& xmm7)
{
    movdqa(xmm1,  M128_tg_3_16);
    movdqa(xmm2, xmm0);
    movdqa(xmm3,  src3);
    pmulhw(xmm0, xmm1);
    pmulhw(xmm1, xmm3);
    movdqa(xmm5,  M128_tg_1_16);
    movdqa(xmm6, xmm4);
    pmulhw(xmm4, xmm5);
    paddsw(xmm0, xmm2);
    pmulhw(xmm5, src1);
    paddsw(xmm1, xmm3);
    movdqa(xmm7,  src6);
    paddsw(xmm0, xmm3);
    movdqa(xmm3,  M128_tg_2_16);
    psubsw(xmm2, xmm1);
    pmulhw(xmm7, xmm3);
    movdqa(xmm1, xmm0);
    pmulhw(xmm3, src2);
    psubsw(xmm5, xmm6);
    paddsw(xmm4, src1);
    paddsw(xmm0, xmm4);
    paddsw(xmm0,  M128_one_corr);
    psubsw(xmm4, xmm1);
    movdqa(xmm6, xmm5);
    psubsw(xmm5, xmm2);
    paddsw(xmm5,  M128_one_corr);
    paddsw(xmm6, xmm2);
    movdqa(src7, xmm0);
    movdqa(xmm1, xmm4);
    movdqa(xmm0,  M128_cos_4_16);
    paddsw(xmm4, xmm5);
    movdqa(xmm2,  M128_cos_4_16);
    pmulhw(xmm2, xmm4);
    movdqa(src3, xmm6);
    psubsw(xmm1, xmm5);
    paddsw(xmm7, src2);
    psubsw(xmm3, src6);
    movdqa(xmm6, src0);
    pmulhw(xmm0, xmm1);
    movdqa(xmm5, src4);
    paddsw(xmm5, xmm6);
    psubsw(xmm6, src4);
    paddsw(xmm4, xmm2);
    por(xmm4,  M128_one_corr);
    paddsw(xmm0, xmm1);
    por(xmm0,  M128_one_corr);
    movdqa(xmm2, xmm5);
    paddsw(xmm5, xmm7);
    movdqa(xmm1, xmm6);
    paddsw(xmm5,  M128_round_inv_col);
    psubsw(xmm2, xmm7);
    movdqa(xmm7, src7);
    paddsw(xmm6, xmm3);
    paddsw(xmm6,  M128_round_inv_col);
    paddsw(xmm7, xmm5);
    psraw(xmm7, SHIFT_INV_COL);
    psubsw(xmm1, xmm3);
    paddsw(xmm1,  M128_round_inv_corr);
    movdqa(xmm3, xmm6);
    paddsw(xmm2,  M128_round_inv_corr);
    paddsw(xmm6, xmm4);
    movdqa(src0, xmm7);
    psraw(xmm6, SHIFT_INV_COL);
    movdqa(xmm7, xmm1);
    paddsw(xmm1, xmm0);
    movdqa(src1, xmm6);
    psraw(xmm1, SHIFT_INV_COL);
    movdqa(xmm6, src3);
    psubsw(xmm7, xmm0);
    psraw(xmm7, SHIFT_INV_COL);
    movdqa(src2, xmm1);
    psubsw(xmm5, src7);
    psraw(xmm5, SHIFT_INV_COL);
    movdqa(src7, xmm5);
    psubsw(xmm3, xmm4);
    paddsw(xmm6, xmm2);
    psubsw(xmm2, src3);
    psraw(xmm6, SHIFT_INV_COL);
    psraw(xmm2, SHIFT_INV_COL);
    movdqa(src3, xmm6);
    psraw(xmm3, SHIFT_INV_COL);
    movdqa(src4, xmm2);
    movdqa(src5, xmm7);
    movdqa(src6, xmm3);
}

static __declspec(nothrow noalias) __forceinline void idct_M128ASM(__m128i& src0, __m128i& src1, __m128i& src2, __m128i& src3, __m128i& src4, __m128i& src5, __m128i& src6, __m128i& src7)
{
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    movdqa(xmm0, src0);
    uint8_t const* esi = reinterpret_cast<uint8_t const*>(M128_tab_i_04);
    movdqa(xmm4, src2);
    uint8_t const* ecx = reinterpret_cast<uint8_t const*>(M128_tab_i_26);
    DCT_8_INV_ROW(ecx, esi, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    movdqa(src0, xmm0);
    movdqa(src2, xmm4);

    movdqa(xmm0, src4);
    movdqa(xmm4, src6);
    DCT_8_INV_ROW(ecx, esi, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    movdqa(src4, xmm0);
    movdqa(src6, xmm4);

    movdqa(xmm0, src3);
    esi = reinterpret_cast<uint8_t const*>(M128_tab_i_35);
    movdqa(xmm4, src1);
    ecx = reinterpret_cast<uint8_t const*>(M128_tab_i_17);
    DCT_8_INV_ROW(ecx, esi, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    movdqa(src3, xmm0);
    movdqa(src1, xmm4);

    movdqa(xmm0, src5);
    movdqa(xmm4, src7);
    DCT_8_INV_ROW(ecx, esi, xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    DCT_8_INV_COL_8(src0, src1, src2, src3, src4, src5, src6, src7,
                    xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
}

__declspec(nothrow noalias) void mpeg2_idct_copy_sse2(int16_t* block, uint8_t* dest, ptrdiff_t stride)
{
    __m128i& src0 = *reinterpret_cast<__m128i*>(block + 0 * 16 / 2);
    __m128i& src1 = *reinterpret_cast<__m128i*>(block + 1 * 16 / 2);
    __m128i& src2 = *reinterpret_cast<__m128i*>(block + 2 * 16 / 2);
    __m128i& src3 = *reinterpret_cast<__m128i*>(block + 3 * 16 / 2);
    __m128i& src4 = *reinterpret_cast<__m128i*>(block + 4 * 16 / 2);
    __m128i& src5 = *reinterpret_cast<__m128i*>(block + 5 * 16 / 2);
    __m128i& src6 = *reinterpret_cast<__m128i*>(block + 6 * 16 / 2);
    __m128i& src7 = *reinterpret_cast<__m128i*>(block + 7 * 16 / 2);
    idct_M128ASM(src0, src1, src2, src3, src4, src5, src6, src7);

    __m128i r0 = _mm_packus_epi16(src0, src1);
    __m128i r1 = _mm_packus_epi16(src2, src3);
    __m128i r2 = _mm_packus_epi16(src4, src5);
    __m128i r3 = _mm_packus_epi16(src6, src7);

    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 0 * stride), _mm_castsi128_ps(r0));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 1 * stride), _mm_castsi128_ps(r0));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 2 * stride), _mm_castsi128_ps(r1));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 3 * stride), _mm_castsi128_ps(r1));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 4 * stride), _mm_castsi128_ps(r2));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 5 * stride), _mm_castsi128_ps(r2));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 6 * stride), _mm_castsi128_ps(r3));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 7 * stride), _mm_castsi128_ps(r3));

    __m128i zero = _mm_setzero_si128();
    src0 = zero;
    src1 = zero;
    src2 = zero;
    src3 = zero;
    src4 = zero;
    src5 = zero;
    src6 = zero;
    src7 = zero;
}

__declspec(nothrow noalias) void mpeg2_idct_add_sse2(size_t last, int16_t* block, uint8_t* dest, ptrdiff_t stride)
{
    __m128i& src0 = *reinterpret_cast<__m128i*>(block + 0 * 16 / 2);
    __m128i& src1 = *reinterpret_cast<__m128i*>(block + 1 * 16 / 2);
    __m128i& src2 = *reinterpret_cast<__m128i*>(block + 2 * 16 / 2);
    __m128i& src3 = *reinterpret_cast<__m128i*>(block + 3 * 16 / 2);
    __m128i& src4 = *reinterpret_cast<__m128i*>(block + 4 * 16 / 2);
    __m128i& src5 = *reinterpret_cast<__m128i*>(block + 5 * 16 / 2);
    __m128i& src6 = *reinterpret_cast<__m128i*>(block + 6 * 16 / 2);
    __m128i& src7 = *reinterpret_cast<__m128i*>(block + 7 * 16 / 2);
    idct_M128ASM(src0, src1, src2, src3, src4, src5, src6, src7);

    __m128i zero = _mm_setzero_si128();
    __m128i r0 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[0 * stride])), zero);
    __m128i r1 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[1 * stride])), zero);
    __m128i r2 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[2 * stride])), zero);
    __m128i r3 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[3 * stride])), zero);
    __m128i r4 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[4 * stride])), zero);
    __m128i r5 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[5 * stride])), zero);
    __m128i r6 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[6 * stride])), zero);
    __m128i r7 = _mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<__m128i*>(&dest[7 * stride])), zero);

    r0 = _mm_adds_epi16(r0, src0);
    r1 = _mm_adds_epi16(r1, src1);
    r2 = _mm_adds_epi16(r2, src2);
    r3 = _mm_adds_epi16(r3, src3);
    r4 = _mm_adds_epi16(r4, src4);
    r5 = _mm_adds_epi16(r5, src5);
    r6 = _mm_adds_epi16(r6, src6);
    r7 = _mm_adds_epi16(r7, src7);

    r0 = _mm_packus_epi16(r0, r1);
    r2 = _mm_packus_epi16(r2, r3);
    r4 = _mm_packus_epi16(r4, r5);
    r6 = _mm_packus_epi16(r6, r7);

    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 0 * stride), _mm_castsi128_ps(r0));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 1 * stride), _mm_castsi128_ps(r0));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 2 * stride), _mm_castsi128_ps(r2));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 3 * stride), _mm_castsi128_ps(r2));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 4 * stride), _mm_castsi128_ps(r4));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 5 * stride), _mm_castsi128_ps(r4));
    _mm_storel_pi(reinterpret_cast<__m64*>(dest + 6 * stride), _mm_castsi128_ps(r6));
    _mm_storeh_pi(reinterpret_cast<__m64*>(dest + 7 * stride), _mm_castsi128_ps(r6));

    src0 = zero;
    src1 = zero;
    src2 = zero;
    src3 = zero;
    src4 = zero;
    src5 = zero;
    src6 = zero;
    src7 = zero;
}

__declspec(nothrow noalias) void mpeg2_idct_init_sse2()
{
}
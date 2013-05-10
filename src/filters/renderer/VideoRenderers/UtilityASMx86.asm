; (C) 2012-2013 see Authors.txt
;
; This file is part of MPC-HC.
;
; MPC-HC is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 3 of the License, or
; (at your option) any later version.
;
; MPC-HC is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;

extern _gk_aDitherMatrix

segment .text
	global $@MoveDitherMatrix@8, $@MoveDitherMatrixAVX@8

$@MoveDitherMatrix@8:
; __fastcall function arguments:
; ecx: pointer to the temporary texture
; edx: pointer to either afQ10bit (1/1023.) or afQ8bit (1/255.) 16-byte aligned set of four packed singles
; no return value
; this function copies the constant image of the dither matrix to a DirectX surface of the same size, and applies either 10- or 8-bit quantization
	; no calls made, so no callee stack created and no registers have to be preserved
	; initialize some registers used in the loop
	movaps xmm0, [edx]; load quantization set
	lea edx, [_gk_aDitherMatrix]; pointer to dithermap
	mov eax, 585; decrementing counter
	jmp SkipFirstInDitherMatrixLoop

	align 64
DitherMatrixLoop:; handles 4095 (585*7) out the total of 4096 vectors of 4 singles
	add edx, byte 112
	add ecx, byte 112
SkipFirstInDitherMatrixLoop:
	add eax, byte -1; the traditional dec creates a false dependency on previous contents of flags, add and sub do not
	; load quantization constant
	movaps xmm1, xmm0
	movaps xmm2, xmm0
	movaps xmm3, xmm0
	movaps xmm4, xmm0
	movaps xmm5, xmm0
	movaps xmm6, xmm0
	movaps xmm7, xmm0
	; load from the dithermap and apply quantization
	mulps xmm1, [edx]
	mulps xmm2, [edx+16]
	mulps xmm3, [edx+32]
	mulps xmm4, [edx+48]
	mulps xmm5, [edx+64]
	mulps xmm6, [edx+80]
	mulps xmm7, [edx+96]
	; store to the temporary texture
	movaps [ecx], xmm1
	movaps [ecx+16], xmm2
	movaps [ecx+32], xmm3
	movaps [ecx+48], xmm4
	movaps [ecx+64], xmm5
	movaps [ecx+80], xmm6
	movaps [ecx+96], xmm7
	jnz DitherMatrixLoop

	; multiply and store the last vector of 4 singles
	mulps xmm0, [edx+112]
	movaps [ecx+112], xmm0
	ret

$@MoveDitherMatrixAVX@8:
; __fastcall function arguments:
; ecx: pointer to the temporary texture
; edx: pointer to either afQ10bit (1/1023.) or afQ8bit (1/255.) 16-byte aligned set of four packed singles
; no return value
; this function copies the constant image of the dither matrix to a DirectX surface of the same size, and applies either 10- or 8-bit quantization
	; no calls made, so no callee stack created and no registers have to be preserved
	; initialize some registers used in the loop
	vbroadcastf128 ymm0, [edx]; load 128-bit quantization set to both halves of the register
	lea edx, [_gk_aDitherMatrix+0x80]; pointer to dithermap, with an offset to facilitate byte offset values within the loop
	sub ecx, byte -0x80; same offset as previous line
	mov eax, 292; decrementing counter
	jmp SkipFirstInDitherMatrixAVXLoop

	align 64
DitherMatrixAVXLoop:; handles 2044 (292*7) out the total of 2048 vectors of 8 singles
	add edx, 224
	add ecx, 224
SkipFirstInDitherMatrixAVXLoop:
	add eax, byte -1; the traditional dec creates a false dependency on previous contents of flags, add and sub do not
	; load from the dithermap and apply quantization
	vmulps ymm1, ymm0, [edx-0x80]
	vmulps ymm2, ymm0, [edx-0x60]
	vmulps ymm3, ymm0, [edx-0x40]
	vmulps ymm4, ymm0, [edx-0x20]
	vmulps ymm5, ymm0, [edx]
	vmulps ymm6, ymm0, [edx+0x20]
	vmulps ymm7, ymm0, [edx+0x40]
	; store to the temporary texture
	vmovaps [ecx-0x80], ymm1
	vmovaps [ecx-0x60], ymm2
	vmovaps [ecx-0x40], ymm3
	vmovaps [ecx-0x20], ymm4
	vmovaps [ecx], ymm5
	vmovaps [ecx+0x20], ymm6
	vmovaps [ecx+0x40], ymm7
	jnz DitherMatrixAVXLoop

	; multiply and store the last 4 vectors of 8 singles
	vmulps ymm1, ymm0, [edx+0x60]
	vmulps ymm2, ymm0, [edx+0x80]
	vmulps ymm3, ymm0, [edx+0xA0]
	vmulps ymm0, [edx+0xC0]
	vmovaps [ecx+0x60], ymm1
	vmovaps [ecx+0x80], ymm2
	vmovaps [ecx+0xA0], ymm3
	vmovaps [ecx+0xC0], ymm0

	vzeroall; eliminate any possible AVX to SSE transition penalty
	ret
end
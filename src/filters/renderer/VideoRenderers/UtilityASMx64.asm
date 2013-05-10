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

extern QueryPerformanceCounter:proc, QueryPerformanceFrequency:proc, gk_aDitherMatrix

segment .text
	global PerfCounter100ns, MoveDitherMatrix, MoveDitherMatrixAVX

PerfCounter100ns:
; no function arguments
; return value in rax
; this function outputs the current time based on QueryPerformanceCounter, expressed in 100 ns units
	lea rcx, [rsp+0x8]; rcx shadow space
	; only minimum size callee stack reserved: interval [rsp, rsp+0x20), 8 bytes padding to keep 16-byte alignment interval [rsp+0x20, rsp+0x28)
	sub rsp, byte 0x28

	call QueryPerformanceFrequency
	lea rcx, [rsp+0x10+0x28]; rdx shadow space
	call QueryPerformanceCounter
	mov eax, 10000000; zero-extended to rax
	mul qword [rsp+0x10+0x28]; output 128-bit value to rdx:rax
	div qword [rsp+0x8+0x28]; divide rdx:rax

	add rsp, byte 0x28
	ret

MoveDitherMatrix:
; function arguments:
; rcx: pointer to the temporary texture
; rdx: pointer to either afQ10bit (1/1023.) or afQ8bit (1/255.) 16-byte aligned set of four packed singles
; no return value
; this function copies the constant image of the dither matrix to a DirectX surface of the same size, and applies either 10- or 8-bit quantization
	; no calls made, so no callee stack created, there's only space reserved for preserving registers
	sub rsp, 0x88

	; initialize some registers used in the loop
	movaps xmm0, [rdx]; load quantization set
	lea rax, [gk_aDitherMatrix+0x80]; pointer to dithermap, with an offset to facilitate byte offset values within the loop
	sub rcx, byte -0x80; same offset as previous line
	mov edx, 273; decrementing counter

	; preserve xmm6 to xmm15, as required by the calling convention
	movaps [rsp-0x88+0x88], xmm6
	movaps [rsp-0x78+0x88], xmm7
	movaps [rsp-0x68+0x88], xmm8
	movaps [rsp-0x58+0x88], xmm9
	movaps [rsp-0x48+0x88], xmm10
	movaps [rsp-0x38+0x88], xmm11
	movaps [rsp-0x28+0x88], xmm12
	movaps [rsp-0x18+0x88], xmm13
	; return address in interval [rsp+0x88, rsp+0x8+0x88), interval [rsp-0x8+0x88, rsp+0x88) unused because of 16-byte alignment requirements
	movaps [rsp+0x8+0x88], xmm14; rcx:rdx shadow space
	movaps [rsp+0x18+0x88], xmm15; r8:r9 shadow space
	jmp SkipFirstInDitherMatrixLoop

	align 64
DitherMatrixLoop:; handles 4095 (273*15) out the total of 4096 vectors of 4 singles
	add rax, 240
	add rcx, 240
SkipFirstInDitherMatrixLoop:
	add edx, byte -1; the traditional dec creates a false dependency on previous contents of flags, add and sub do not
	; load quantization constant
	movaps xmm1, xmm0
	movaps xmm2, xmm0
	movaps xmm3, xmm0
	movaps xmm4, xmm0
	movaps xmm5, xmm0
	movaps xmm6, xmm0
	movaps xmm7, xmm0
	movaps xmm8, xmm0
	movaps xmm9, xmm0
	movaps xmm10, xmm0
	movaps xmm11, xmm0
	movaps xmm12, xmm0
	movaps xmm13, xmm0
	movaps xmm14, xmm0
	movaps xmm15, xmm0
	; load from the dithermap and apply quantization
	mulps xmm1, [rax-0x80]
	mulps xmm2, [rax-0x70]
	mulps xmm3, [rax-0x60]
	mulps xmm4, [rax-0x50]
	mulps xmm5, [rax-0x40]
	mulps xmm6, [rax-0x30]
	mulps xmm7, [rax-0x20]
	mulps xmm8, [rax-0x10]
	mulps xmm9, [rax]
	mulps xmm10, [rax+0x10]
	mulps xmm11, [rax+0x20]
	mulps xmm12, [rax+0x30]
	mulps xmm13, [rax+0x40]
	mulps xmm14, [rax+0x50]
	mulps xmm15, [rax+0x60]
	; store to the temporary texture
	movaps [rcx-0x80], xmm1
	movaps [rcx-0x70], xmm2
	movaps [rcx-0x60], xmm3
	movaps [rcx-0x50], xmm4
	movaps [rcx-0x40], xmm5
	movaps [rcx-0x30], xmm6
	movaps [rcx-0x20], xmm7
	movaps [rcx-0x10], xmm8
	movaps [rcx], xmm9
	movaps [rcx+0x10], xmm10
	movaps [rcx+0x20], xmm11
	movaps [rcx+0x30], xmm12
	movaps [rcx+0x40], xmm13
	movaps [rcx+0x50], xmm14
	movaps [rcx+0x60], xmm15
	jnz DitherMatrixLoop

	mulps xmm0, [rax+0x70]; multiply the last vector of 4 singles

	; restore xmm6 to xmm15
	movaps xmm6, [rsp-0x88+0x88]
	movaps xmm7, [rsp-0x78+0x88]
	movaps xmm8, [rsp-0x68+0x88]
	movaps xmm9, [rsp-0x58+0x88]
	movaps xmm10, [rsp-0x48+0x88]
	movaps xmm11, [rsp-0x38+0x88]
	movaps xmm12, [rsp-0x28+0x88]
	movaps xmm13, [rsp-0x18+0x88]
	movaps xmm14, [rsp+0x8+0x88]
	movaps xmm15, [rsp+0x18+0x88]

	movaps [rcx+0x70], xmm0; store the last vector of 4 singles

	add rsp, 0x88
	ret

MoveDitherMatrixAVX:
; function arguments:
; rcx: pointer to the temporary texture
; rdx: pointer to either afQ10bit (1/1023.) or afQ8bit (1/255.) 16-byte aligned set of four packed singles
; no return value
; this function copies the constant image of the dither matrix to a DirectX surface of the same size, and applies either 10- or 8-bit quantization
	; no calls made, so no callee stack created, there's only space reserved for preserving registers
	sub rsp, 0x88

	; initialize some registers used in the loop
	vbroadcastf128 ymm0, [rdx]; load 128-bit quantization set to both halves of the register
	lea rax, [gk_aDitherMatrix+0x80]; pointer to dithermap, with an offset to facilitate byte offset values within the loop
	sub rcx, byte -0x80; same offset as previous line
	mov edx, 136; decrementing counter

	; preserve xmm6 to xmm15, as required by the calling convention
	vmovaps [rsp-0x88+0x88], xmm6
	vmovaps [rsp-0x78+0x88], xmm7
	vmovaps [rsp-0x68+0x88], xmm8
	vmovaps [rsp-0x58+0x88], xmm9
	vmovaps [rsp-0x48+0x88], xmm10
	vmovaps [rsp-0x38+0x88], xmm11
	vmovaps [rsp-0x28+0x88], xmm12
	vmovaps [rsp-0x18+0x88], xmm13
	; return address in interval [rsp+0x88, rsp+0x8+0x88), interval [rsp-0x8+0x88, rsp+0x88) unused because of 16-byte alignment requirements
	vmovaps [rsp+0x8+0x88], xmm14; rcx:rdx shadow space
	vmovaps [rsp+0x18+0x88], xmm15; r8:r9 shadow space
	jmp SkipFirstInDitherMatrixAVXLoop

	align 64
DitherMatrixAVXLoop:; handles 2040 (136*15) out the total of 2048 vectors of 8 singles
	add rax, 480
	add rcx, 480
SkipFirstInDitherMatrixAVXLoop:
	add edx, byte -1; the traditional dec creates a false dependency on previous contents of flags, add and sub do not
	; load from the dithermap and apply quantization
	vmulps ymm1, ymm0, [rax-0x80]
	vmulps ymm2, ymm0, [rax-0x60]
	vmulps ymm3, ymm0, [rax-0x40]
	vmulps ymm4, ymm0, [rax-0x20]
	vmulps ymm5, ymm0, [rax]
	vmulps ymm6, ymm0, [rax+0x20]
	vmulps ymm7, ymm0, [rax+0x40]
	vmulps ymm8, ymm0, [rax+0x60]
	vmulps ymm9, ymm0, [rax+0x80]
	vmulps ymm10, ymm0, [rax+0xA0]
	vmulps ymm11, ymm0, [rax+0xC0]
	vmulps ymm12, ymm0, [rax+0xE0]
	vmulps ymm13, ymm0, [rax+0x100]
	vmulps ymm14, ymm0, [rax+0x120]
	vmulps ymm15, ymm0, [rax+0x140]
	; store to the temporary texture
	vmovaps [rcx-0x80], ymm1
	vmovaps [rcx-0x60], ymm2
	vmovaps [rcx-0x40], ymm3
	vmovaps [rcx-0x20], ymm4
	vmovaps [rcx], ymm5
	vmovaps [rcx+0x20], ymm6
	vmovaps [rcx+0x40], ymm7
	vmovaps [rcx+0x60], ymm8
	vmovaps [rcx+0x80], ymm9
	vmovaps [rcx+0xA0], ymm10
	vmovaps [rcx+0xC0], ymm11
	vmovaps [rcx+0xE0], ymm12
	vmovaps [rcx+0x100], ymm13
	vmovaps [rcx+0x120], ymm14
	vmovaps [rcx+0x140], ymm15
	jnz DitherMatrixAVXLoop

	; multiply and store the last 8 vectors of 8 singles
	vmulps ymm1, ymm0, [rax+0x160]
	vmulps ymm2, ymm0, [rax+0x180]
	vmulps ymm3, ymm0, [rax+0x1A0]
	vmulps ymm4, ymm0, [rax+0x1C0]
	vmulps ymm5, ymm0, [rax+0x1E0]
	vmulps ymm6, ymm0, [rax+0x200]
	vmulps ymm7, ymm0, [rax+0x220]
	vmulps ymm0, [rax+0x240]
	vmovaps [rcx+0x160], ymm1
	vmovaps [rcx+0x180], ymm2
	vmovaps [rcx+0x1A0], ymm3
	vmovaps [rcx+0x1C0], ymm4
	vmovaps [rcx+0x1E0], ymm5
	vmovaps [rcx+0x200], ymm6
	vmovaps [rcx+0x220], ymm7
	vmovaps [rcx+0x240], ymm0

	vzeroall; eliminate any possible AVX to SSE transition penalty

	; restore xmm6 to xmm15
	vmovaps xmm6, [rsp-0x88+0x88]
	vmovaps xmm7, [rsp-0x78+0x88]
	vmovaps xmm8, [rsp-0x68+0x88]
	vmovaps xmm9, [rsp-0x58+0x88]
	vmovaps xmm10, [rsp-0x48+0x88]
	vmovaps xmm11, [rsp-0x38+0x88]
	vmovaps xmm12, [rsp-0x28+0x88]
	vmovaps xmm13, [rsp-0x18+0x88]
	vmovaps xmm14, [rsp+0x8+0x88]
	vmovaps xmm15, [rsp+0x18+0x88]

	add rsp, 0x88
	ret
end
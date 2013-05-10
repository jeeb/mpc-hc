/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
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
#include "RenderersSettings.h"
#include "../../../mpc-hc/mplayerc.h"
#include <intrin.h>

__declspec(nothrow noalias) void CRenderersSettings::UpdateData(bool bSave)
{
    reinterpret_cast<CAppSettings*>(reinterpret_cast<uintptr_t>(this) - offsetof(CAppSettings, m_RenderersSettings))->UpdateRenderersData(bSave);// inverse relationship with CAppSettings
}

// always make sure that SetDefault() and SetOptimal() set up all values in the class
__declspec(nothrow noalias) void CRenderersSettings::SetDefault()
{
    bD3DFullscreen = 0;
    iVMR9HighColorResolution = 0;
    iVMR9DisableInitialColorMixing = 0;
    iVMR9ChromaFix = 1;
    iVMR9SurfacesQuality = 0;
    iVMRDisableDesktopComposition = 0;
    iEVRAlternativeScheduler = 0;
    iEVREnableFrameTimeCorrection = 0;

    fVMR9AlterativeVSync = 0;
    iVMR9VSyncOffset = 0;

    iVMRFlushGPUBeforeVSync = 0;
    iVMRFlushGPUWait = 0;

    iVMR9ColorManagementEnable = 0;
    iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DIM;
    iVMR9ColorManagementIntent = 3;
    iVMR9ColorManagementWpAdaptState = WPADAPT_STATE_NONE;
    iVMR9ColorManagementLookupQuality = 128;
    iVMR9ColorManagementBPC = 1;

    iVMR9DitheringLevels = 0;
    iVMR9DitheringTestEnable = 0;

    iVMR9FrameInterpolation = 0;
    dRefreshRateAdjust = 1.0;

    bSynchronizeVideo = 0;
    bSynchronizeDisplay = 0;
    bSynchronizeNearest = 1;
    iLineDelta = 0;
    iColumnDelta = 0;
    fCycleDelta = 0.0012;
    fTargetSyncOffset = 12.0;
    fControlLimit = 2.0;
}

__declspec(nothrow noalias) void CRenderersSettings::SetOptimal()
{
    bD3DFullscreen = 0;
    iVMR9HighColorResolution = 0;
    iVMR9DisableInitialColorMixing = 0;
    iVMR9ChromaFix = 10;
    iVMR9SurfacesQuality = 3;
    iVMRDisableDesktopComposition = 0;
    iEVRAlternativeScheduler = 0;
    iEVREnableFrameTimeCorrection = 0;

    fVMR9AlterativeVSync = 0;
    iVMR9VSyncOffset = 0;

    iVMRFlushGPUBeforeVSync = 0;
    iVMRFlushGPUWait = 0;

    iVMR9ColorManagementEnable = 0;
    iVMR9ColorManagementAmbientLight = AMBIENT_LIGHT_DIM;
    iVMR9ColorManagementIntent = 3;
    iVMR9ColorManagementWpAdaptState = WPADAPT_STATE_NONE;
    iVMR9ColorManagementLookupQuality = 256;
    iVMR9ColorManagementBPC = 1;

    iVMR9DitheringLevels = 1;
    iVMR9DitheringTestEnable = 0;

    iVMR9FrameInterpolation = 0;
    dRefreshRateAdjust = 1.0;

    bSynchronizeVideo = 0;
    bSynchronizeDisplay = 0;
    bSynchronizeNearest = 1;
    iLineDelta = 0;
    iColumnDelta = 0;
    fCycleDelta = 0.0012;
    fTargetSyncOffset = 12.0;
    fControlLimit = 2.0;
}

#ifndef _M_X64// external asm file for x64
extern "C" __declspec(nothrow noalias) __int64 PerfCounter100ns()
{
    ULARGE_INTEGER uc, uf;
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&uf));
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&uc));
    // TODO: replace this item like I did for the x64 version, this really isn't efficient
    // note that unlike the LARGE_INTEGER parameter used by the above functions, these values are actually unsigned, that makes it a lot easier)

    // compute 10000000*uc/uf, somewhat specialized from llMulDiv, only usable for 32-bit integer registers
    // do long multiplication
    ULARGE_INTEGER p0;
    p0.QuadPart = __emulu(10000000, uc.LowPart);

    /* This next computation cannot overflow into p1.HighPart because the max number we can compute here is:

                 (2 ** 32 - 1) * (2 ** 32 - 1) +    // ua.LowPart * uc.LowPart
    (2 ** 32) * (2 ** 31) * (2 ** 32 - 1) * 2       // x.LowPart * y.HighPart * 2

    == 2 ** 96 - 2 ** 64 + (2 ** 64 - 2 ** 33 + 1)
    == 2 ** 96 - 2 ** 33 + 1
    < 2 ** 96
    */

    ULARGE_INTEGER x;
    x.QuadPart = __emulu(10000000, uc.HighPart) + static_cast<unsigned __int64>(p0.HighPart);
    p0.HighPart = x.LowPart;

    // do the division
    /* compiler uses an external call to __aulldiv for this case: not worth the trouble
    // if the dividend is 64 bit or smaller, use the compiler
    //if (!x.HighPart) return p0.QuadPart/uf.QuadPart;

    compiler uses an external call to __aulldvrm and __aulldiv for this case: really not worth the trouble
    // if the divisor is 32 bit then its simpler
    if (!uf.HighPart) {
        ULARGE_INTEGER uliDividend;
        ULARGE_INTEGER uliResult;
        DWORD dwDivisor = uf.LowPart;

        ASSERT(x.HighPart < dwDivisor);
        uliDividend.HighPart = x.HighPart;
        uliDividend.LowPart = p0.HighPart;

        uliResult.HighPart = static_cast<unsigned __int32>(uliDividend.QuadPart / dwDivisor);
        p0.HighPart = static_cast<unsigned __int32>(uliDividend.QuadPart % dwDivisor);
        uliResult.LowPart = 0;
        uliResult.QuadPart = p0.QuadPart / dwDivisor + uliResult.QuadPart;

        return uliResult.QuadPart;
    }*/

    ULARGE_INTEGER uliResult, p1;
    p1.QuadPart = x.HighPart;
    uliResult.QuadPart = 0;

    // OK - do long division
    unsigned __int32 i = 0x80000000;// most significant bit only
    do {
        // shift 128 bit p left by 1
        p1.QuadPart <<= 1;
        p1.QuadPart |= p0.QuadPart >> 63;
        p0.QuadPart <<= 1;

        // compare
        if (uf.QuadPart <= p1.QuadPart) {
            p1.QuadPart -= uf.QuadPart;
            uliResult.HighPart |= i;
        }
    } while (i >>= 1); // fill the 32 high bits, one at a time
    i = 0x80000000;// most significant bit only
    do {
        // shift 128 bit p left by 1
        p1.QuadPart = p1.QuadPart << 1 | p0.QuadPart >> 63;
        p0.QuadPart <<= 1;

        // compare
        if (uf.QuadPart <= p1.QuadPart) {
            p1.QuadPart -= uf.QuadPart;
            uliResult.LowPart |= i;
        }
    } while (i >>= 1); // fill the 32 low bits, one at a time

    return uliResult.QuadPart;
}
#endif

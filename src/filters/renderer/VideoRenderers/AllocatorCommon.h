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

#pragma once

#include <d3dx9.h>

#pragma pack(push, 4)// this directive is used to copy 4-byte packed data to video memory on x86 and x64
// important: do not add __declspec(align(?)) on the declaration of these structs, only on the array of which these are used in
struct CUSTOMVERTEX_COLOR {
    float x, y, z, rhw;
    __int32 Diffuse;
};
struct CUSTOMVERTEX_TEX1 {
    float x, y, z, rhw, u, v;
};
#pragma pack(pop)
static_assert(20 == sizeof(CUSTOMVERTEX_COLOR), "struct packing failure");
static_assert(24 == sizeof(CUSTOMVERTEX_TEX1), "struct packing failure");

extern __declspec(nothrow noalias) double RoundCommonRates(double r);

extern __declspec(nothrow noalias) CString GetWindowsErrorMessage(HRESULT _Error, HINSTANCE _Module);

extern GUID const GUID_SURFACE_INDEX;

extern CCritSec g_ffdshowReceive;
extern bool queue_ffdshow_support;
// Support ffdshow queuing.
// This interface is used to check version of Media Player Classic.
// {A273C7F6-25D4-46b0-B2C8-4F7FADC44E37}
DEFINE_GUID(IID_IVMRffdshow9,
            0xA273C7F6, 0x25D4, 0x46B0, 0xB2, 0xC8, 0x4F, 0x7F, 0xAD, 0xC4, 0x4E, 0x37);

interface __declspec(uuid("A273C7F6-25D4-46b0-B2C8-4F7FADC44E37") novtable)
IVMRffdshow9 :
IUnknown {
    virtual __declspec(nothrow noalias) STDMETHODIMP support_ffdshow() {// this function should be fine for all derived classes
        queue_ffdshow_support = true;// support ffdshow queueing; we show ffdshow that this is patched Media Player Classic
        return S_OK;
    }
};

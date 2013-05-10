/*
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
#include "AllocatorCommon.h"

// Guid to tag IMFSample with DirectX surface index
extern GUID const GUID_SURFACE_INDEX = {0x30C8E9F6, 0x0415, 0x4B81, {0xA3, 0x15, 0x01, 0x0A, 0xC6, 0xA9, 0xDA, 0x19}};

CCritSec g_ffdshowReceive;
bool queue_ffdshow_support = false;

extern __declspec(nothrow noalias) double RoundCommonRates(double r)
{
    if ((r <= 60.0 * 2067.0 / 2048.0) && (r > 24.0 * 2027.0 / 2048.0)) {// rounding the found value, locking the frame rate
        if (r <= 30.0 * 2067.0 / 2048.0) {// lower set, 24/1.001 to 30 Hz
            if (r <= 24.0 * 2067.0 / 2048.0) {
                if (r <= 24.0 * 2047.0 / 2048.0) {
                    r = 24.0 / 1.001;
                } else {
                    r = 24.0;
                }
            } else if (r > 30.0 * 2027.0 / 2048.0) {
                if (r > 30.0 * 2047.0 / 2048.0) {
                    r = 30.0;
                } else {
                    r = 30.0 / 1.001;
                }
            } else if ((r > 25.0 * 2038.0 / 2048.0) && (r <= 25.0 * 2058.0 / 2048.0)) {
                r = 25.0;
            }
        } else if (r > 48.0 * 2027.0 / 2048.0) {// higher set, 48/1.001 to 60 Hz
            if (r <= 48.0 * 2067.0 / 2048.0) {
                if (r <= 48.0 * 2047.0 / 2048.0) {
                    r = 48.0 / 1.001;
                } else {
                    r = 48.0;
                }
            } else if (r > 60.0 * 2027.0 / 2048.0) {
                if (r > 60.0 * 2047.0 / 2048.0) {
                    r = 60.0;
                } else {
                    r = 60.0 / 1.001;
                }
            } else if ((r > 50.0 * 2038.0 / 2048.0) && (r <= 50.0 * 2058.0 / 2048.0)) {
                r = 50.0;
            }
        }
    }
    return r;
}

extern __declspec(nothrow noalias) CString GetWindowsErrorMessage(HRESULT _Error, HINSTANCE _Module)
{
    switch (_Error) {
        case S_OK:// not covered by the standard converter
            return _T("S_OK");
            // D3D errors
        case D3DERR_WRONGTEXTUREFORMAT:
            return _T("D3DERR_WRONGTEXTUREFORMAT");
        case D3DERR_UNSUPPORTEDCOLOROPERATION:
            return _T("D3DERR_UNSUPPORTEDCOLOROPERATION");
        case D3DERR_UNSUPPORTEDCOLORARG:
            return _T("D3DERR_UNSUPPORTEDCOLORARG");
        case D3DERR_UNSUPPORTEDALPHAOPERATION:
            return _T("D3DERR_UNSUPPORTEDALPHAOPERATION");
        case D3DERR_UNSUPPORTEDALPHAARG:
            return _T("D3DERR_UNSUPPORTEDALPHAARG");
        case D3DERR_TOOMANYOPERATIONS:
            return _T("D3DERR_TOOMANYOPERATIONS");
        case D3DERR_CONFLICTINGTEXTUREFILTER:
            return _T("D3DERR_CONFLICTINGTEXTUREFILTER");
        case D3DERR_UNSUPPORTEDFACTORVALUE:
            return _T("D3DERR_UNSUPPORTEDFACTORVALUE");
        case D3DERR_CONFLICTINGRENDERSTATE:
            return _T("D3DERR_CONFLICTINGRENDERSTATE");
        case D3DERR_UNSUPPORTEDTEXTUREFILTER:
            return _T("D3DERR_UNSUPPORTEDTEXTUREFILTER");
        case D3DERR_CONFLICTINGTEXTUREPALETTE:
            return _T("D3DERR_CONFLICTINGTEXTUREPALETTE");
        case D3DERR_DRIVERINTERNALERROR:
            return _T("D3DERR_DRIVERINTERNALERROR");
        case D3DERR_NOTFOUND:
            return _T("D3DERR_NOTFOUND");
        case D3DERR_MOREDATA:
            return _T("D3DERR_MOREDATA");
        case D3DERR_DEVICELOST:
            return _T("D3DERR_DEVICELOST");
        case D3DERR_DEVICENOTRESET:
            return _T("D3DERR_DEVICENOTRESET");
        case D3DERR_NOTAVAILABLE:
            return _T("D3DERR_NOTAVAILABLE");
        case D3DERR_OUTOFVIDEOMEMORY:
            return _T("D3DERR_OUTOFVIDEOMEMORY");
        case D3DERR_INVALIDDEVICE:
            return _T("D3DERR_INVALIDDEVICE");
        case D3DERR_INVALIDCALL:
            return _T("D3DERR_INVALIDCALL");
        case D3DERR_DRIVERINVALIDCALL:
            return _T("D3DERR_DRIVERINVALIDCALL");
        case D3DERR_WASSTILLDRAWING:
            return _T("D3DERR_WASSTILLDRAWING");
        case D3DOK_NOAUTOGEN:
            return _T("D3DOK_NOAUTOGEN");
        case D3DERR_DEVICEREMOVED:
            return _T("D3DERR_DEVICEREMOVED");
        case S_NOT_RESIDENT:
            return _T("S_NOT_RESIDENT");
        case S_RESIDENT_IN_SHARED_MEMORY:
            return _T("S_RESIDENT_IN_SHARED_MEMORY");
        case S_PRESENT_MODE_CHANGED:
            return _T("S_PRESENT_MODE_CHANGED");
        case S_PRESENT_OCCLUDED:
            return _T("S_PRESENT_OCCLUDED");
        case D3DERR_DEVICEHUNG:
            return _T("D3DERR_DEVICEHUNG");
        case D3DERR_UNSUPPORTEDOVERLAY:
            return _T("D3DERR_UNSUPPORTEDOVERLAY");
        case D3DERR_UNSUPPORTEDOVERLAYFORMAT:
            return _T("D3DERR_UNSUPPORTEDOVERLAYFORMAT");
        case D3DERR_CANNOTPROTECTCONTENT:
            return _T("D3DERR_CANNOTPROTECTCONTENT");
        case D3DERR_UNSUPPORTEDCRYPTO:
            return _T("D3DERR_UNSUPPORTEDCRYPTO");
        case D3DERR_PRESENT_STATISTICS_DISJOINT:
            return _T("D3DERR_PRESENT_STATISTICS_DISJOINT");
    }

    CString errmsg;
    TCHAR* pMsgBuf;
    if (DWORD len = FormatMessage(
                        _Module ? FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE
                        : FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        _Module, _Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPTSTR>(&pMsgBuf), 0, nullptr)) {
        errmsg.SetString(pMsgBuf, len);
        LocalFree(pMsgBuf);
    } else {
        errmsg.Format(L"0x%08x ", _Error);
    }
    return errmsg;
}

/*
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
#include "MainFrm.h"
#include <d3d9.h>
#include <vmr9.h>
#include <evr9.h>

// WAITORTIMERCALLBACK implementation
// note: the timer handle has to be closed afterwards by DeleteTimerQueueTimer(), but it may not be closed by the queue timer's thread itself
__declspec(nothrow noalias) VOID CALLBACK CVMROSD::TimerFunc(__in PVOID lpParameter, __in BOOLEAN TimerOrWaitFired)
{
    UNREFERENCED_PARAMETER(TimerOrWaitFired);

    TRACE(L"OSD TimerFunc\n");
    CVMROSD* pVMROSD = reinterpret_cast<CVMROSD*>(lpParameter);
    pVMROSD->ClearMessageInternal();
}

__declspec(nothrow noalias) void CVMROSD::DisplayMessage(OSD_MESSAGEPOS nPos, CStringW const& strMsg, DWORD dwDuration)
{
    TRACE(L"OSD DisplayMessage: %s, duration: %u\n", strMsg, dwDuration);
    if (!m_bShowMessage) {
        TRACE(L"OSD DisplayMessage suppressed\n");
        return;
    }

    if (m_pVMB || m_pMFVMB) {
        m_nMessagePos = nPos;
        m_strMessage = strMsg;

        CAppSettings& s = AfxGetAppSettings();
        if ((m_iFontSize != s.nOSDSize) || (m_strFontMain != s.strOSDFont)) {// initialize or renew font handle, m_strFontMain will be empty on initialization, this will also initialize m_iFontSize
            m_iFontSize = s.nOSDSize;
            m_strFontMain = s.strOSDFont;

            HFONT hFontMain = CreateFontW(m_iFontSize, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_NATURAL_QUALITY, FF_DONTCARE, m_strFontMain);
            ASSERT(hFontMain);
            if (hFontMain) {
                if (m_hFontMain) {
                    EXECUTE_ASSERT(DeleteObject(m_hFontMain));
                }
                m_hFontMain = hFontMain;
                SelectObject(m_hdcMain, hFontMain);
            }
        }

        Paint(dwDuration);
    } else if (m_pMVTO) {
        HRESULT hr = m_pMVTO->OsdDisplayMessage(strMsg, dwDuration);
        ASSERT(SUCCEEDED(hr));
    }
}

__declspec(nothrow noalias) void CVMROSD::ClearMessageInternal()
{
    ASSERT(!m_pMVTO);// can not be used when IMadVRTextOsd is active
    TRACE(L"OSD ClearMessageInternal\n");
    m_nMessagePos = OSD_NOMESSAGE;

    if (m_bSeekBarVisible) {
        TRACE(L"OSD ClearMessageInternal suppressed because of seekbar\n");
        return;
    }

    CAutoLock Lock(&m_csExternalInterfacesLock);
    if (m_bOSDVisible) {
        m_bOSDVisible = false;
        if (m_pVMB) {
            DWORD dwBackup = m_VMR9AlphaBitmap.dwFlags | VMRBITMAP_DISABLE;
            m_VMR9AlphaBitmap.dwFlags = VMRBITMAP_DISABLE;
            HRESULT hr = m_pVMB->SetAlphaBitmap(&m_VMR9AlphaBitmap);
            ASSERT(SUCCEEDED(hr));
            m_VMR9AlphaBitmap.dwFlags = dwBackup;
        } else {
            ASSERT(m_pMFVMB);
            HRESULT hr = m_pMFVMB->ClearAlphaBitmap();
            ASSERT(SUCCEEDED(hr));
        }
    }
}

__declspec(nothrow noalias) void CVMROSD::UpdateBitmap()
{
    ASSERT(!m_pMVTO);// can not be used when IMadVRTextOsd is active
    TRACE(L"OSD UpdateBitmap\n");
    ZeroMemory(&m_BitmapInfo, sizeof(m_BitmapInfo));
    if (m_hdcMain) {// remove the old one
        EXECUTE_ASSERT(DeleteDC(m_hdcMain));
        m_hdcMain = nullptr;
    }

    HDC hdcWnd = GetWindowDC(m_hWnd);
    ASSERT(hdcWnd);
    if (hdcWnd) {
        HDC hdcUtil = CreateCompatibleDC(hdcWnd);// always make a copy, as we will not be painting on the actual window area
        ASSERT(hdcUtil);
        EXECUTE_ASSERT(ReleaseDC(m_hWnd, hdcWnd));
        if (hdcUtil) {
            EXECUTE_ASSERT(SetICMMode(hdcUtil, ICM_DONE_OUTSIDEDC));// leave ICM to the receiving renderer

            BITMAPINFO bmi;
            *reinterpret_cast<__int32*>(bmi.bmiColors) = 0;
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = m_rectSeekBar.right;
            bmi.bmiHeader.biHeight = -m_rectSeekBar.bottom;// biHeight for RGB is bottom-to-top ordered for a DIB, but we will draw a regular top-down image here
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biSizeImage = 0;
            bmi.bmiHeader.biXPelsPerMeter = 0;
            bmi.bmiHeader.biYPelsPerMeter = 0;
            bmi.bmiHeader.biClrUsed = 0;
            bmi.bmiHeader.biClrImportant = 0;

            HBITMAP hbmpRender = CreateDIBSection(hdcUtil, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
            ASSERT(hbmpRender);
            if (hbmpRender) {
                m_hdcMain = hdcUtil;
                SelectObject(hdcUtil, hbmpRender);
                EXECUTE_ASSERT(GetObjectW(hbmpRender, sizeof(BITMAP), &m_BitmapInfo));

                if (m_pVMB) {
                    // Configure the VMR's bitmap structure
                    m_VMR9AlphaBitmap.dwFlags      = VMRBITMAP_HDC | VMRBITMAP_SRCCOLORKEY;
                    m_VMR9AlphaBitmap.hdc          = hdcUtil;
                    m_VMR9AlphaBitmap.pDDS         = nullptr;
                    m_VMR9AlphaBitmap.rSrc.left    = 0;
                    m_VMR9AlphaBitmap.rSrc.top     = 0;
                    m_VMR9AlphaBitmap.rSrc.right   = m_rectSeekBar.right;
                    m_VMR9AlphaBitmap.rSrc.bottom  = m_rectSeekBar.bottom;
                    m_VMR9AlphaBitmap.rDest.left   = 0.0f;
                    m_VMR9AlphaBitmap.rDest.top    = 0.0f;
                    m_VMR9AlphaBitmap.rDest.right  = 1.0f;
                    m_VMR9AlphaBitmap.rDest.bottom = 1.0f;
                    m_VMR9AlphaBitmap.fAlpha       = 1.0f;
                    m_VMR9AlphaBitmap.clrSrcKey    = OSD_TRANSPARENT;
                    m_VMR9AlphaBitmap.dwFilterMode = 0;
                } else {
                    ASSERT(m_pMFVMB);
                    // Configure the MF type bitmap structure
                    m_MFVideoAlphaBitmap.GetBitmapFromDC       = TRUE;
                    m_MFVideoAlphaBitmap.bitmap.hdc            = hdcUtil;
                    m_MFVideoAlphaBitmap.params.dwFlags        = MFVideoAlphaBitmap_SrcColorKey;
                    m_MFVideoAlphaBitmap.params.clrSrcKey      = OSD_TRANSPARENT;
                    m_MFVideoAlphaBitmap.params.rcSrc.left     = 0;
                    m_MFVideoAlphaBitmap.params.rcSrc.top      = 0;
                    m_MFVideoAlphaBitmap.params.rcSrc.right    = m_rectSeekBar.right;
                    m_MFVideoAlphaBitmap.params.rcSrc.bottom   = m_rectSeekBar.bottom;
                    m_MFVideoAlphaBitmap.params.nrcDest.left   = 0.0f;
                    m_MFVideoAlphaBitmap.params.nrcDest.top    = 0.0f;
                    m_MFVideoAlphaBitmap.params.nrcDest.right  = 1.0f;
                    m_MFVideoAlphaBitmap.params.nrcDest.bottom = 1.0f;
                    m_MFVideoAlphaBitmap.params.fAlpha         = 1.0f;
                    m_MFVideoAlphaBitmap.params.dwFilterMode   = 0;
                }

                SetTextColor(hdcUtil, OSD_TEXT);
                SetBkMode(hdcUtil, TRANSPARENT);
                SelectObject(hdcUtil, m_hFontMain);

                EXECUTE_ASSERT(DeleteObject(hbmpRender));
            }
        }
    }
}

__declspec(nothrow noalias) void CVMROSD::UpdateSeekBarPos(__int32 x)
{
    ASSERT(m_pVMB);// can only be used with IVMRMixerBitmap9
    ASSERT(m_bShowSeekBar);
    __int64 i64SeekPos;
    if (x <= 0) {
        i64SeekPos = 0;
    } else if (x >= m_rectSeekBar.right - OSD_SLIDER_CURSOR_WIDTH) {
        i64SeekPos = m_i64SeekMax;
    } else {
        i64SeekPos = x * m_i64SeekMax / (m_rectSeekBar.right - OSD_SLIDER_CURSOR_WIDTH);
    }
    m_i64SeekPos = i64SeekPos;

    CMainFrame* pMainWindow = reinterpret_cast<CMainFrame*>(reinterpret_cast<uintptr_t>(this) - offsetof(CMainFrame, m_OSD));
    EXECUTE_ASSERT(S_OK == pMainWindow->SendMessageW(WM_HSCROLL, 0xFFFFFFFF, 0));// this will also redraw the OSD
}

__declspec(nothrow noalias) void CVMROSD::Paint(DWORD dwDuration)
{
    ASSERT(!m_pMVTO);// can not be used when IMadVRTextOsd is active

    HANDLE hTimer(m_hTimer);
    if (hTimer) {
        m_hTimer = nullptr;
        EXECUTE_ASSERT(DeleteTimerQueueTimer(nullptr, hTimer, INVALID_HANDLE_VALUE));
    }
    if (!m_bOSDSuppressed && m_hdcMain && (m_bSeekBarVisible || ((m_nMessagePos != OSD_NOMESSAGE) && !m_strMessage.IsEmpty()))) {// test for availability and activity
        // clear the image to transparent
        __declspec(align(16)) static __int32 const iFillVal[4] = {0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000};
        __m128 xFillVal = _mm_load_ps(reinterpret_cast<float const*>(iFillVal));
        ULONG ulCount = static_cast<ULONG>(m_BitmapInfo.bmWidth) * static_cast<ULONG>(m_BitmapInfo.bmHeight);// expression in 4-byte units
        // the GDI functions will allocate its surfaces in non-shared memory at system allocation granulatity
        float* pDst = reinterpret_cast<float*>(m_BitmapInfo.bmBits);
        ASSERT(!(reinterpret_cast<uintptr_t>(pDst) & 15)); // if not 16-byte aligned, _mm_stream_ps will fail
        ULONG i = (ulCount + 3) >> 2;// expression in 16-byte units, rounded up, so this loop could possibly write 12 extra bytes
        // aligned writes can't go past system allocation granulatity boundaries, so rounding up is fine in this case
        do {
            _mm_stream_ps(pDst, xFillVal);
            pDst += 4;
        } while (--i); // 16 aligned bytes are written every time

        if (m_bSeekBarVisible) {
            TRACE(L"OSD Draw Slider\n");
            ASSERT(!m_rectSeekBar.left);
            ASSERT(m_rectSeekBar.bottom - m_rectSeekBar.top);
            ASSERT(m_rectSeekBar.right);
            ASSERT(m_rectCursor.top == m_rectSeekBar.top + ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1));
            ASSERT(m_rectCursor.bottom == m_rectSeekBar.bottom - ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1));
            LONG lCl = 0;
            if (m_i64SeekMax) {
                lCl = static_cast<LONG>((m_rectSeekBar.right - OSD_SLIDER_CURSOR_WIDTH) * m_i64SeekPos / m_i64SeekMax);
            }
            m_rectCursor.left = lCl;
            m_rectCursor.right = lCl + OSD_SLIDER_CURSOR_WIDTH;

            SelectObject(m_hdcMain, m_hBrushBack);
            SelectObject(m_hdcMain, m_hPenBorder);
            EXECUTE_ASSERT(Rectangle(m_hdcMain, 0, m_rectSeekBar.top, m_rectSeekBar.right, m_rectSeekBar.bottom));

            SelectObject(m_hdcMain, m_hBrushBar);
            HPEN hPenNull = static_cast<HPEN>(GetStockObject(NULL_PEN));
            ASSERT(hPenNull);
            SelectObject(m_hdcMain, hPenNull);
            EXECUTE_ASSERT(Rectangle(m_hdcMain, 1, m_rectSeekBar.top + 15, m_rectSeekBar.right - 1, m_rectSeekBar.bottom - 15));// 2 pixels less in width, 30 less in height

            HBRUSH hBrushHollow = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
            ASSERT(hBrushHollow);
            SelectObject(m_hdcMain, hBrushHollow);
            SelectObject(m_hdcMain, m_hPenCursor);
            EXECUTE_ASSERT(Rectangle(m_hdcMain, m_rectCursor.left, m_rectCursor.top, m_rectCursor.right, m_rectCursor.bottom));
        }

        if ((m_nMessagePos != OSD_NOMESSAGE) && !m_strMessage.IsEmpty()) {
            TRACE(L"OSD Draw Message: %s\n", m_strMessage);

            __declspec(align(16)) RECT rectText;
            _mm_store_ps(reinterpret_cast<float*>(&rectText), _mm_setzero_ps());// the rectangle is expanded by the next function, not just overwritten
            EXECUTE_ASSERT(DrawTextW(m_hdcMain, m_strMessage, m_strMessage.GetLength(), &rectText, DT_CALCRECT));

            LONG lTh = rectText.bottom + 10;
            if (lTh > static_cast<LONG>(m_rectSeekBar.bottom)) {
                lTh = m_rectSeekBar.bottom;
            }
            rectText.bottom = lTh;// messages always go on top

            LONG lTw = rectText.right + 20;
            DWORD dwFormat = DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX;
            if (lTw > static_cast<LONG>(m_rectSeekBar.right)) {
                dwFormat |= DT_END_ELLIPSIS;
                lTw = m_rectSeekBar.right;
            }

            // position top left or top right
            rectText.right = lTw;// top left default
            if (m_nMessagePos == OSD_TOPRIGHT) {
                rectText.left  = m_rectSeekBar.right - lTw;
                rectText.right = m_rectSeekBar.right;
            }

            SelectObject(m_hdcMain, m_hBrushBack);
            SelectObject(m_hdcMain, m_hPenBorder);
            EXECUTE_ASSERT(Rectangle(m_hdcMain, rectText.left, rectText.top, rectText.right, rectText.bottom));
            EXECUTE_ASSERT(DrawTextW(m_hdcMain, m_strMessage, m_strMessage.GetLength(), &rectText, dwFormat));
        }

        m_csExternalInterfacesLock.Lock();
        m_bOSDVisible = true;
        if (m_pVMB) {
            m_VMR9AlphaBitmap.dwFlags &= ~VMRBITMAP_DISABLE;
            HRESULT hr = m_pVMB->SetAlphaBitmap(&m_VMR9AlphaBitmap);
            ASSERT(SUCCEEDED(hr));
        } else {
            ASSERT(m_pMFVMB);
            HRESULT hr = m_pMFVMB->SetAlphaBitmap(&m_MFVideoAlphaBitmap);
            ASSERT(SUCCEEDED(hr));
        }
        m_csExternalInterfacesLock.Unlock();

        // note: the timer handle has to be closed afterwards by DeleteTimerQueueTimer(), but it may not be closed by the queue timer's thread itself
        EXECUTE_ASSERT(CreateTimerQueueTimer(&m_hTimer, nullptr, TimerFunc, this, dwDuration, 0, WT_EXECUTEONLYONCE | WT_EXECUTEINTIMERTHREAD));
    } else {// for in case the working set is empty
        ClearMessageInternal();
    }
}

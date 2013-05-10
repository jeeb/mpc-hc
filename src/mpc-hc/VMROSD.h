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

#pragma once

#include <atlbase.h>
#include "../filters/renderer/VideoRenderers/madVRAllocatorPresenter.h"

typedef enum {
    OSD_NOMESSAGE,
    OSD_TOPLEFT,
    OSD_TOPRIGHT
} OSD_MESSAGEPOS;

#define OSD_SEEKBAR_HEIGHT          40
#define OSD_SLIDER_CURSOR_HEIGHT    30
#define OSD_SLIDER_CURSOR_WIDTH     10

#define OSD_TRANSPARENT RGB(0,   0,   0)
#define OSD_BACKGROUND  RGB(32,  40,  48)
#define OSD_BORDER      RGB(48,  56,  62)
#define OSD_TEXT        RGB(224, 224, 224)
#define OSD_BAR         RGB(64,  72,  80)
#define OSD_CURSOR      RGB(192, 200, 208)

class CVMROSD
{
    static __declspec(nothrow noalias) VOID CALLBACK TimerFunc(__in PVOID lpParameter, __in BOOLEAN TimerOrWaitFired);
    __declspec(nothrow noalias) void ClearMessageInternal();// internal usage only, mostly used by the incoming timer threads, use ClearMessage() for the general task
    __declspec(nothrow noalias) void UpdateBitmap();
    __declspec(nothrow noalias) void UpdateSeekBarPos(__int32 x);
    __declspec(nothrow noalias) void Paint(DWORD dwDuration);

    __int64 m_i64SeekMax, m_i64SeekPos;
    RECT m_rectSeekBar, m_rectCursor;// note: m_rectSeekBar.left is always 0, m_rectSeekBar.right is the common usage width, m_rectSeekBar.bottom is the common usage height
    union {
        VMR9AlphaBitmap m_VMR9AlphaBitmap;
        MFVideoAlphaBitmap m_MFVideoAlphaBitmap;
    };
    BITMAP m_BitmapInfo;

    // Messages
    CStringW m_strMessage;

    // pens and brushes only managed in the constructor and destructor
    HPEN m_hPenBorder;
    HPEN m_hPenCursor;
    HBRUSH m_hBrushBack;
    HBRUSH m_hBrushBar;

    // main font, variable in size and type
    HFONT m_hFontMain;
    CStringW m_strFontMain;
    // main drawing context
    HDC m_hdcMain;

    // only one interface will be active at a time
    // the IVMRMixerBitmap9 or IMFVideoMixerBitmap interface pointer may be used by the queue timer thread, always use m_Lock to guard against multiple entry for these two
    IVMRMixerBitmap9* m_pVMB;
    IMFVideoMixerBitmap* m_pMFVMB;
    IMadVRTextOsd* m_pMVTO;// the queue timer thread isn't used for IMadVRTextOsd, do not use m_csExternalInterfacesLock with it
    CCritSec m_csExternalInterfacesLock;

    HANDLE m_hTimer;
    HWND m_hWnd;

    int m_iFontSize;

    OSD_MESSAGEPOS m_nMessagePos;

    unsigned __int8 m_bShowSeekBar : 1, m_bShowMessage : 1, m_bCursorMoving : 1, m_bSeekBarVisible : 1, m_bOSDVisible : 1, m_bOSDSuppressed : 1;
public:
    __declspec(nothrow noalias) void DisplayMessage(OSD_MESSAGEPOS nPos, CStringW const& strMsg, DWORD dwDuration = 5000);

    __declspec(nothrow noalias) __forceinline CVMROSD()
        : m_i64SeekMax(0)
        , m_i64SeekPos(0)
        , m_hTimer(nullptr)
        , m_pVMB(nullptr)
        , m_pMFVMB(nullptr)
        , m_pMVTO(nullptr)
        , m_hWnd(nullptr)
        , m_hFontMain(nullptr)
        , m_hdcMain(nullptr)
        , m_nMessagePos(OSD_NOMESSAGE)
        , m_bShowSeekBar(false)
        , m_bShowMessage(true)
        , m_bCursorMoving(false)
        , m_bSeekBarVisible(false)
        , m_bOSDVisible(false)
        , m_bOSDSuppressed(false)
        , m_hPenBorder(CreatePen(PS_SOLID, 1, OSD_BORDER))
        , m_hPenCursor(CreatePen(PS_SOLID, 4, OSD_CURSOR))
        , m_hBrushBack(CreateSolidBrush(OSD_BACKGROUND))
        , m_hBrushBar(CreateSolidBrush(OSD_BAR)) {
        ASSERT(m_hPenBorder);
        ASSERT(m_hPenCursor);
        ASSERT(m_hBrushBack);
        ASSERT(m_hBrushBar);
    }
    __declspec(nothrow noalias) __forceinline ~CVMROSD() {
        HANDLE hTimer(m_hTimer);
        if (hTimer) {
            EXECUTE_ASSERT(DeleteTimerQueueTimer(nullptr, hTimer, INVALID_HANDLE_VALUE));
        }
        // release interface pointer if present
        // two or three pointers will be null, all inherit IUnknown at VTable location 0
        if (IUnknown* pInterface = reinterpret_cast<IUnknown*>(reinterpret_cast<uintptr_t>(m_pVMB) | reinterpret_cast<uintptr_t>(m_pMFVMB) | reinterpret_cast<uintptr_t>(m_pMVTO))) {
            pInterface->Release();
        }

        if (m_hdcMain) {
            EXECUTE_ASSERT(DeleteDC(m_hdcMain));
        }
        if (m_hFontMain) {
            EXECUTE_ASSERT(DeleteObject(m_hFontMain));
        }
        EXECUTE_ASSERT(DeleteObject(m_hPenBorder));
        EXECUTE_ASSERT(DeleteObject(m_hPenCursor));
        EXECUTE_ASSERT(DeleteObject(m_hBrushBack));
        EXECUTE_ASSERT(DeleteObject(m_hBrushBar));
    }
    __declspec(nothrow noalias) __forceinline void Start(HWND hWnd, IVMRMixerBitmap9* pVMB, bool bShowSeekBar) {
        m_bShowSeekBar = bShowSeekBar;
        ASSERT(hWnd);
        ASSERT(pVMB);
        ASSERT(!m_pVMB);
        ASSERT(!m_pMFVMB);
        ASSERT(!m_pMVTO);
        TRACE(L"OSD Start, IVMRMixerBitmap9\n");
        m_hWnd = hWnd;
        m_pVMB = pVMB;
        pVMB->AddRef();

        EXECUTE_ASSERT(GetClientRect(m_hWnd, &m_rectSeekBar));
        LONG lHeight = m_rectSeekBar.bottom;
        m_rectSeekBar.top = lHeight - OSD_SEEKBAR_HEIGHT;
        m_rectCursor.top = lHeight - OSD_SEEKBAR_HEIGHT + ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1);
        m_rectCursor.bottom = lHeight - ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1);
        UpdateBitmap();
    }
    __declspec(nothrow noalias) __forceinline void Start(HWND hWnd, IMFVideoMixerBitmap* pMFVMB) {
        ASSERT(hWnd);
        ASSERT(pMFVMB);
        ASSERT(!m_pVMB);
        ASSERT(!m_pMFVMB);
        ASSERT(!m_pMVTO);
        TRACE(L"OSD Start, IMFVideoMixerBitmap\n");
        m_hWnd = hWnd;
        m_pMFVMB = pMFVMB;
        pMFVMB->AddRef();

        EXECUTE_ASSERT(GetClientRect(m_hWnd, &m_rectSeekBar));
        UpdateBitmap();
    }
    __declspec(nothrow noalias) __forceinline void Start(HWND hWnd, IMadVRTextOsd* pMVTO) {
        ASSERT(hWnd);
        ASSERT(pMVTO);
        ASSERT(!m_pVMB);
        ASSERT(!m_pMFVMB);
        ASSERT(!m_pMVTO);
        TRACE(L"OSD Start, IMadVRTextOsd\n");
        m_hWnd = hWnd;
        m_pMVTO = pMVTO;
        pMVTO->AddRef();
    }
    __declspec(nothrow noalias) __forceinline void Stop() {
        TRACE(L"OSD Stop\n");
        // reset status to default
        m_nMessagePos = OSD_NOMESSAGE;
        m_bCursorMoving = false;
        m_bSeekBarVisible = false;
        m_bOSDVisible = false;
        m_bOSDSuppressed = false;
        m_bShowSeekBar = false;
        m_bShowMessage = true;

        // release interface pointer if present
        // two or three pointers will be nullptr, all inherit IUnknown at VTable location 0
        if (IUnknown* pInterface = reinterpret_cast<IUnknown*>(reinterpret_cast<uintptr_t>(m_pVMB) | reinterpret_cast<uintptr_t>(m_pMFVMB) | reinterpret_cast<uintptr_t>(m_pMVTO))) {
            m_pVMB = nullptr;
            m_pMFVMB = nullptr;
            m_pMVTO = nullptr;
            m_hWnd = nullptr;
            pInterface->Release();
        }
    }

    __declspec(nothrow noalias) __forceinline __int64 GetPos() const {
        return m_i64SeekPos;
    }
    __declspec(nothrow noalias) __forceinline void SetPos(__int64 pos) {
        m_i64SeekPos = pos;
    }
    __declspec(nothrow noalias) __forceinline void SetRange(__int64 stop) {
        m_i64SeekMax = stop;
    }
    __declspec(nothrow noalias) __forceinline void SetSize(unsigned __int32 cx, unsigned __int32 cy) {
        if ((m_pVMB || m_pMFVMB) && (m_rectSeekBar.right != static_cast<__int32>(cx) || m_rectSeekBar.bottom != static_cast<__int32>(cy))) {
            m_rectSeekBar.right = cx;
            m_rectSeekBar.bottom = cy;
            m_rectSeekBar.top = cy - OSD_SEEKBAR_HEIGHT;
            // m_rectSeekBar.left stays 0
            m_rectCursor.top = cy - OSD_SEEKBAR_HEIGHT + ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1);
            m_rectCursor.bottom = cy - ((OSD_SEEKBAR_HEIGHT - OSD_SLIDER_CURSOR_HEIGHT) >> 1);

            UpdateBitmap();
            m_bCursorMoving = false;
            Paint(5000);
        }
    }

    __declspec(nothrow noalias) __forceinline void ClearMessage() {
        if (m_pMVTO) {
            HRESULT hr = m_pMVTO->OsdClearMessage();
            ASSERT(SUCCEEDED(hr));
            return;
        }

        HANDLE hTimer(m_hTimer);
        if (hTimer) {
            m_hTimer = nullptr;
            EXECUTE_ASSERT(DeleteTimerQueueTimer(nullptr, hTimer, INVALID_HANDLE_VALUE));
        }
        ClearMessageInternal();
    }
    __declspec(nothrow noalias) __forceinline void EnableShowMessage(bool enabled) {
        TRACE(L"OSD EnableShowMessage: %hu\n", static_cast<unsigned __int16>(enabled));
        m_bShowMessage = enabled;
    }
    __declspec(nothrow noalias) __forceinline void SuppressOSD() {
        TRACE(L"OSD SuppressOSD\n");
        m_bOSDSuppressed = true;
        m_bSeekBarVisible = false;// force clearing if in full screen
        ClearMessage();
    }
    __declspec(nothrow noalias) __forceinline void UnsuppressOSD() {
        TRACE(L"OSD UnsuppressOSD\n");
        m_bOSDSuppressed = false;
    }
    __declspec(nothrow noalias) __forceinline void BindToWindow(HWND hWnd, bool bFullScreen) {
        ASSERT(hWnd);
        ASSERT(m_hWnd);
        ASSERT(m_pVMB);
        ASSERT(!m_pMFVMB);
        ASSERT(!m_pMVTO);
        TRACE(L"OSD BindToWindow, full screen: %hu\n", bFullScreen);
        m_hWnd = hWnd;
        m_bShowSeekBar = bFullScreen;
        m_bSeekBarVisible = false;// force clearing if in full screen
        HANDLE hTimer(m_hTimer);
        if (hTimer) {
            m_hTimer = nullptr;
            EXECUTE_ASSERT(DeleteTimerQueueTimer(nullptr, hTimer, INVALID_HANDLE_VALUE));
        }
        ClearMessageInternal();
    }

    __declspec(nothrow noalias) __forceinline void OnMouseMove(__int32 x, __int32 y) {
        if (m_bShowSeekBar) {// note: only available in combination with IVMRMixerBitmap9
            if (m_bCursorMoving) {
                UpdateSeekBarPos(x);
            } else if ((0 <= x) && (m_rectSeekBar.right > x) && (m_rectSeekBar.top <= y) && (m_rectSeekBar.bottom > y)) { // point inside seekbar rectangle?
                if (!m_bSeekBarVisible) {
                    m_bSeekBarVisible = true;
                    Paint(5000);
                }
            } else {
                m_bSeekBarVisible = false;
                Paint(5000);
            }
        }
    }
    __declspec(nothrow noalias) __forceinline bool OnLButtonDown(__int32 x, __int32 y) {
        bool bRet = false;
        if (m_bShowSeekBar) {// note: only available in combination with IVMRMixerBitmap9
            if ((m_rectCursor.left <= x) && (m_rectCursor.right > x) && (m_rectCursor.top <= y) && (m_rectCursor.bottom > y)) { // point inside cursor rectangle?
                m_bCursorMoving = true;
                bRet = true;
            } else if ((0 <= x) && (m_rectSeekBar.right > x) && (m_rectSeekBar.top <= y) && (m_rectSeekBar.bottom > y)) { // point inside seekbar rectangle?
                UpdateSeekBarPos(x);
                bRet = true;
            }
        }
        return bRet;
    }
    __declspec(nothrow noalias) __forceinline bool OnLButtonUp(__int32 x, __int32 y) {
        bool bRet = false;
        if (m_bShowSeekBar) {// note: only available in combination with IVMRMixerBitmap9
            m_bCursorMoving = false;
            bRet = ((m_rectCursor.left <= x) && (m_rectCursor.right > x) && (m_rectCursor.top <= y) && (m_rectCursor.bottom > y))// point inside cursor rectangle?
                   || ((0 <= x) && (m_rectSeekBar.right > x) && (m_rectSeekBar.top <= y) && (m_rectSeekBar.bottom > y));// point inside seekbar rectangle?
        }
        return bRet;
    }
};

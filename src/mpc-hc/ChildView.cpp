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
#include "mplayerc.h"
#include "ChildView.h"
#include "MainFrm.h"

/////////////////////////////////////////////////////////////////////////////
// CChildView

CChildView::CChildView()
//: m_lastlmdowntime(0)
{
    LoadLogo();
}

CChildView::~CChildView()
{
}

BOOL CChildView::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CWnd::PreCreateWindow(cs)) {
        return FALSE;
    }

    cs.style &= ~WS_BORDER;
    cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
                                       ::LoadCursor(nullptr, IDC_ARROW), HBRUSH(COLOR_WINDOW + 1), nullptr);

    return TRUE;
}

BOOL CChildView::PreTranslateMessage(MSG* pMsg)
{
    CWnd* pParent = AfxGetApp()->m_pMainWnd;
    HWND hParentwnd = pParent->m_hWnd;
    if (!hParentwnd) { return FALSE; } // this is to solve a problem with D3D fullscreen exclusive mode: a case where the parent window is closed before the child window

    UINT message = pMsg->message;
    if ((message >= WM_MOUSEFIRST) && (message <= WM_MYMOUSELAST)) {
        DWORD dwLParam = DWORD(pMsg->lParam); // only 32 bits required in this case
        POINT p = {dwLParam & 0xffff, dwLParam >> 16};
        ::MapWindowPoints(pMsg->hwnd, hParentwnd, &p, 1);

        bool fDblClick = false;

        bool fInteractiveVideo = static_cast<CMainFrame*>(pParent)->IsInteractiveVideo();
        /*if (fInteractiveVideo)
        {
            if (pMsg->message == WM_LBUTTONDOWN)
            {
                if ((pMsg->time - m_lastlmdowntime) <= GetDoubleClickTime()
                && abs(pMsg->pt.x - m_lastlmdownpoint.x) <= GetSystemMetrics(SM_CXDOUBLECLK)
                && abs(pMsg->pt.y - m_lastlmdownpoint.y) <= GetSystemMetrics(SM_CYDOUBLECLK))
                {
                    fDblClick = true;
                    m_lastlmdowntime = 0;
                    m_lastlmdownpoint.SetPoint(0, 0);
                }
                else
                {
                    m_lastlmdowntime = pMsg->time;
                    m_lastlmdownpoint = pMsg->pt;
                }
            }
            else if (pMsg->message == WM_LBUTTONDBLCLK)
            {
                m_lastlmdowntime = pMsg->time;
                m_lastlmdownpoint = pMsg->pt;
            }
        }*/
        LPARAM coord = DWORD(p.x | (p.y << 16)); // cast to usigned to prevent sign-extension on x64
        WPARAM wParam = pMsg->wParam;
        if ((message == WM_LBUTTONDOWN || message == WM_LBUTTONUP || message == WM_MOUSEMOVE)
                && fInteractiveVideo) {
            if (message == WM_MOUSEMOVE) {
                pParent->PostMessage(message, wParam, coord);
            }

            if (fDblClick) {
                pParent->PostMessage(WM_LBUTTONDOWN, wParam, coord);
                pParent->PostMessage(WM_LBUTTONDBLCLK, wParam, coord);
            }
        } else {
            pParent->PostMessage(message, wParam, coord);
            return TRUE;
        }
    }

    return CWnd::PreTranslateMessage(pMsg);
}

void CChildView::LoadLogo()
{
    CAppSettings& s = AfxGetAppSettings();
    bool bHaveLogo = false;

    CAutoLock cAutoLock(&m_csLogo);

    m_logo.DeleteObject();

    if (s.fLogoExternal) {
        bHaveLogo = !!m_logo.LoadFromFile(s.strLogoFileName);
    }

    if (!bHaveLogo) {
        s.fLogoExternal = false;                // use the built-in logo instead
        s.strLogoFileName = "";                 // clear logo file name

        if (!m_logo.Load(s.nLogoId)) {          // try the latest selected build-in logo
            m_logo.Load(s.nLogoId = DEF_LOGO);  // if fail then use the default logo, should and must never fail
        }
    }

    if (m_hWnd) {
        Invalidate();
    }
}

CSize CChildView::GetLogoSize()
{
    BITMAP bitmap = {0};
    m_logo.GetBitmap(&bitmap);
    return CSize(bitmap.bmWidth, bitmap.bmHeight);
}

IMPLEMENT_DYNAMIC(CChildView, CWnd)

BEGIN_MESSAGE_MAP(CChildView, CWnd)
    //{{AFX_MSG_MAP(CChildView)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_SETCURSOR()
    //}}AFX_MSG_MAP
    //  ON_WM_NCHITTEST()
    ON_WM_NCHITTEST()
    ON_WM_NCLBUTTONDOWN()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChildView message handlers

void CChildView::OnPaint()
{
    // discard this call; only video renderers have to draw regularly in this window and interference is unwelcome
    // OnEraseBkgnd() calls ValidateRect() to prevent as many redraws as possible for this class
    // Do not call CWnd::OnPaint() for painting messages
}

BOOL CChildView::OnEraseBkgnd(CDC* pDC)
{
    RECT r;
    GetClientRect(&r);
    HDC hdcWnd = *pDC;
    m_csLogo.Lock();
    if (m_logo.m_hObject) {
        // bitmap in window area geometry
        BITMAP bm;
        GetObject(m_logo, sizeof(bm), &bm);
        int w = (bm.bmWidth < r.right) ? bm.bmWidth : r.right;
        int h = (bm.bmHeight < r.bottom) ? bm.bmHeight : r.bottom;
        int x = (r.right - w) >> 1;
        int y = (r.bottom - h) >> 1;

        // copy/stretch the bitmap to the window surface
        int oldmode = SetStretchBltMode(hdcWnd, STRETCH_HALFTONE);
        HDC hdcLogo = CreateCompatibleDC(hdcWnd);
        SelectObject(hdcLogo, m_logo);
        StretchBlt(hdcWnd, x, y, w, h, hdcLogo, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
        DeleteDC(hdcLogo);
        SetStretchBltMode(hdcWnd, oldmode);

        // prevent the logo from being cleared by FillRect()
        ExcludeClipRect(hdcWnd, x, y, x + w, y + h);
    }
    m_csLogo.Unlock();
    FillRect(hdcWnd, &r, HBRUSH(GetStockObject(BLACK_BRUSH))); // fill surrounding area with black, stock objects don't need to be released after use
    ValidateRect(NULL); // normally done in OnPaint(), but that one is useless in this class
    return TRUE;
}

void CChildView::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);

    static_cast<CMainFrame*>(AfxGetApp()->m_pMainWnd)->MoveVideoWindow();
}

BOOL CChildView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CWinApp* pApp = AfxGetApp();
    CMainFrame* pFrame = static_cast<CMainFrame*>(pApp->m_pMainWnd);

    if (pFrame->m_fHideCursor) {
        SetCursor(nullptr);
        return TRUE;
    }
    if (pFrame->IsSomethingLoaded() && (nHitTest == HTCLIENT)) {
        if (pFrame->GetPlaybackMode() == PM_DVD) {
            return FALSE;
        }
        ::SetCursor(pApp->LoadStandardCursor(IDC_ARROW));
        return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

LRESULT CChildView::OnNcHitTest(CPoint point)
{
    LRESULT nHitTest = CWnd::OnNcHitTest(point);

    CMainFrame* pFrame = static_cast<CMainFrame*>(AfxGetApp()->m_pMainWnd);
    bool fLeftMouseBtnUnassigned = !AssignedToCmd(wmcmd::LDOWN);
    if (!pFrame->m_fFullScreen && (pFrame->IsCaptionHidden() || fLeftMouseBtnUnassigned)) {
        RECT rcFrame;
        GetWindowRect(&rcFrame);

        // add border size to base window size
        int borderX = GetSystemMetrics(SM_CXBORDER);
        int borderY = GetSystemMetrics(SM_CYBORDER);
        LONG l = rcFrame.left - borderX;
        LONG r = rcFrame.right + borderX;
        LONG t = rcFrame.top - borderY;
        LONG b = rcFrame.bottom + borderY;

        if ((l <= point.x) && (r > point.x) && (t <= point.y) && (b > point.y)) { // point inside window rectangle?
            // point to client rectangle geometry, substract 5 * border size from the base window rectangle
            borderX = (borderX << 1) + (borderX << 2);
            borderY = (borderY << 1) + (borderY << 2);
            l += borderX;
            r -= borderX;
            t += borderY;
            b -= borderY;

            if (point.x >= r) {
                if (point.y < t) {
                    nHitTest = HTTOPRIGHT;
                } else if (point.y >= b) {
                    nHitTest = HTBOTTOMRIGHT;
                } else {
                    nHitTest = HTRIGHT;
                }
            } else if (point.x < l) {
                if (point.y < t) {
                    nHitTest = HTTOPLEFT;
                } else if (point.y >= b) {
                    nHitTest = HTBOTTOMLEFT;
                } else {
                    nHitTest = HTLEFT;
                }
            } else if (point.y < t) {
                nHitTest = HTTOP;
            } else if (point.y >= b) {
                nHitTest = HTBOTTOM;
            }
        }
    }
    return nHitTest;
}

void CChildView::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
    CMainFrame* pFrame = static_cast<CMainFrame*>(AfxGetApp()->m_pMainWnd);
    bool fLeftMouseBtnUnassigned = !AssignedToCmd(wmcmd::LDOWN);
    if (!pFrame->m_fFullScreen && (pFrame->IsCaptionHidden() || fLeftMouseBtnUnassigned)) {
        WPARAM Flag;
        switch (nHitTest) {
            case HTTOP:
                Flag = SC_SIZE | WMSZ_TOP;
                break;
            case HTTOPLEFT:
                Flag = SC_SIZE | WMSZ_TOPLEFT;
                break;
            case HTTOPRIGHT:
                Flag = SC_SIZE | WMSZ_TOPRIGHT;
                break;
            case HTLEFT:
                Flag = SC_SIZE | WMSZ_LEFT;
                break;
            case HTRIGHT:
                Flag = SC_SIZE | WMSZ_RIGHT;
                break;
            case HTBOTTOM:
                Flag = SC_SIZE | WMSZ_BOTTOM;
                break;
            case HTBOTTOMLEFT:
                Flag = SC_SIZE | WMSZ_BOTTOMLEFT;
                break;
            case HTBOTTOMRIGHT:
                Flag = SC_SIZE | WMSZ_BOTTOMRIGHT;
                break;
            default:
                CWnd::OnNcLButtonDown(nHitTest, point);
                return;
        }
        pFrame->PostMessage(WM_SYSCOMMAND, Flag, DWORD(point.x | (point.y << 16))); // cast to usigned to prevent sign-extension on x64
    }
}

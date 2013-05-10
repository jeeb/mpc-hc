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
#include "FullscreenWnd.h"
#include "MainFrm.h"

// CFullscreenWindow

extern wchar_t const gk_szFullscreenWindowName[] = L"CFullscreenWindow";

extern WNDCLASSW const gk_wcFullscreenWindowClassDef = {
    CS_CLASSDC | CS_DBLCLKS, // window class styles
    CFullscreenWindow::InitialWndProc,
    0, // no per class data, or if we have it we make it a static member variable of CFullscreenWindow
    sizeof(CFullscreenWindow*), // need to store a pointer in the user data area per instance
    nullptr, // HINSTANCE for this application run
    nullptr, // use default icon
    nullptr, // use default cursor
    nullptr, // no background specified
    nullptr, // no menu
    gk_szFullscreenWindowName
};

// The actual windows procedure that gets called after the WM_NCCREATE message is processed. This is a member function so you can access member variables and such here.
// Or just call other member functions to handle the messages
__declspec(nothrow noalias) __forceinline LRESULT CFullscreenWindow::ActualWndProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    switch (Msg) {
            // forwarded messages
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
        case WM_NCRBUTTONDBLCLK:
        case WM_NCMBUTTONDOWN:
        case WM_NCMBUTTONUP:
        case WM_NCMBUTTONDBLCLK:
        case WM_NCXBUTTONDOWN:
        case WM_NCXBUTTONUP:
        case WM_NCXBUTTONDBLCLK:

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_SYSCHAR:
        case WM_SYSDEADCHAR:
        case WM_UNICHAR:

        case WM_SYSCOMMAND:// window menu accelerator; all others are buttons

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEHWHEEL:

            return static_cast<CMainFrame*>(this)->SendMessageW(Msg, wParam, lParam);

            // unprocessed messages
        case WM_DESTROY:// no PostQuitMessage(0); on WM_DESTROY, as this class doesn't run its own thread
        case WM_PAINT:
        case WM_NCPAINT:
#ifndef _DEBUG
        case WM_CREATE:
#endif
            return 0;

        case WM_ERASEBKGND:
            EXECUTE_ASSERT(ValidateRect(m_hFullscreenWnd, NULL));// discard the order to erase the background and prevent WM_PAINT
            return 1;

#ifdef _DEBUG
        case WM_CREATE: {
            // all this does is perform a sanity check as it should be called after WM_NCCREATE
            LPCREATESTRUCT pCreatestruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            LPVOID lpCreateParam = pCreatestruct->lpCreateParams;
            CFullscreenWindow* pThiswindow = reinterpret_cast<CFullscreenWindow*>(lpCreateParam);
            ASSERT(pThiswindow == this);
            return 0;
        }
#endif
    }
    return DefWindowProcW(m_hFullscreenWnd, Msg, wParam, lParam);
}

// The first windows procedure used by the window class.
// Its only purpose is to wait till WM_NCCREATE is sent and then jam the CFullscreenWindow pointer into the user data portion of the window instance. Then, having done its duty it changes the windows procedure to StaticWndProc()
__declspec(nothrow noalias) LRESULT CALLBACK CFullscreenWindow::InitialWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_NCCREATE) {
        LPCREATESTRUCT pCreatestruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        LPVOID lpCreateParam = pCreatestruct->lpCreateParams;
        CFullscreenWindow* pThiswindow = reinterpret_cast<CFullscreenWindow*>(lpCreateParam);
        ASSERT(!pThiswindow->m_hFullscreenWnd);// this should be the first (and only) time WM_NCCREATE is processed
        pThiswindow->m_hFullscreenWnd = hWnd;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThiswindow));
        SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&CFullscreenWindow::StaticWndProc));
        return pThiswindow->ActualWndProc(Msg, wParam, lParam);
    }
    // if it isn't WM_NCCREATE, do something sensible and wait until WM_NCCREATE is sent
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

// This function's sole purpose in life is to take the pointer from the user data portion of the window instance and call the actual windows procedure on that pointer. It does no special handling for any messages.
__declspec(nothrow noalias) LRESULT CALLBACK CFullscreenWindow::StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR pUserdata = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    CFullscreenWindow* pThiswindow = reinterpret_cast<CFullscreenWindow*>(pUserdata);
    ASSERT(pThiswindow); // WM_NCCREATE should have assigned the pointer
    ASSERT(hWnd == pThiswindow->m_hFullscreenWnd);
    return pThiswindow->ActualWndProc(Msg, wParam, lParam);
}

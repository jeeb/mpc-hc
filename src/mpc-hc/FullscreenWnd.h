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

extern wchar_t const gk_szFullscreenWindowName[];
extern WNDCLASSW const gk_wcFullscreenWindowClassDef;

// note: this class is modified to be used as a base of CMainFrame
class CFullscreenWindow
{
    HCURSOR m_hCursor;
    bool m_bCursorVisible;

    __declspec(nothrow noalias) __forceinline LRESULT ActualWndProc(UINT Msg, WPARAM wParam, LPARAM lParam);
    static __declspec(nothrow noalias) LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
protected:
    /*
        1: Initialize the skeleton of the class
        CFullscreenWindow* m_pFullscreenWindow;
        ATOM m_u16FullscreenWindowClassAtom;
        ...
        m_pFullscreenWindow = new CFullscreenWindow();
    */
    __declspec(nothrow noalias) __forceinline CFullscreenWindow()
        : m_hFullscreenWnd(NULL)
        , m_bCursorVisible(true)
        , m_hCursor(::LoadCursorW(NULL, IDC_ARROW)) {
        ASSERT(m_hCursor);
    }

    /*
        2: Register the window class
        // this routine should only be called once
        ASSERT(!m_u16FullscreenWindowClassAtom);
        m_u16FullscreenWindowClassAtom = RegisterClassW(&gk_wcFullscreenWindowClassDef);
        if (!m_u16FullscreenWindowClassAtom) { ERROR(); }// class has to be registered

        3: Create the window and display it
        if (!m_pFullscreenWindow->CreateFullscreenWindow(m_hWnd, m_u16FullscreenWindowClassAtom, nWidth, nHeight)) { ERROR(); }// window has to be created
    */
    __declspec(nothrow noalias) __forceinline HWND CreateFullscreenWindow(HWND hParentWindow, ATOM RegisteredClassAtom, RECT const* pRect) {
        ASSERT(RegisteredClassAtom);// the window class must be registered in advance
        ASSERT(pRect);

        HWND hWnd = CreateWindowExW(
                        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_ACCEPTFILES | WS_EX_NOPARENTNOTIFY,
                        reinterpret_cast<wchar_t const*>(static_cast<uintptr_t>(RegisteredClassAtom)),
                        ResStr(IDS_MAINFRM_136),
                        WS_POPUP | WS_VISIBLE,
                        pRect->left, // position
                        pRect->top,
                        pRect->right, // size
                        pRect->bottom,
                        hParentWindow, // parent window
                        NULL, // no menu
                        NULL, // default HINSTANCE
                        this);
        ASSERT(hWnd);
        // The process of calling CreateWindow() actually sends a number of windows messages to the window. One of them should be WM_NCCREATE where we assign the m_hFullscreenWnd member of CFullscreenWindow. This hWnd should be the same hWnd returned by the CreateWindow() function.
        // Note: the constructor of this class must check the m_hFullscreenWnd member and handle the possibility that this class failed at initialization.
        ASSERT(hWnd == m_hFullscreenWnd);
        return hWnd;
    }

    /*
        4: Destroy all objects
        if (m_u16FullscreenWindowClassAtom) {// remove extra window class registration
            if (m_pFullscreenWindow) {// delete extra window
                EXECUTE_ASSERT(DestroyWindow(m_pFullscreenWindow->m_hFullscreenWnd));
                delete m_pFullscreenWindow;
            }
            EXECUTE_ASSERT(UnregisterClassW(reinterpret_cast<wchar_t const*>(static_cast<uintptr_t>(m_u16FullscreenWindowClassAtom)), NULL));
        }
    */

    __declspec(nothrow noalias) __forceinline void ShowCursor() {
        if (!m_bCursorVisible) {
            m_bCursorVisible = true;
            ::SetCursor(m_hCursor);
        }
    }

    __declspec(nothrow noalias) __forceinline void HideCursor() {
        if (m_bCursorVisible) {
            m_bCursorVisible = false;
            ::SetCursor(NULL);
        }
    }

    HWND m_hFullscreenWnd;
public:
    static __declspec(nothrow noalias) LRESULT CALLBACK InitialWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
};

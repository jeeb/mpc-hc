/*
 * (C) 2012-2013 see Authors.txt
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

namespace DSObjects
{
    extern wchar_t const gk_szUtilityWindowName[];
    extern WNDCLASSW const gk_wUtilityWindowClassDef;

    // note: this class is modified to be used as a base of another class
    class CUtilityWindow
    {
        __declspec(nothrow noalias) __forceinline LRESULT ActualWndProc(UINT Msg, WPARAM wParam, LPARAM lParam);
        static __declspec(nothrow noalias) LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    protected:
        /*
            1: Initialize the skeleton of the class
            CUtilityWindow* m_pUtilityWindow;
            ATOM m_u16RegisteredWindowClassAtom;
            ...
            m_pUtilityWindow = new CUtilityWindow(hBaseWindow);
        */
        __declspec(nothrow noalias) __forceinline CUtilityWindow(HWND hCallbackWnd)
            : m_hUtilityWnd(nullptr)
            , m_hCallbackWnd(hCallbackWnd) {
            ASSERT(hCallbackWnd);
        }

        /*
            2: Register the window class
            // this routine should only be called once
            ASSERT(!m_u16RegisteredWindowClassAtom);
            m_u16RegisteredWindowClassAtom = RegisterClassW(&gk_wUtilityWindowClassDef);
            if (!m_u16RegisteredWindowClassAtom) { ERROR(); }// class has to be registered

            3: Create the window and display it
            if (!m_pUtilityWindow->CreateUtilityWindow(m_u16RegisteredWindowClassAtom, nWidth, nHeight)) { ERROR(); }// window has to be created
        */
        __declspec(nothrow noalias) __forceinline HWND CreateUtilityWindow(ATOM RegisteredClassAtom, int nWidth, int nHeight) {
            ASSERT(RegisteredClassAtom);// the window class must be registered in advance
            ASSERT(nWidth);
            ASSERT(nHeight);
            ASSERT(m_hCallbackWnd);

            HWND hWnd = CreateWindowExW(
                            WS_EX_ACCEPTFILES | WS_EX_NOPARENTNOTIFY,
                            reinterpret_cast<wchar_t const*>(static_cast<uintptr_t>(RegisteredClassAtom)),
                            L"UtilityWindow",
                            WS_CHILD | WS_VISIBLE,
                            0, // position
                            0,
                            nWidth, // size
                            nHeight,
                            m_hCallbackWnd, // parent window
                            nullptr, // no menu
                            nullptr, // default HINSTANCE
                            this);
            ASSERT(hWnd);
            // The process of calling CreateWindow() actually sends a number of windows messages to the window. One of them should be WM_NCCREATE where we assign the m_hUtilityWnd member of CUtilityWindow. This hWnd should be the same hWnd returned by the CreateWindow() function.
            // Note: the constructor of this class must check the m_hUtilityWnd member and handle the possibility that this class failed at initialization.
            ASSERT(hWnd == m_hUtilityWnd);
            return hWnd;
        }

        /*
            4: Destroy all objects
            if (m_u16RegisteredWindowClassAtom) {// remove extra window class registration
                if (m_pUtilityWindow) {// delete extra window
                    EXECUTE_ASSERT(DestroyWindow(m_pUtilityWindow->m_hUtilityWnd));
                    delete m_pUtilityWindow;
                }
                EXECUTE_ASSERT(UnregisterClassW(reinterpret_cast<wchar_t const*>(static_cast<uintptr_t>(m_u16RegisteredWindowClassAtom)), nullptr));
            }
        */

        HWND m_hCallbackWnd;
        HWND m_hUtilityWnd;
    public:
        static __declspec(nothrow noalias) LRESULT CALLBACK InitialWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
    };
}

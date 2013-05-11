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

#include "MPCPngImage.h"

class CChildView : public CWnd
{
    //DWORD m_lastlmdowntime;
    //CPoint m_lastlmdownpoint;

    CCritSec m_csLogo;
    CMPCPngImage m_logo;

public:
    CChildView();
    virtual ~CChildView();

    DECLARE_DYNAMIC(CChildView)

public:
    void LoadLogo();
    CSize GetLogoSize();

protected:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* pMsg);

    DECLARE_MESSAGE_MAP()

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint point);
};

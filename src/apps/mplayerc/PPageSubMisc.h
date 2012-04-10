/*
 * $Id$
 *
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

#include "PPageBase.h"


// CPPageSubMisc dialog

class CPPageSubMisc : public CPPageBase
{
	DECLARE_DYNAMIC(CPPageSubMisc)

public:
	CPPageSubMisc();
	virtual ~CPPageSubMisc();

	// Dialog Data
	enum { IDD = IDD_PPAGESUBMISC };
	BOOL m_fPrioritizeExternalSubtitles;
	BOOL m_fDisableInternalSubtitles;
	CString m_szAutoloadPaths;
	CComboBox m_ISDbCombo;
	CString m_ISDb;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnUpdateButton2(CCmdUI* pCmdUI);
	afx_msg void OnURLModified();
};

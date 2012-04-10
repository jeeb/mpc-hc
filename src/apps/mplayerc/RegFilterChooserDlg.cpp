/*
 * $Id$
 *
 * (C) 2003-2006 Gabest
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
#include "mplayerc.h"
#include <dmo.h>
#include "RegFilterChooserDlg.h"
#include "FGFilter.h"
#include "../../DSUtil/DSUtil.h"


// CRegFilterChooserDlg dialog

//IMPLEMENT_DYNAMIC(CRegFilterChooserDlg, CResizableDialog)
CRegFilterChooserDlg::CRegFilterChooserDlg(CWnd* pParent /*=NULL*/)
	: CResizableDialog(CRegFilterChooserDlg::IDD, pParent)
{
}

CRegFilterChooserDlg::~CRegFilterChooserDlg()
{
	POSITION pos = m_filters.GetHeadPosition();
	while (pos) {
		delete m_filters.GetNext(pos);
	}
}

void CRegFilterChooserDlg::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST2, m_list);
}

void CRegFilterChooserDlg::AddToList(IMoniker* pMoniker)
{
	CComPtr<IPropertyBag> pPB;
	if (SUCCEEDED(pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPB))) {
		CComVariant var;
		if (SUCCEEDED(pPB->Read(CComBSTR(_T("FriendlyName")), &var, NULL))) {
			m_list.SetItemData(
				m_list.InsertItem(-1, CString(CStringW(var.bstrVal))),
				(DWORD_PTR)m_monikers.AddTail(pMoniker));
		}
	}

}


BEGIN_MESSAGE_MAP(CRegFilterChooserDlg, CResizableDialog)
	ON_LBN_DBLCLK(IDC_LIST1, OnLbnDblclkList1)
	ON_UPDATE_COMMAND_UI(IDOK, OnUpdateOK)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST2, OnNMDblclkList2)
END_MESSAGE_MAP()


// CRegFilterChooserDlg message handlers

BOOL CRegFilterChooserDlg::OnInitDialog()
{
	__super::OnInitDialog();

	BeginEnumSysDev(CLSID_LegacyAmFilterCategory, pMoniker) {
		AddToList(pMoniker);
	}
	EndEnumSysDev

	BeginEnumSysDev(DMOCATEGORY_VIDEO_EFFECT, pMoniker) {
		AddToList(pMoniker);
	}
	EndEnumSysDev

	BeginEnumSysDev(DMOCATEGORY_AUDIO_EFFECT, pMoniker) {
		AddToList(pMoniker);
	}
	EndEnumSysDev

	AddAnchor(IDC_LIST2, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_BUTTON1, BOTTOM_LEFT);
	AddAnchor(IDOK, BOTTOM_RIGHT);
	AddAnchor(IDCANCEL, BOTTOM_RIGHT);

	SetMinTrackSize(CSize(300,100));

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CRegFilterChooserDlg::OnLbnDblclkList1()
{
	SendMessage(WM_COMMAND, IDOK);
}

void CRegFilterChooserDlg::OnUpdateOK(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(!!m_list.GetFirstSelectedItemPosition());
}

void CRegFilterChooserDlg::OnBnClickedOk()
{
	CComPtr<IMoniker> pMoniker;

	POSITION pos = m_list.GetFirstSelectedItemPosition();
	if (pos) {
		pos = (POSITION)m_list.GetItemData(m_list.GetNextSelectedItem(pos));
	}
	if (pos) {
		pMoniker = m_monikers.GetAt(pos);
	}
	if (pMoniker) {
		CFGFilterRegistry fgf(pMoniker);
		FilterOverride* f = DNew FilterOverride;
		f->fDisabled = false;
		f->type = FilterOverride::REGISTERED;
		f->name = fgf.GetName();
		f->dispname = fgf.GetDisplayName();
		f->guids.AddTailList(&fgf.GetTypes());
		f->backup.AddTailList(&fgf.GetTypes());
		f->dwMerit = fgf.GetMeritForDirectShow();
		f->iLoadType = FilterOverride::MERIT;
		m_filters.AddTail(f);
	}

	__super::OnOK();
}

void CRegFilterChooserDlg::OnBnClickedButton1()
{
	CFileDialog dlg(TRUE, NULL, NULL,
					OFN_EXPLORER|OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,
					_T("DirectShow Filters (*.ax,*.dll)|*.ax;*.dll|"), this, 0);

	if (dlg.DoModal() == IDOK) {
		CFilterMapper2 fm2(false);
		fm2.Register(dlg.GetPathName());
		m_filters.AddTail(&fm2.m_filters);
		fm2.m_filters.RemoveAll();

		__super::OnOK();
	}
}

void CRegFilterChooserDlg::OnNMDblclkList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	if (m_list.GetFirstSelectedItemPosition()) {
		OnBnClickedOk();
	}

	*pResult = 0;
}

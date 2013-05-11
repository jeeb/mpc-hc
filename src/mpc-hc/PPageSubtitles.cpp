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
#include "MainFrm.h"
#include "PPageSubtitles.h"


// CPPageSubtitles dialog

IMPLEMENT_DYNAMIC(CPPageSubtitles, CPPageBase)
CPPageSubtitles::CPPageSubtitles()
    : CPPageBase(CPPageSubtitles::IDD, CPPageSubtitles::IDD)
    , m_fOverridePlacement(FALSE)
    , m_nHorPos(0)
    , m_nVerPos(0)
    , m_nSPCSize(0)
    , m_fSPCAllowAnimationWhenBuffering(TRUE)
    , m_iPositionRelative(TRUE)
    , m_nSubDelayInterval(0)
{
}

void CPPageSubtitles::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CHECK3, m_fOverridePlacement);
    DDX_Text(pDX, IDC_EDIT2, m_nHorPos);
    DDX_Control(pDX, IDC_SPIN2, m_nHorPosCtrl);
    DDX_Text(pDX, IDC_EDIT3, m_nVerPos);
    DDX_Control(pDX, IDC_SPIN3, m_nVerPosCtrl);
    DDX_Text(pDX, IDC_EDIT1, m_nSPCSize);
    DDX_Control(pDX, IDC_SPIN1, m_nSPCSizeCtrl);
    DDX_Control(pDX, IDC_COMBO1, m_spmaxres);
    DDX_Control(pDX, IDC_EDIT2, m_nHorPosEdit);
    DDX_Control(pDX, IDC_EDIT3, m_nVerPosEdit);
    DDX_Check(pDX, IDC_CHECK_SPCANIMWITHBUFFER, m_fSPCAllowAnimationWhenBuffering);
    DDX_Check(pDX, IDC_CHECK_RELATIVETO, m_iPositionRelative);
    DDX_Text(pDX, IDC_EDIT4, m_nSubDelayInterval);
}


BEGIN_MESSAGE_MAP(CPPageSubtitles, CPPageBase)
    ON_UPDATE_COMMAND_UI(IDC_EDIT2, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_SPIN2, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_EDIT3, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_SPIN3, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_STATIC1, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_STATIC2, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_STATIC3, OnUpdatePosOverride)
    ON_UPDATE_COMMAND_UI(IDC_STATIC4, OnUpdatePosOverride)
    ON_EN_CHANGE(IDC_EDIT4, OnSubDelayInterval)
END_MESSAGE_MAP()


// CPPageSubtitles message handlers

BOOL CPPageSubtitles::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_COMBO1);

    const CAppSettings& s = AfxGetAppSettings();

    m_fOverridePlacement = s.fOverridePlacement;
    m_nHorPos = s.nHorPos;
    m_nHorPosCtrl.SetRange(-10, 110);
    m_nVerPos = s.nVerPos;
    m_nVerPosCtrl.SetRange(110, -10);
    m_nSPCSize = s.m_RenderersSettings.nSPCSize;
    m_nSPCSizeCtrl.SetRange(0, 60);
    m_spmaxres.AddString(_T("Screen"));
    m_spmaxres.AddString(_T("\u00BE Screen"));
    m_spmaxres.AddString(_T("\u00BD Screen"));
    m_spmaxres.SetCurSel(s.m_RenderersSettings.nSPCMaxRes);
    m_fSPCAllowAnimationWhenBuffering = s.m_RenderersSettings.fSPCAllowAnimationWhenBuffering;
    m_iPositionRelative = s.m_RenderersSettings.bPositionRelative;
    m_nSubDelayInterval = s.nSubDelayInterval;

    UpdateData(FALSE);

    CreateToolTip();

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageSubtitles::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    bool bOverridePlacement = static_cast<bool>(m_fOverridePlacement), bSPCAllowAnimationWhenBuffering = static_cast<bool>(m_fSPCAllowAnimationWhenBuffering), bPositionRelative = static_cast<bool>(m_iPositionRelative);
    unsigned __int8 u8Spmaxres = static_cast<unsigned __int8>(m_spmaxres.GetCurSel());
    if (s.fOverridePlacement != bOverridePlacement
            || s.nHorPos != m_nHorPos
            || s.nVerPos != m_nVerPos
            || s.m_RenderersSettings.nSPCSize != m_nSPCSize
            || s.nSubDelayInterval != m_nSubDelayInterval
            || s.m_RenderersSettings.nSPCMaxRes != u8Spmaxres
            || s.m_RenderersSettings.fSPCAllowAnimationWhenBuffering != bSPCAllowAnimationWhenBuffering
            || s.m_RenderersSettings.bPositionRelative != bPositionRelative) {
        s.fOverridePlacement = bOverridePlacement;
        s.nHorPos = m_nHorPos;
        s.nVerPos = m_nVerPos;
        s.m_RenderersSettings.nSPCSize = m_nSPCSize;
        s.nSubDelayInterval = m_nSubDelayInterval;
        s.m_RenderersSettings.nSPCMaxRes = u8Spmaxres;
        s.m_RenderersSettings.fSPCAllowAnimationWhenBuffering = bSPCAllowAnimationWhenBuffering;
        s.m_RenderersSettings.bPositionRelative = bPositionRelative;

        if (CMainFrame* pFrame = (CMainFrame*)GetParentFrame()) {
            pFrame->SetSubtitle(0, true, true);
        }
    }

    return __super::OnApply();
}

void CPPageSubtitles::OnUpdatePosOverride(CCmdUI* pCmdUI)
{
    UpdateData();
    pCmdUI->Enable(m_fOverridePlacement);
}

void CPPageSubtitles::OnSubDelayInterval()
{
    // If incorrect number, revert modifications
    if (!UpdateData()) {
        UpdateData(FALSE);
    }

    SetModified();
}

/*
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
#include "PPageOutput.h"
#include "moreuuids.h"
#include "PPageMisc.h"
#include <psapi.h>
#include "WinAPIUtils.h"


// CPPageMisc dialog

IMPLEMENT_DYNAMIC(CPPageMisc, CPPageBase)
CPPageMisc::CPPageMisc()
    : CPPageBase(CPPageMisc::IDD, CPPageMisc::IDD)
    , m_iBrightness(0)
    , m_iContrast(0)
    , m_iHue(0)
    , m_iSaturation(0)
    , m_nUpdaterAutoCheck(-1)
    , m_nUpdaterDelay(7)
{
}

CPPageMisc::~CPPageMisc()
{
}

void CPPageMisc::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SLI_BRIGHTNESS, m_SliBrightness);
    DDX_Control(pDX, IDC_SLI_CONTRAST, m_SliContrast);
    DDX_Control(pDX, IDC_SLI_HUE, m_SliHue);
    DDX_Control(pDX, IDC_SLI_SATURATION, m_SliSaturation);
    DDX_Control(pDX, IDC_EXPORT_KEYS, m_ExportKeys);
    DDX_Text(pDX, IDC_STATIC1, m_sBrightness);
    DDX_Text(pDX, IDC_STATIC2, m_sContrast);
    DDX_Text(pDX, IDC_STATIC3, m_sHue);
    DDX_Text(pDX, IDC_STATIC4, m_sSaturation);
    DDX_Check(pDX, IDC_CHECK1, m_nUpdaterAutoCheck);
    DDX_Text(pDX, IDC_EDIT1, m_nUpdaterDelay);
    DDX_Control(pDX, IDC_CHECK1, m_updaterAutoCheckCtrl);
    DDX_Control(pDX, IDC_EDIT1, m_updaterDelayCtrl);
    DDX_Control(pDX, IDC_SPIN1, m_updaterDelaySpin);

    // Validate the delay between each check
    if (pDX->m_bSaveAndValidate && (m_nUpdaterDelay < 1 || m_nUpdaterDelay > 365)) {
        m_updaterDelayCtrl.ShowBalloonTip(ResStr(IDS_UPDATE_DELAY_ERROR_TITLE), ResStr(IDS_UPDATE_DELAY_ERROR_MSG), TTI_ERROR);
        pDX->PrepareEditCtrl(IDC_EDIT1);
        pDX->Fail();
    }
}


BEGIN_MESSAGE_MAP(CPPageMisc, CPPageBase)
    ON_WM_HSCROLL()
    ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
    ON_BN_CLICKED(IDC_RESET_SETTINGS, OnResetSettings)
    ON_BN_CLICKED(IDC_EXPORT_SETTINGS, OnExportSettings)
    ON_BN_CLICKED(IDC_EXPORT_KEYS, OnExportKeys)
    ON_UPDATE_COMMAND_UI(IDC_EDIT1, OnUpdateDelayEditBox)
    ON_UPDATE_COMMAND_UI(IDC_SPIN1, OnUpdateDelayEditBox)
    ON_BN_CLICKED(IDC_BACKGROUNDCOLOR, OnBackgroundcolor)
END_MESSAGE_MAP()


// CPPageMisc message handlers

BOOL CPPageMisc::OnInitDialog()
{
    __super::OnInitDialog();

    const CAppSettings& s = AfxGetAppSettings();

    CreateToolTip();

    m_iBrightness = s.iBrightness;
    m_iContrast   = s.iContrast;
    m_iHue        = s.iHue;
    m_iSaturation = s.iSaturation;

    m_SliBrightness.EnableWindow(TRUE);
    m_SliBrightness.SetRange(-100, 100, true);
    m_SliBrightness.SetTic(0);
    m_SliBrightness.SetPos(m_iBrightness);

    m_SliContrast.EnableWindow(TRUE);
    m_SliContrast.SetRange(-100, 100, true);
    m_SliContrast.SetTic(0);
    m_SliContrast.SetPos(m_iContrast);

    m_SliHue.EnableWindow(TRUE);
    m_SliHue.SetRange(-180, 180, true);
    m_SliHue.SetTic(0);
    m_SliHue.SetPos(m_iHue);

    m_SliSaturation.EnableWindow(TRUE);
    m_SliSaturation.SetRange(-100, 100, true);
    m_SliSaturation.SetTic(0);
    m_SliSaturation.SetPos(m_iSaturation);

    if (AfxGetMyApp()->IsIniValid()) {
        m_ExportKeys.EnableWindow(FALSE);
    }

    m_iBrightness ? m_sBrightness.Format(_T("%+d"), m_iBrightness) : m_sBrightness = _T("0");
    m_iContrast ? m_sContrast.Format(_T("%+d"), m_iContrast) : m_sContrast = _T("0");
    m_iHue ? m_sHue.Format(_T("%+d"), m_iHue) : m_sHue = _T("0");
    m_iSaturation ? m_sSaturation.Format(_T("%+d"), m_iSaturation) : m_sSaturation = _T("0");

    m_nUpdaterAutoCheck = s.nUpdaterAutoCheck;
    m_nUpdaterDelay = s.nUpdaterDelay;
    m_updaterDelaySpin.SetRange32(1, 365);

    UpdateData(FALSE);

    return TRUE;
}

BOOL CPPageMisc::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    s.iBrightness = m_iBrightness;
    s.iContrast   = m_iContrast;
    s.iHue        = m_iHue;
    s.iSaturation = m_iSaturation;

    s.nUpdaterAutoCheck = m_nUpdaterAutoCheck;
    s.nUpdaterDelay = m_nUpdaterDelay;

    return __super::OnApply();
}

void CPPageMisc::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    UpdateData();
    if (*pScrollBar == m_SliBrightness) {
        m_iBrightness = m_SliBrightness.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Brightness, m_iBrightness, m_iContrast, m_iHue, m_iSaturation);
        m_iBrightness ? m_sBrightness.Format(_T("%+d"), m_iBrightness) : m_sBrightness = _T("0");
    } else if (*pScrollBar == m_SliContrast) {
        m_iContrast = m_SliContrast.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Contrast, m_iBrightness, m_iContrast, m_iHue, m_iSaturation);
        m_iContrast ? m_sContrast.Format(_T("%+d"), m_iContrast) : m_sContrast = _T("0");
    } else if (*pScrollBar == m_SliHue) {
        m_iHue = m_SliHue.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Hue, m_iBrightness, m_iContrast, m_iHue, m_iSaturation);
        m_iHue ? m_sHue.Format(_T("%+d"), m_iHue) : m_sHue = _T("0");
    } else if (*pScrollBar == m_SliSaturation) {
        m_iSaturation = m_SliSaturation.GetPos();
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_Saturation, m_iBrightness, m_iContrast, m_iHue, m_iSaturation);
        m_iSaturation ? m_sSaturation.Format(_T("%+d"), m_iSaturation) : m_sSaturation = _T("0");
    }

    UpdateData(FALSE);

    SetModified();

    __super::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CPPageMisc::OnBnClickedReset()
{
    m_iBrightness = AfxGetMyApp()->GetColorControl(ProcAmp_Brightness)->DefaultValue;
    m_iContrast   = AfxGetMyApp()->GetColorControl(ProcAmp_Contrast)->DefaultValue;
    m_iHue        = AfxGetMyApp()->GetColorControl(ProcAmp_Hue)->DefaultValue;
    m_iSaturation = AfxGetMyApp()->GetColorControl(ProcAmp_Saturation)->DefaultValue;

    m_SliBrightness.SetPos(m_iBrightness);
    m_SliContrast.SetPos(m_iContrast);
    m_SliHue.SetPos(m_iHue);
    m_SliSaturation.SetPos(m_iSaturation);

    m_iBrightness ? m_sBrightness.Format(_T("%+d"), m_iBrightness) : m_sBrightness = _T("0");
    m_iContrast ? m_sContrast.Format(_T("%+d"), m_iContrast) : m_sContrast = _T("0");
    m_iHue ? m_sHue.Format(_T("%+d"), m_iHue) : m_sHue = _T("0");
    m_iSaturation ? m_sSaturation.Format(_T("%+d"), m_iSaturation) : m_sSaturation = _T("0");

    ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_All, m_iBrightness, m_iContrast, m_iHue, m_iSaturation);

    UpdateData(FALSE);

    SetModified();
}

void CPPageMisc::OnUpdateDelayEditBox(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_updaterAutoCheckCtrl.GetCheck() == BST_CHECKED);
}

void CPPageMisc::OnResetSettings()
{
    if (MessageBox(ResStr(IDS_RESET_SETTINGS_WARNING), ResStr(IDS_RESET_SETTINGS), MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
        ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SendMessage(WM_CLOSE);

        ShellExecute(nullptr, _T("open"), GetProgramPath(true), _T("/reset"), nullptr, SW_SHOWNORMAL);
    }
}

void CPPageMisc::OnExportSettings()
{
    if (GetParent()->GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled()) {
        int ret = MessageBox(ResStr(IDS_EXPORT_SETTINGS_WARNING), ResStr(IDS_EXPORT_SETTINGS), MB_ICONEXCLAMATION | MB_YESNOCANCEL);

        if (ret == IDCANCEL) {
            return;
        } else if (ret == IDYES) {
            GetParent()->PostMessage(PSM_APPLY);
        }
    }

    CString ext = AfxGetMyApp()->IsIniValid() ? _T("ini") : _T("reg");
    CFileDialog fileSaveDialog(FALSE, ext, _T("mpc-hc-settings.") + ext);

    if (fileSaveDialog.DoModal() == IDOK) {
        if (AfxGetMyApp()->ExportSettings(fileSaveDialog.GetPathName())) {
            MessageBox(ResStr(IDS_EXPORT_SETTINGS_SUCCESS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
        } else {
            MessageBox(ResStr(IDS_EXPORT_SETTINGS_FAILED), ResStr(IDS_EXPORT_SETTINGS), MB_ICONERROR | MB_OK);
        }
    }
}

void CPPageMisc::OnExportKeys()
{
    if (GetParent()->GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled()) {
        int ret = MessageBox(ResStr(IDS_EXPORT_SETTINGS_WARNING), ResStr(IDS_EXPORT_SETTINGS), MB_ICONEXCLAMATION | MB_YESNOCANCEL);

        if (ret == IDCANCEL) {
            return;
        } else if (ret == IDYES) {
            GetParent()->PostMessage(PSM_APPLY);
        }
    }

    CFileDialog fileSaveDialog(FALSE, _T("reg"), _T("mpc-hc-keys.reg"));

    if (fileSaveDialog.DoModal() == IDOK) {
        if (AfxGetMyApp()->ExportSettings(fileSaveDialog.GetPathName(), _T("Commands2"))) {
            MessageBox(ResStr(IDS_EXPORT_SETTINGS_SUCCESS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
        } else {
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                MessageBox(ResStr(IDS_EXPORT_SETTINGS_NO_KEYS), ResStr(IDS_EXPORT_SETTINGS), MB_ICONINFORMATION | MB_OK);
            } else {
                MessageBox(ResStr(IDS_EXPORT_SETTINGS_FAILED), ResStr(IDS_EXPORT_SETTINGS), MB_ICONERROR | MB_OK);
            }
        }
    }
}

void CPPageMisc::OnCancel()
{
    CAppSettings& s = AfxGetAppSettings();

    ((CMainFrame*)AfxGetMyApp()->GetMainWnd())->SetColorControl(ProcAmp_All, s.iBrightness, s.iContrast, s.iHue, s.iSaturation);
    __super::OnCancel();
}

void CPPageMisc::OnBackgroundcolor()
{
    // the ChooseColor systems work with XBGR ordering, while XRGB is standard for the renderer parts
    // _byteswap_ulong(x) >> 8; 0xAABBCCDD to 0xDDCCBBAA to 0x00DDCCBB, the cheap x86 bswap instruction is more suitable for such a conversion than set of values that are individually masked, shifted and bitwise OR'd together
    CRenderersSettings& s = AfxGetAppSettings().m_RenderersSettings;

    DWORD dwBPresetCustomColors[16];
    ptrdiff_t i = 15;
    do {
        dwBPresetCustomColors[i] = _byteswap_ulong(s.dwBPresetCustomColors[i]) >> 8;
    } while (--i >= 0);
    DWORD dwBackgoundColor = _byteswap_ulong(s.dwBackgoundColor) >> 8;

    CHOOSECOLORW cc = {sizeof(CHOOSECOLORW), m_hWnd, NULL, dwBackgoundColor, dwBPresetCustomColors, CC_FULLOPEN | CC_RGBINIT, NULL, NULL, NULL};
    if (ChooseColorW(&cc)) {
        ptrdiff_t j = 15;
        do {
            s.dwBPresetCustomColors[j] = _byteswap_ulong(dwBPresetCustomColors[j]) >> 8;
        } while (--j >= 0);
        s.dwBackgoundColor = _byteswap_ulong(cc.rgbResult) >> 8;
    }
}

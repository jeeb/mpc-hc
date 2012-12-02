/*
 * (C) 2012 see Authors.txt
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
#include "UpdateCheckerDlg.h"
#include <afxdialogex.h>

IMPLEMENT_DYNAMIC(UpdateCheckerDlg, CDialog)

UpdateCheckerDlg::UpdateCheckerDlg(Update_Status updateStatus, const Version& latestVersion, CWnd* pParent /*=NULL*/)
    : CDialog(UpdateCheckerDlg::IDD, pParent)
    , m_updateStatus(updateStatus)
{
    switch (updateStatus) {
        case UPDATER_UPDATE_AVAILABLE:
        case UPDATER_UPDATE_AVAILABLE_IGNORED:
            m_text.Format(IDS_NEW_UPDATE_AVAILABLE,
                          latestVersion.major, latestVersion.minor, latestVersion.patch, latestVersion.revision,
                          UpdateChecker::MPC_HC_VERSION.major, UpdateChecker::MPC_HC_VERSION.minor, UpdateChecker::MPC_HC_VERSION.patch, UpdateChecker::MPC_HC_VERSION.revision);
            break;
        case UPDATER_LATEST_STABLE:
            m_text.LoadString(IDS_USING_LATEST_STABLE);
            break;
        case UPDATER_NEWER_VERSION:
            m_text.Format(IDS_USING_NEWER_VERSION,
                          UpdateChecker::MPC_HC_VERSION.major, UpdateChecker::MPC_HC_VERSION.minor, UpdateChecker::MPC_HC_VERSION.patch, UpdateChecker::MPC_HC_VERSION.revision,
                          latestVersion.major, latestVersion.minor, latestVersion.patch, latestVersion.revision);
            break;
        case UPDATER_ERROR:
            m_text.LoadString(IDS_UPDATE_ERROR);
            break;
        default:
            ASSERT(0); // should never happen
    }
}

UpdateCheckerDlg::~UpdateCheckerDlg()
{
}

void UpdateCheckerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_UPDATE_DLG_TEXT, m_text);
    DDX_Control(pDX, IDC_UPDATE_ICON, m_icon);
    DDX_Control(pDX, IDC_UPDATE_DL_BUTTON, m_dlButton);
    DDX_Control(pDX, IDC_UPDATE_LATER_BUTTON, m_laterButton);
    DDX_Control(pDX, IDC_UPDATE_IGNORE_BUTTON, m_ignoreButton);
}


BEGIN_MESSAGE_MAP(UpdateCheckerDlg, CDialog)
    ON_BN_CLICKED(IDC_UPDATE_DL_BUTTON, OnOpenDownloadPage)
    ON_BN_CLICKED(IDC_UPDATE_LATER_BUTTON, OnUpdateLater)
    ON_BN_CLICKED(IDC_UPDATE_IGNORE_BUTTON, OnIgnoreUpdate)
END_MESSAGE_MAP()

BOOL UpdateCheckerDlg::OnInitDialog()
{
    BOOL ret = __super::OnInitDialog();

    switch (m_updateStatus) {
        case UPDATER_UPDATE_AVAILABLE:
        case UPDATER_UPDATE_AVAILABLE_IGNORED:
            m_icon.SetIcon(LoadIcon(NULL, IDI_QUESTION));
            break;
        case UPDATER_LATEST_STABLE:
        case UPDATER_NEWER_VERSION:
        case UPDATER_ERROR: {
            m_icon.SetIcon(LoadIcon(NULL, (m_updateStatus == UPDATER_ERROR) ? IDI_WARNING : IDI_INFORMATION));
            m_dlButton.ShowWindow(SW_HIDE);
            m_laterButton.ShowWindow(SW_HIDE);
            m_ignoreButton.SetWindowText(ResStr(IDS_UPDATE_CLOSE));

            CRect buttonRect, windowRect;
            m_ignoreButton.GetWindowRect(&buttonRect);
            ScreenToClient(&buttonRect);
            // Reduce the button width
            buttonRect.left += 30;
            // Center the button
            GetWindowRect(&windowRect);
            buttonRect.MoveToX((windowRect.Width() - buttonRect.Width()) / 2);
            m_ignoreButton.MoveWindow(&buttonRect);

            // Change the default button
            SetDefID(IDC_UPDATE_IGNORE_BUTTON);
            ret = FALSE; // Focus has been set explicitly
        }
        break;
        default:
            ASSERT(0); // should never happen
    }

    return ret;
}

void UpdateCheckerDlg::OnOpenDownloadPage()
{
    ShellExecute(NULL, _T("open"), _T("http://www.cccp-project.net/"), NULL, NULL, SW_SHOWNORMAL);

    EndDialog(IDC_UPDATE_DL_BUTTON);
}

void UpdateCheckerDlg::OnUpdateLater()
{
    EndDialog(IDC_UPDATE_LATER_BUTTON);
}

void UpdateCheckerDlg::OnIgnoreUpdate()
{
    EndDialog((m_updateStatus == UPDATER_UPDATE_AVAILABLE) ? IDC_UPDATE_IGNORE_BUTTON : 0);
}

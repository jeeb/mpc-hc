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
#include "PPageOutput.h"
#include "SysVersion.h"
#include "moreuuids.h"
#include "Monitors.h"
#include "MPCPngImage.h"

// CPPageOutput dialog

IMPLEMENT_DYNAMIC(CPPageOutput, CPPageBase)
CPPageOutput::CPPageOutput()
    : CPPageBase(CPPageOutput::IDD, CPPageOutput::IDD)
    , m_iDSVideoRendererType(0)
    , m_iRMVideoRendererType(0)
    , m_iQTVideoRendererType(0)
    , m_iAudioRendererType(0)
    , m_iDX9Resizer(0)
    , m_iMixerBuffersBase(0)
    , m_dRefreshRateAdjust(1.0)
    , m_fVMR9MixerYUV(FALSE)
    , m_fVMR9AlterativeVSync(FALSE)
    , m_fD3DFullscreen(FALSE)
    , m_fD3D9RenderDevice(FALSE)
    , m_iD3D9RenderDevice(-1)
    , m_tick(nullptr)
    , m_cross(nullptr)
{
}

CPPageOutput::~CPPageOutput()
{
    DestroyIcon(m_tick);
    DestroyIcon(m_cross);
}

void CPPageOutput::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_VIDRND_COMBO, m_iDSVideoRendererTypeCtrl);
    DDX_Control(pDX, IDC_RMRND_COMBO, m_iRMVideoRendererTypeCtrl);
    DDX_Control(pDX, IDC_QTRND_COMBO, m_iQTVideoRendererTypeCtrl);
    DDX_Control(pDX, IDC_AUDRND_COMBO, m_iAudioRendererTypeCtrl);
    DDX_Control(pDX, IDC_D3D9DEVICE_COMBO, m_iD3D9RenderDeviceCtrl);
    DDX_CBIndex(pDX, IDC_DX9RESIZER_COMBO, m_iDX9Resizer);
    DDX_Control(pDX, IDC_VIDRND_DXVA_SUPPORT, m_iDSDXVASupport);
    DDX_Control(pDX, IDC_VIDRND_SUBTITLE_SUPPORT, m_iDSSubtitleSupport);
    DDX_Control(pDX, IDC_VIDRND_SAVEIMAGE_SUPPORT, m_iDSSaveImageSupport);
    DDX_Control(pDX, IDC_VIDRND_SHADER_SUPPORT, m_iDSShaderSupport);
    DDX_Control(pDX, IDC_VIDRND_ROTATION_SUPPORT, m_iDSRotationSupport);
    DDX_Control(pDX, IDC_RMRND_SUBTITLE_SUPPORT, m_iRMSubtitleSupport);
    DDX_Control(pDX, IDC_RMRND_SAVEIMAGE_SUPPORT, m_iRMSaveImageSupport);
    DDX_Control(pDX, IDC_QTRND_SUBTITLE_SUPPORT, m_iQTSubtitleSupport);
    DDX_Control(pDX, IDC_QTRND_SAVEIMAGE_SUPPORT, m_iQTSaveImageSupport);
    DDX_CBIndex(pDX, IDC_RMRND_COMBO, m_iRMVideoRendererType);
    DDX_CBIndex(pDX, IDC_QTRND_COMBO, m_iQTVideoRendererType);
    DDX_CBIndex(pDX, IDC_AUDRND_COMBO, m_iAudioRendererType);
    DDX_CBIndex(pDX, IDC_DX9RESIZER_COMBO, m_iDX9Resizer);
    DDX_CBIndex(pDX, IDC_D3D9DEVICE_COMBO, m_iD3D9RenderDevice);
    DDX_Check(pDX, IDC_D3D9DEVICE, m_fD3D9RenderDevice);
    DDX_Check(pDX, IDC_FULLSCREEN_MONITOR_CHECK, m_fD3DFullscreen);
    DDX_Check(pDX, IDC_DSVMR9ALTERNATIVEVSYNC, m_fVMR9AlterativeVSync);
    DDX_Check(pDX, IDC_DSVMR9YUVMIXER, m_fVMR9MixerYUV);
    DDX_CBIndex(pDX, IDC_MIXERBUFFERS, m_iMixerBuffersBase);
    DDX_Text(pDX, IDC_REFRESHRATEADJ, m_dRefreshRateAdjust);
}

BEGIN_MESSAGE_MAP(CPPageOutput, CPPageBase)
    ON_CBN_SELCHANGE(IDC_VIDRND_COMBO, &CPPageOutput::OnDSRendererChange)
    ON_CBN_SELCHANGE(IDC_RMRND_COMBO, &CPPageOutput::OnRMRendererChange)
    ON_CBN_SELCHANGE(IDC_QTRND_COMBO, &CPPageOutput::OnQTRendererChange)
    ON_BN_CLICKED(IDC_D3D9DEVICE, OnD3D9DeviceCheck)
    ON_BN_CLICKED(IDC_FULLSCREEN_MONITOR_CHECK, OnFullscreenCheck)
    ON_UPDATE_COMMAND_UI(IDC_DSVMR9YUVMIXER, OnUpdateMixerYUV)
END_MESSAGE_MAP()

// CPPageOutput message handlers

BOOL CPPageOutput::OnInitDialog()
{
    __super::OnInitDialog();

    SetHandCursor(m_hWnd, IDC_AUDRND_COMBO);

    const CAppSettings& s = AfxGetAppSettings();
    const CRenderersSettings& renderersSettings = s.m_RenderersSettings;

    m_iDSVideoRendererType  = s.iDSVideoRendererType;
    m_iRMVideoRendererType  = s.iRMVideoRendererType;
    m_iQTVideoRendererType  = s.iQTVideoRendererType;
    m_iDX9Resizer           = renderersSettings.iDX9Resizer;
    m_fVMR9MixerYUV         = renderersSettings.fVMR9MixerYUV;
    m_fVMR9AlterativeVSync  = renderersSettings.fVMR9AlterativeVSync;
    m_fD3DFullscreen        = renderersSettings.bD3DFullscreen;
    m_iMixerBuffersBase     = renderersSettings.MixerBuffers - 4; // it has an offset
    m_dRefreshRateAdjust    = renderersSettings.dRefreshRateAdjust;

    m_iAudioRendererTypeCtrl.SetRedraw(FALSE);
    m_AudioRendererDisplayNames.Add(_T(""));
    m_iAudioRendererTypeCtrl.AddString(_T("1: ") + ResStr(IDS_PPAGE_OUTPUT_SYS_DEF));
    m_iAudioRendererType = 0;

    int i = 2;
    CString Cbstr;

    BeginEnumSysDev(CLSID_AudioRendererCategory, pMoniker) {
        LPOLESTR olestr = nullptr;
        if (FAILED(pMoniker->GetDisplayName(0, 0, &olestr))) {
            continue;
        }

        CStringW str(olestr);
        CoTaskMemFree(olestr);

        m_AudioRendererDisplayNames.Add(CString(str));

        CComPtr<IPropertyBag> pPB;
        if (SUCCEEDED(pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPB))) {
            CComVariant var;
            pPB->Read(CComBSTR(_T("FriendlyName")), &var, nullptr);

            CString fstr(var.bstrVal);

            var.Clear();
            if (SUCCEEDED(pPB->Read(CComBSTR(_T("FilterData")), &var, nullptr))) {
                BSTR* pbstr;
                if (SUCCEEDED(SafeArrayAccessData(var.parray, (void**)&pbstr))) {
                    fstr.Format(_T("%s (%08x)"), CString(fstr), *((DWORD*)pbstr + 1));
                    SafeArrayUnaccessData(var.parray);
                }
            }
            Cbstr.Format(_T("%d: %s"), i, fstr);
        } else {
            Cbstr.Format(_T("%d: %s"), i, CString(str));
        }
        m_iAudioRendererTypeCtrl.AddString(Cbstr);

        if (s.strAudioRendererDisplayName == str && m_iAudioRendererType == 0) {
            m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
        }
        i++;
    }
    EndEnumSysDev;

    Cbstr.Format(_T("%d: %s"), i++, AUDRNDT_NULL_COMP);
    m_AudioRendererDisplayNames.Add(AUDRNDT_NULL_COMP);
    m_iAudioRendererTypeCtrl.AddString(Cbstr);
    if (s.strAudioRendererDisplayName == AUDRNDT_NULL_COMP && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    }

    Cbstr.Format(_T("%d: %s"), i++, AUDRNDT_NULL_UNCOMP);
    m_AudioRendererDisplayNames.Add(AUDRNDT_NULL_UNCOMP);
    m_iAudioRendererTypeCtrl.AddString(Cbstr);
    if (s.strAudioRendererDisplayName == AUDRNDT_NULL_UNCOMP && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    }

    Cbstr.Format(_T("%d: %s"), i++, AUDRNDT_MPC);
    m_AudioRendererDisplayNames.Add(AUDRNDT_MPC);
    m_iAudioRendererTypeCtrl.AddString(Cbstr);
    if (s.strAudioRendererDisplayName == AUDRNDT_MPC && m_iAudioRendererType == 0) {
        m_iAudioRendererType = m_iAudioRendererTypeCtrl.GetCount() - 1;
    }

    CorrectComboListWidth(m_iAudioRendererTypeCtrl);
    m_iAudioRendererTypeCtrl.SetRedraw(TRUE);
    m_iAudioRendererTypeCtrl.Invalidate();
    m_iAudioRendererTypeCtrl.UpdateWindow();

    if (HINSTANCE hD3D9 = LoadLibrary(
#ifdef D3D_DEBUG_INFO
                              _T("d3d9d.dll")
#else
                              _T("d3d9.dll")
#endif
                          )) {
        typedef IDirect3D9* (WINAPI * Direct3DCreate9Ptr)(__in UINT SDKVersion);
        Direct3DCreate9Ptr pDirect3DCreate9 = reinterpret_cast<Direct3DCreate9Ptr>(GetProcAddress(hD3D9, "Direct3DCreate9"));

        IDirect3D9* pD3D = pDirect3DCreate9(D3D_SDK_VERSION);
        if (pD3D) {
            CString d3ddevice_str;
            __declspec(align(8)) D3DADAPTER_IDENTIFIER9 adapterIdentifier;

            INT i = pD3D->GetAdapterCount() - 1;
            while (i >= 0) {
                if (S_OK == pD3D->GetAdapterIdentifier(i, 0, &adapterIdentifier)) {
                    // The GUID is used multiple times, pre-load it to registers
                    __int64 i64ID0 = reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[0];
                    __int64 i64ID1 = reinterpret_cast<__int64*>(&adapterIdentifier.DeviceIdentifier)[1];
                    // only insert unique GUIDs
                    POSITION pos = m_VRendererDevices.GetHeadPosition();
                    while (pos) {
                        GUIDai const& current = m_VRendererDevices.GetNext(pos);
                        if ((current.g[0] == i64ID0) && (current.g[1] == i64ID1)) {
                            --i;
                            continue;
                        }
                    }

                    d3ddevice_str = adapterIdentifier.Description;
                    d3ddevice_str += _T(" - ");
                    d3ddevice_str += adapterIdentifier.DeviceName;
                    int iIndex = m_iD3D9RenderDeviceCtrl.AddString(d3ddevice_str);
                    pos = m_VRendererDevices.AddTail();
                    GUIDai& tail = m_VRendererDevices.GetAt(pos);
                    tail.i = iIndex;
                    tail.g[0] = i64ID0;
                    tail.g[1] = i64ID1;
                    if ((renderersSettings.GUIDVRendererDevice[0] == i64ID0)
                            && (renderersSettings.GUIDVRendererDevice[1] == i64ID1)) {
                        m_iD3D9RenderDevice = iIndex;
                    }
                }
                --i;
            }
            pD3D->Release();
        }
        BOOL b = FreeLibrary(hD3D9);
        ASSERT(b);
    }
    CorrectComboListWidth(m_iD3D9RenderDeviceCtrl);

    CComboBox& m_iDSVRTC = m_iDSVideoRendererTypeCtrl;
    m_iDSVRTC.SetRedraw(FALSE); // Do not draw the control while we are filling it with items
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_SYS_DEF)), IDC_DSSYSDEF);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_OLDRENDERER)), IDC_DSOLD);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_OVERLAYMIXER)), IDC_DSOVERLAYMIXER);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_VMR7WINDOWED)), IDC_DSVMR7WIN);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_VMR9WINDOWED)), IDC_DSVMR9WIN);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_VMR9RENDERLESS)), IDC_DSVMR9REN);
    if (IsCLSIDRegistered(CLSID_EnhancedVideoRenderer)) {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_EVR)), IDC_DSEVR);
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_EVR_CUSTOM)), IDC_DSEVR_CUSTOM);
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_SYNC)), IDC_DSSYNC);
    } else {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_EVR) + ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE)), IDC_DSEVR);
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_EVR_CUSTOM) + ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE)), IDC_DSEVR_CUSTOM);
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_SYNC) + ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE)), IDC_DSSYNC);
    }
    if (IsCLSIDRegistered(CLSID_DXR)) {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_DXR)), IDC_DSDXR);
    } else {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_DXR) + ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE)), IDC_DSDXR);
    }
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_NULL_COMP)), IDC_DSNULL_COMP);
    m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_NULL_UNCOMP)), IDC_DSNULL_UNCOMP);
    if (IsCLSIDRegistered(CLSID_madVR)) {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_MADVR)), IDC_DSMADVR);
    } else {
        m_iDSVRTC.SetItemData(m_iDSVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_MADVR) + ResStr(IDS_PPAGE_OUTPUT_UNAVAILABLE)), IDC_DSMADVR);
    }

    for (int i = 0; i < m_iDSVRTC.GetCount(); ++i) {
        if (m_iDSVideoRendererType == m_iDSVRTC.GetItemData(i)) {
            m_iDSVRTC.SetCurSel(i);
            break;
        }
    }

    m_iDSVRTC.SetRedraw(TRUE);
    m_iDSVRTC.Invalidate();
    m_iDSVRTC.UpdateWindow();

    CComboBox& m_iQTVRTC = m_iQTVideoRendererTypeCtrl;
    m_iQTVRTC.SetItemData(m_iQTVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_SYS_DEF)), VIDRNDT_QT_DEFAULT);
    m_iQTVRTC.SetItemData(m_iQTVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_VMR9RENDERLESS)), VIDRNDT_QT_DX9);
    m_iQTVRTC.SetCurSel(m_iQTVideoRendererType);
    CorrectComboListWidth(m_iQTVRTC);

    CComboBox& m_iRMVRTC = m_iRMVideoRendererTypeCtrl;
    m_iRMVideoRendererTypeCtrl.SetItemData(m_iRMVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_SYS_DEF)), VIDRNDT_RM_DEFAULT);
    m_iRMVRTC.SetItemData(m_iRMVRTC.AddString(ResStr(IDS_PPAGE_OUTPUT_VMR9RENDERLESS)), VIDRNDT_RM_DX9);
    m_iRMVRTC.SetCurSel(m_iRMVideoRendererType);
    CorrectComboListWidth(m_iRMVRTC);

    UpdateData(FALSE);

    m_tickcross.Create(16, 16, ILC_COLOR32, 2, 0);
    CMPCPngImage tickcross;
    tickcross.Load(IDF_TICKCROSS);
    m_tickcross.Add(&tickcross, (CBitmap*)nullptr);
    m_tick = m_tickcross.ExtractIcon(0);
    m_cross = m_tickcross.ExtractIcon(1);

    CreateToolTip();

    m_wndToolTip.AddTool(GetDlgItem(IDC_RMRND_COMBO), ResStr(IDC_RMSYSDEF));
    m_wndToolTip.AddTool(GetDlgItem(IDC_QTRND_COMBO), ResStr(IDC_QTSYSDEF));

    OnDSRendererChange();
    OnRMRendererChange();
    OnQTRendererChange();

    // YUV mixing is incompatible with Vista+
    if (SysVersion::IsVistaOrLater()) {
        GetDlgItem(IDC_DSVMR9YUVMIXER)->EnableWindow(FALSE);
    }

    CheckDlgButton(IDC_D3D9DEVICE, BST_CHECKED);
    GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(TRUE);
    GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(TRUE);

    if (((m_iDSVideoRendererType == IDC_DSVMR9REN) || (m_iDSVideoRendererType == IDC_DSEVR_CUSTOM) || (m_iDSVideoRendererType == IDC_DSSYNC)) && (m_iD3D9RenderDeviceCtrl.GetCount() > 1)) {
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(TRUE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(FALSE);
        CheckDlgButton(IDC_D3D9DEVICE, BST_UNCHECKED);
        if (m_iD3D9RenderDevice != -1) {
            CheckDlgButton(IDC_D3D9DEVICE, BST_CHECKED);
            GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(TRUE);
        }
    } else {
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(FALSE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(FALSE);
        if (m_iD3D9RenderDevice == -1) {
            CheckDlgButton(IDC_D3D9DEVICE, BST_UNCHECKED);
        }
    }
    UpdateData(TRUE);

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageOutput::OnApply()
{
    UpdateData();

    CAppSettings& s = AfxGetAppSettings();

    if (!IsRenderTypeAvailable(m_iDSVideoRendererType)) {
        ((CPropertySheet*)GetParent())->SetActivePage(this);
        AfxMessageBox(IDS_PPAGE_OUTPUT_UNAVAILABLEMSG, MB_ICONEXCLAMATION | MB_OK, 0);

        // revert to the renderer in the settings
        m_iDSVideoRendererTypeCtrl.SetCurSel(0);
        for (int i = 0; i < m_iDSVideoRendererTypeCtrl.GetCount(); ++i) {
            if (s.iDSVideoRendererType == m_iDSVideoRendererTypeCtrl.GetItemData(i)) {
                m_iDSVideoRendererTypeCtrl.SetCurSel(i);
                break;
            }
        }
        OnDSRendererChange();

        return FALSE;
    }

    CRenderersSettings& renderersSettings                   = s.m_RenderersSettings;
    s.iDSVideoRendererType                                  = m_iDSVideoRendererType;
    s.iRMVideoRendererType                                  = m_iRMVideoRendererType;
    s.iQTVideoRendererType                                  = m_iQTVideoRendererType;
    renderersSettings.iDX9Resizer                           = m_iDX9Resizer;
    renderersSettings.fVMR9MixerYUV                         = m_fVMR9MixerYUV;
    renderersSettings.fVMR9AlterativeVSync = m_fVMR9AlterativeVSync;
    s.strAudioRendererDisplayName                           = m_AudioRendererDisplayNames[m_iAudioRendererType];
    renderersSettings.bD3DFullscreen       = m_fD3DFullscreen;
    renderersSettings.MixerBuffers                          = m_iMixerBuffersBase + 4; // it has an offset
    renderersSettings.dRefreshRateAdjust   = m_dRefreshRateAdjust;
    ZeroMemory(renderersSettings.GUIDVRendererDevice, sizeof(renderersSettings.GUIDVRendererDevice));
    if (m_fD3D9RenderDevice && ((m_iDSVideoRendererType == IDC_DSVMR9REN) || (m_iDSVideoRendererType == IDC_DSEVR_CUSTOM) || (m_iDSVideoRendererType == IDC_DSSYNC))) {
        POSITION pos = m_VRendererDevices.GetHeadPosition();
        while (pos) {
            GUIDai& current = m_VRendererDevices.GetNext(pos);
            if (m_iD3D9RenderDevice == current.i) {
                memcpy(renderersSettings.GUIDVRendererDevice, current.g, sizeof(current.g));
            }
        }
    }

    return __super::OnApply();
}

void CPPageOutput::OnUpdateMixerYUV(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsDlgButtonChecked(IDC_DSVMR9REN) && !SysVersion::IsVistaOrLater());
}

void CPPageOutput::OnDSRendererChange()
{
    UpdateData();
    m_iDSDXVASupport.SetRedraw(FALSE);
    m_iDSSubtitleSupport.SetRedraw(FALSE);
    m_iDSSaveImageSupport.SetRedraw(FALSE);
    m_iDSShaderSupport.SetRedraw(FALSE);
    m_iDSRotationSupport.SetRedraw(FALSE);

    m_iDSVideoRendererType = (int)m_iDSVideoRendererTypeCtrl.GetItemData(m_iDSVideoRendererTypeCtrl.GetCurSel());
    // properties of the custom internal renderers
    if ((m_iDSVideoRendererType == IDC_DSVMR9REN) || (m_iDSVideoRendererType == IDC_DSEVR_CUSTOM) || (m_iDSVideoRendererType == IDC_DSSYNC)) {
        GetDlgItem(IDC_DX9RESIZER_COMBO)->EnableWindow(TRUE);
        GetDlgItem(IDC_FULLSCREEN_MONITOR_CHECK)->EnableWindow(TRUE);
        GetDlgItem(IDC_MIXERBUFFERS)->EnableWindow(TRUE);
        GetDlgItem(IDC_MIXERBUFFERS_TXT)->EnableWindow(TRUE);
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(TRUE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(TRUE);
        m_iDSRotationSupport.SetIcon(m_tick);
        m_iDSShaderSupport.SetIcon(m_tick);

        if (m_iD3D9RenderDeviceCtrl.GetCount() <= 1) {
            goto DisableD3D9RenderDeviceOptions;
        }
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(TRUE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(IsDlgButtonChecked(IDC_D3D9DEVICE));
    } else {
        GetDlgItem(IDC_DX9RESIZER_COMBO)->EnableWindow(FALSE);
        GetDlgItem(IDC_FULLSCREEN_MONITOR_CHECK)->EnableWindow(FALSE);
        GetDlgItem(IDC_MIXERBUFFERS)->EnableWindow(FALSE);
        GetDlgItem(IDC_MIXERBUFFERS_TXT)->EnableWindow(FALSE);
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(FALSE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(FALSE);
        m_iDSRotationSupport.SetIcon(m_cross);
        m_iDSShaderSupport.SetIcon(m_cross);
DisableD3D9RenderDeviceOptions:
        GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(FALSE);
        GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(FALSE);
    }

    GetDlgItem(IDC_DSVMR9YUVMIXER)->EnableWindow(FALSE);
    GetDlgItem(IDC_DSVMR9ALTERNATIVEVSYNC)->EnableWindow(FALSE);
    GetDlgItem(IDC_REFRESHRATEADJ)->EnableWindow(FALSE);
    GetDlgItem(IDC_REFRESHRATEADJ_TXT)->EnableWindow(FALSE);
    m_iDSDXVASupport.SetIcon(m_cross);
    m_iDSSubtitleSupport.SetIcon(m_cross);
    m_iDSSaveImageSupport.SetIcon(m_cross);
    m_iDSShaderSupport.SetIcon(m_cross);
    m_iDSRotationSupport.SetIcon(m_cross);

    m_wndToolTip.UpdateTipText(ResStr(IDC_VIDRND_COMBO), GetDlgItem(IDC_VIDRND_COMBO));

    switch (m_iDSVideoRendererType) {
        case IDC_DSSYSDEF:
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSSYSDEF), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSOLD:
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSOLD), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSOVERLAYMIXER:
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSOVERLAYMIXER), GetDlgItem(IDC_VIDRND_COMBO));
            if (!SysVersion::IsVistaOrLater()) {
                m_iDSDXVASupport.SetIcon(m_tick);
            }
            break;
        case IDC_DSVMR7WIN:
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSVMR7WIN), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSVMR9WIN:
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSVMR9WIN), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSEVR:
            if (SysVersion::IsVistaOrLater()) {
                m_iDSDXVASupport.SetIcon(m_tick);
            }
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSEVR), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSNULL_COMP:
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSNULL_COMP), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSNULL_UNCOMP:
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSNULL_UNCOMP), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSVMR9REN:
            GetDlgItem(IDC_DSVMR9YUVMIXER)->EnableWindow(TRUE);

            m_wndToolTip.UpdateTipText(ResStr(IDC_DSVMR9REN), GetDlgItem(IDC_VIDRND_COMBO));
        case IDC_DSEVR_CUSTOM:
            if (m_iD3D9RenderDeviceCtrl.GetCount() > 1) {
                GetDlgItem(IDC_D3D9DEVICE)->EnableWindow(TRUE);
                GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(IsDlgButtonChecked(IDC_D3D9DEVICE));
            }

            GetDlgItem(IDC_DSVMR9ALTERNATIVEVSYNC)->EnableWindow(TRUE);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSEVR_CUSTOM), GetDlgItem(IDC_VIDRND_COMBO));
            m_iDSSubtitleSupport.SetIcon(m_tick);
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_iDSShaderSupport.SetIcon(m_tick);
            m_iDSRotationSupport.SetIcon(m_tick);

            if (m_iDSVideoRendererType == IDC_DSEVR_CUSTOM) {
                GetDlgItem(IDC_REFRESHRATEADJ)->EnableWindow(TRUE);
                GetDlgItem(IDC_REFRESHRATEADJ_TXT)->EnableWindow(TRUE);
                if (SysVersion::IsVistaOrLater()) {
                    m_iDSDXVASupport.SetIcon(m_tick);
                }
            } else if (!SysVersion::IsVistaOrLater()) {
                m_iDSDXVASupport.SetIcon(m_tick);
            }
            break;
        case IDC_DSSYNC:
            if (SysVersion::IsVistaOrLater()) {
                m_iDSDXVASupport.SetIcon(m_tick);
            }
            m_iDSSubtitleSupport.SetIcon(m_tick);
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_iDSShaderSupport.SetIcon(m_tick);
            m_iDSRotationSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSSYNC), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSMADVR:
            m_iDSDXVASupport.SetIcon(m_tick);
            m_iDSSubtitleSupport.SetIcon(m_tick);
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_iDSShaderSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSMADVR), GetDlgItem(IDC_VIDRND_COMBO));
            break;
        case IDC_DSDXR:
            m_iDSSubtitleSupport.SetIcon(m_tick);
            m_iDSSaveImageSupport.SetIcon(m_tick);
            m_wndToolTip.UpdateTipText(ResStr(IDC_DSDXR), GetDlgItem(IDC_VIDRND_COMBO));
            break;
    }

    m_iDSDXVASupport.SetRedraw(TRUE);
    m_iDSDXVASupport.Invalidate();
    m_iDSDXVASupport.UpdateWindow();
    m_iDSSubtitleSupport.SetRedraw(TRUE);
    m_iDSSubtitleSupport.Invalidate();
    m_iDSSubtitleSupport.UpdateWindow();
    m_iDSSaveImageSupport.SetRedraw(TRUE);
    m_iDSSaveImageSupport.Invalidate();
    m_iDSSaveImageSupport.UpdateWindow();
    m_iDSShaderSupport.SetRedraw(TRUE);
    m_iDSShaderSupport.Invalidate();
    m_iDSShaderSupport.UpdateWindow();
    m_iDSRotationSupport.SetRedraw(TRUE);
    m_iDSRotationSupport.Invalidate();
    m_iDSRotationSupport.UpdateWindow();

    SetModified();
}

void CPPageOutput::OnRMRendererChange()
{
    UpdateData();

    switch (m_iRMVideoRendererType) {
        case VIDRNDT_RM_DEFAULT:
            m_iRMSaveImageSupport.SetIcon(m_cross);
            m_iRMSubtitleSupport.SetIcon(m_cross);

            m_wndToolTip.UpdateTipText(ResStr(IDC_RMSYSDEF), GetDlgItem(IDC_RMRND_COMBO));
            break;
        case VIDRNDT_RM_DX9:
            m_iRMSaveImageSupport.SetIcon(m_tick);
            m_iRMSaveImageSupport.SetIcon(m_tick);
            m_iRMSubtitleSupport.SetIcon(m_tick);

            m_wndToolTip.UpdateTipText(ResStr(IDC_RMDX9), GetDlgItem(IDC_RMRND_COMBO));
            break;
    }

    SetModified();
}

void CPPageOutput::OnQTRendererChange()
{
    UpdateData();

    switch (m_iQTVideoRendererType) {
        case VIDRNDT_QT_DEFAULT:
            m_iQTSaveImageSupport.SetIcon(m_cross);
            m_iQTSubtitleSupport.SetIcon(m_cross);

            m_wndToolTip.UpdateTipText(ResStr(IDC_QTSYSDEF), GetDlgItem(IDC_QTRND_COMBO));
            break;
        case VIDRNDT_QT_DX9:
            m_iQTSaveImageSupport.SetIcon(m_tick);
            m_iQTSaveImageSupport.SetIcon(m_tick);
            m_iQTSubtitleSupport.SetIcon(m_tick);

            m_wndToolTip.UpdateTipText(ResStr(IDC_QTDX9), GetDlgItem(IDC_QTRND_COMBO));
            break;
    }

    SetModified();
}

void CPPageOutput::OnFullscreenCheck()
{
    UpdateData();
    if (m_fD3DFullscreen &&
            (MessageBox(ResStr(IDS_D3DFS_WARNING), nullptr, MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDNO)) {
        m_fD3DFullscreen = false;
        UpdateData(FALSE);
    } else {
        SetModified();
    }
}

void CPPageOutput::OnD3D9DeviceCheck()
{
    UpdateData();
    GetDlgItem(IDC_D3D9DEVICE_COMBO)->EnableWindow(m_fD3D9RenderDevice);
    SetModified();
}

bool CPPageOutput::IsRenderTypeAvailable(int VideoRendererType)
{
    switch (m_iDSVideoRendererType) {
        case IDC_DSEVR:
        case IDC_DSEVR_CUSTOM:
        case IDC_DSSYNC:
            return IsCLSIDRegistered(CLSID_EnhancedVideoRenderer);
        case IDC_DSDXR:
            return IsCLSIDRegistered(CLSID_DXR);
        case IDC_DSMADVR:
            return IsCLSIDRegistered(CLSID_madVR);
        default:
            return true;
    }
}

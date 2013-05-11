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

#ifndef __AFXWIN_H__
#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include <afxadv.h>
#include <atlsync.h>
#include "FakeFilterMapper2.h"
#include "AppSettings.h"
#include <d3d9.h>
#include <vmr9.h>
#include <dxva2api.h> //#include <evr9.h>
#include <map>
#include <afxmt.h>

#define MPC_WND_CLASS_NAME L"MediaPlayerClassicW"

//define the default logo we use
#define DEF_LOGO IDF_LOGO3


enum {
    WM_GRAPHNOTIFY = WM_RESET_DEVICE + 1,
    WM_RESUMEFROMSTATE,
    WM_TUNER_SCAN_PROGRESS,
    WM_TUNER_SCAN_END,
    WM_TUNER_STATS,
    WM_TUNER_NEW_CHANNEL
};

#define WM_MYMOUSELAST WM_XBUTTONDBLCLK

///////////////

extern HICON LoadIcon(CString fn, bool fSmall);
extern bool LoadType(CString fn, CString& type);
extern bool LoadResource(UINT resid, CStringA& str, LPCTSTR restype);
extern CStringA GetContentType(CString fn, CAtlList<CString>* redir = NULL);
extern WORD AssignedToCmd(UINT keyOrMouseValue, bool bIsFullScreen = false, bool bCheckMouse = true);

typedef enum {
    ProcAmp_Brightness = 0x1,
    ProcAmp_Contrast   = 0x2,
    ProcAmp_Hue        = 0x4,
    ProcAmp_Saturation = 0x8,
    ProcAmp_All = ProcAmp_Brightness | ProcAmp_Contrast | ProcAmp_Hue | ProcAmp_Saturation
} ControlType;

typedef struct {
    DWORD dwProperty;
    int   MinValue;
    int   MaxValue;
    int   DefaultValue;
    int   StepSize;
} COLORPROPERTY_RANGE;

__inline DXVA2_Fixed32 IntToFixed(__in const int _int_, __in const short divisor = 1)
{
    // special converter that is resistant to MS bugs
    DXVA2_Fixed32 _fixed_;
    _fixed_.Value = SHORT(_int_ / divisor);
    _fixed_.Fraction = USHORT((_int_ % divisor * 0x10000 + divisor / 2) / divisor);
    return _fixed_;
}

__inline int FixedToInt(__in const DXVA2_Fixed32 _fixed_, __in const short factor = 1)
{
    // special converter that is resistant to MS bugs
    return (int)_fixed_.Value * factor + ((int)_fixed_.Fraction * factor + 0x8000) / 0x10000;
}

extern void GetCurDispMode(dispmode& dm, CString& DisplayName);
extern bool GetDispMode(int i, dispmode& dm, CString& DisplayName);
extern void SetDispMode(const dispmode& dm, CString& DisplayName);
extern void SetAudioRenderer(int AudioDevNo);

extern void SetHandCursor(HWND m_hWnd, UINT nID);

struct LanguageResource {
    const UINT resourceID;
    const LANGID localeID; // Check http://msdn.microsoft.com/en-us/goglobal/bb964664
    const LPCTSTR name;
    const LPCTSTR dllPath;
};

class CMPlayerCApp : public CWinApp
{
    HMODULE m_hNTDLL;

    ATL::CMutex m_mutexOneInstance;

    CAtlList<CString> m_cmdln;
    void PreProcessCommandLine();
    bool SendCommandLine(HWND hWnd);
    UINT GetVKFromAppCommand(UINT nAppCommand);

    COLORPROPERTY_RANGE     m_ColorControl[4];

    static UINT GetRemoteControlCodeMicrosoft(UINT nInputcode, HRAWINPUT hRawInput);
    static UINT GetRemoteControlCodeSRM7500(UINT nInputcode, HRAWINPUT hRawInput);

public:
    CMPlayerCApp();
    ~CMPlayerCApp();

    void ShowCmdlnSwitches() const;

    bool StoreSettingsToIni();
    bool StoreSettingsToRegistry();
    CString GetIniPath() const;
    bool IsIniValid() const;
    bool ChangeSettingsLocation(bool useIni);
    bool ExportSettings(CString savePath, CString subKey = _T(""));

private:
    struct CStringIgnoreCaseLess {
        bool operator()(const CString& str1, const CString& str2) const {
            return str1.CompareNoCase(str2) < 0;
        }
    };
    std::map<CString, std::map<CString, CString, CStringIgnoreCaseLess>, CStringIgnoreCaseLess> m_ProfileMap;
    bool m_fProfileInitialized;
    void InitProfile();
    ::CCriticalSection m_ProfileCriticalSection;
public:
    void FlushProfile();
    virtual BOOL GetProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE* ppData, UINT* pBytes);
    virtual UINT GetProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nDefault);
    virtual CString GetProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszDefault = NULL);
    virtual BOOL WriteProfileBinary(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPBYTE pData, UINT nBytes);
    virtual BOOL WriteProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue);
    virtual BOOL WriteProfileString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpszValue);

    bool GetAppSavePath(CString& path);

    bool m_fClosingState;
    CRenderersData m_Renderers;
    CString     m_strVersion;
    CString     m_AudioRendererDisplayName_CL;

    VMR9ProcAmpControlRange m_VMR9ColorControl[4];
    DXVA2_ValueRange        m_EVRColorControl[4];

    CAppSettings m_s;

    typedef UINT(*PTR_GetRemoteControlCode)(UINT nInputcode, HRAWINPUT hRawInput);

    PTR_GetRemoteControlCode    GetRemoteControlCode;
    COLORPROPERTY_RANGE*        GetColorControl(ControlType nFlag);
    void                        ResetColorControlRange();
    void                        UpdateColorControlRange(bool isEVR);

    static const LanguageResource languageResources[];
    static const size_t languageResourcesCount;

    static const LanguageResource& GetLanguageResourceByResourceID(UINT resourceID);
    static const LanguageResource& GetLanguageResourceByLocaleID(LANGID localeID);
    static bool SetLanguage(const LanguageResource& languageResource, bool showErrorMsg = true);
    static void SetDefaultLanguage();

    static void RunAsAdministrator(LPCTSTR strCommand, LPCTSTR strArgs, bool bWaitProcess);

    void RegisterHotkeys();
    void UnregisterHotkeys();
    // Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CMPlayerCApp)
public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    //}}AFX_VIRTUAL

    // Implementation

public:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnAppAbout();
    afx_msg void OnFileExit();
    afx_msg void OnHelpShowcommandlineswitches();
};

#define AfxGetMyApp() static_cast<CMPlayerCApp*>(AfxGetApp())
#define AfxGetAppSettings() static_cast<CMPlayerCApp*>(AfxGetApp())->m_s

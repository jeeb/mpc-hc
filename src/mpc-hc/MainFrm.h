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

#include <atlbase.h>

#include "FullscreenWnd.h"
#include "ChildView.h"
#include "PlayerSeekBar.h"
#include "PlayerToolBar.h"
#include "PlayerInfoBar.h"
#include "PlayerStatusBar.h"
#include "PlayerSubresyncBar.h"
#include "PlayerPlaylistBar.h"
#include "PlayerCaptureBar.h"
#include "PlayerNavigationBar.h"
#include "PlayerShaderEditorBar.h"
#include "EditListEditor.h"
#include "PPageSheet.h"
#include "PPageFileInfoSheet.h"
#include "FileDropTarget.h"
#include "KeyProvider.h"
#include "GraphThread.h"

#include "../SubPic/ISubPic.h"

#include "IGraphBuilder2.h"

#include "RealMediaGraph.h"
#ifndef _WIN64
// TODO: add QuickTime support for x64 when available!
#include "QuicktimeGraph.h"
#endif /* _WIN64 */
#include "ShockwaveGraph.h"

#include "IChapterInfo.h"
#include "IKeyFrameInfo.h"
#include "IBufferInfo.h"

#include "WebServer.h"
#include <d3d9.h>
#include <vmr9.h>
#include <evr.h>
#include <evr9.h>
#include <Il21dec.h>
#include "VMROSD.h"
#include "LcdSupport.h"
#include "MpcApi.h"
#include "../filters/renderer/SyncClock/SyncClock.h"
#include "sizecbar/scbarg.h"
#include "DSMPropertyBag.h"
#include "SkypeMoodMsgHandler.h"

enum {
    PM_NONE,
    PM_FILE,
    PM_DVD,
    PM_CAPTURE
};

interface __declspec(uuid("6E8D4A21-310C-11d0-B79A-00AA003767A7")) // IID_IAMLine21Decoder
IAMLine21Decoder_2 :
public IAMLine21Decoder {};

#define OpenMediaType_File 0
#define OpenMediaType_DVD 1
#define OpenMediaType_Device 2

class __declspec(novtable) OpenMediaData
{
public:
    __forceinline OpenMediaData(unsigned __int8 u8MediaType) : u8kMediaType(u8MediaType) {}
    virtual __forceinline ~OpenMediaData() {}
    CString title;
    CAtlList<CString> subs;
    unsigned __int8 const u8kMediaType;
};

class OpenFileData : public OpenMediaData
{
public:
    OpenFileData() : OpenMediaData(OpenMediaType_File), rtStart(0) {}
    CAtlList<CString> fns;
    REFERENCE_TIME rtStart;
};

class OpenDVDData : public OpenMediaData
{
public:
    OpenDVDData() : OpenMediaData(OpenMediaType_DVD) {}
    CString path;
    CComPtr<IDvdState> pDvdState;
};

class OpenDeviceData : public OpenMediaData
{
public:
    OpenDeviceData() : OpenMediaData(OpenMediaType_Device) {
        vinput = vchannel = ainput = -1;
    }
    CStringW DisplayName[2];
    int vinput, vchannel, ainput;
};

class TunerScanData
{
public:
    ULONG FrequencyStart;
    ULONG FrequencyStop;
    ULONG Bandwidth;
    LONG  Offset;
    HWND  Hwnd;
};

struct SubtitleInput {
    CComQIPtr<ISubStream> subStream;
    CComPtr<IBaseFilter> sourceFilter;

    SubtitleInput() {};
    SubtitleInput(CComQIPtr<ISubStream> subStream) : subStream(subStream) {};
    SubtitleInput(CComQIPtr<ISubStream> subStream, CComPtr<IBaseFilter> sourceFilter)
        : subStream(subStream), sourceFilter(sourceFilter) {};
};

interface ISubClock;

class CMainFrame : public CFrameWnd, public CDropTarget, public CFullscreenWindow
{
    enum {
        TIMER_STREAMPOSPOLLER = 1,
        TIMER_STREAMPOSPOLLER2,
        TIMER_FULLSCREENCONTROLBARHIDER,
        TIMER_FULLSCREENMOUSEHIDER,
        TIMER_STATS,
        TIMER_LEFTCLICK,
        TIMER_STATUSERASER,
        TIMER_DVBINFO_UPDATER
    };
    enum {
        SEEK_DIRECTION_NONE,
        SEEK_DIRECTION_BACKWARD,
        SEEK_DIRECTION_FORWARD
    };
    enum {
        ZOOM_DEFAULT_LEVEL = 0,
        ZOOM_AUTOFIT = -1,
        ZOOM_AUTOFIT_LARGER = -2
    };

    friend class CPPageFileInfoSheet;
    friend class CPPageLogo;
    friend class CSubtitleDlDlg;

    // TODO: wrap these graph objects into a class to make it look cleaner

    CComPtr<IGraphBuilder2> pGB;
    CComQIPtr<IMediaControl> pMC;
    CComQIPtr<IMediaEventEx> pME;
    CComQIPtr<IVideoWindow> pVW;
    CComQIPtr<IBasicVideo> pBV;
    CComQIPtr<IBasicAudio> pBA;
    CComQIPtr<IMediaSeeking> pMS;
    CComQIPtr<IVideoFrameStep> pFS;
    CComQIPtr<IQualProp, &IID_IQualProp> pQP;
    CComQIPtr<IBufferInfo> pBI;
    CComQIPtr<IAMOpenProgress> pAMOP;

    CComQIPtr<IDvdControl2> pDVDC;
    CComQIPtr<IDvdInfo2> pDVDI;

    CComPtr<ICaptureGraphBuilder2> pCGB;
    CStringW m_VidDispName, m_AudDispName;
    CComPtr<IBaseFilter> pVidCap, pAudCap;
    CComPtr<IAMVideoCompression> pAMVCCap, pAMVCPrev;
    CComPtr<IAMStreamConfig> pAMVSCCap, pAMVSCPrev, pAMASC;
    CComPtr<IAMCrossbar> pAMXBar;
    CComPtr<IAMTVTuner> pAMTuner;
    CComPtr<IAMDroppedFrames> pAMDF;

    CComPtr<CSubPicAllocatorPresenterImpl> m_pCAP;

    void SetVolumeBoost(UINT nAudioBoost);
    void SetBalance(int balance);

    // subtitles

    CCritSec m_csSubLock;

    CList<SubtitleInput> m_pSubStreams;
    POSITION m_posFirstExtSub;
    ISubStream* m_pCurrentSubStream;

    SubtitleInput* GetSubtitleInput(int& i, bool bIsOffset = false);

    friend class CTextPassThruFilter;

    // windowing

    CRect m_lastWindowRect;
    CPoint m_lastMouseMove;

    void ShowControls(int nCS, bool fSave = false);
    void SetUIPreset(int iCaptionMenuMode, UINT nCS);

    void SetDefaultWindowRect(int iMonitor = 0);
    void SetDefaultFullscreenState();
    void RestoreDefaultWindowRect();
    void ZoomVideoWindow(bool snap = true, double scale = ZOOM_DEFAULT_LEVEL);
    double GetZoomAutoFitScale(bool bLargerOnly = false) const;

    void SetAlwaysOnTop(int iOnTop);

    // dynamic menus

    void SetupOpenCDSubMenu();
    void SetupFiltersSubMenu();
    void SetupAudioSwitcherSubMenu();
    void SetupSubtitlesSubMenu();
    void SetupNavAudioSubMenu();
    void SetupNavSubtitleSubMenu();
    void SetupNavAngleSubMenu();
    void SetupNavChaptersSubMenu();
    void SetupFavoritesSubMenu();
    void SetupShadersSubMenu();
    void SetupRecentFilesSubMenu();
    void SetupLanguageMenu();

    IBaseFilter* FindSourceSelectableFilter();
    void SetupNavStreamSelectSubMenu(CMenu* pSub, UINT id, DWORD dwSelGroup);
    void OnNavStreamSelectSubMenu(UINT id, DWORD dwSelGroup);

    CMenu m_popupmain, m_popup;
    CMenu m_opencds;
    CMenu m_filters, m_subtitles, m_audios;
    CMenu m_language;
    CAutoPtrArray<CMenu> m_filterpopups;
    CMenu m_navangle;
    CMenu m_navchapters;
    CMenu m_favorites;
    CMenu m_shaders;
    CMenu m_recentfiles;

    CInterfaceArray<IUnknown, &IID_IUnknown> m_pparray;
    CInterfaceArray<IAMStreamSelect> m_ssarray;

    // chapters (file mode)
    CComPtr<IDSMChapterBag> m_pCB;
    void SetupChapters();

    // chapters (dvd mode)
    void SetupDVDChapters();

    void SetupIViAudReg();

    void AddTextPassThruFilter();

    int m_nLoops;
    UINT m_nLastSkipDirection;

    bool m_fCustomGraph;
    bool m_fRealMediaGraph, m_fShockwaveGraph, m_fQuicktimeGraph;

    CComPtr<ISubClock> m_pSubClock;

    int m_fFrameSteppingActive;
    int m_nStepForwardCount;
    REFERENCE_TIME m_rtStepForwardStart;
    int m_VolumeBeforeFrameStepping;

    bool m_fEndOfStream;

    LARGE_INTEGER m_liLastSaveTime;
    DWORD m_dwLastRun;

    bool m_fBuffering;

    bool m_fLiveWM;

    bool m_fUpdateInfoBar;

    void SendStatusMessage(CString msg, int nTimeOut);
    CString m_playingmsg, m_closingmsg;

    REFERENCE_TIME m_rtDurationOverride;

    CComPtr<IUnknown> m_pProv;

    void CleanGraph();

    CComPtr<IBaseFilter> pAudioDubSrc;

    void ShowOptions(int idPage = 0);

    bool GetDIB(void** ppData, bool bSilent);
    void SaveDIB(LPCTSTR fn, void* pData);
    BOOL IsRendererCompatibleWithSaveImage();
    void SaveImage(LPCTSTR fn);
    void SaveThumbnails(LPCTSTR fn);

    //

    friend class CWebClientSocket;
    friend class CWebServer;
    CAutoPtr<CWebServer> m_pWebServer;
    int m_iPlaybackMode;
    ULONG m_lCurrentChapter;
    ULONG m_lChapterStartTime;

    CAutoPtr<SkypeMoodMsgHandler> m_pSkypeMoodMsgHandler;
    void SendNowPlayingToSkype();

public:
    void StartWebServer(int nPort);
    void StopWebServer();

    CString GetStatusMessage() const;
    int GetPlaybackMode() const { return m_iPlaybackMode; }
    void SetPlaybackMode(int iNewStatus);
    bool IsMuted() { return m_wndToolBar.GetVolume() == -10000; }
    int GetVolume() { return m_wndToolBar.m_volctrl.GetPos(); }

public:
    CMainFrame();
    DECLARE_DYNAMIC(CMainFrame)

    // Attributes
public:
    bool m_fFullScreen;
    bool m_fFirstFSAfterLaunchOnFS;
    bool m_fHideCursor;
    CMenu m_navaudio, m_navsubtitle;

    CComPtr<IBaseFilter> m_pRefClock; // Adjustable reference clock. GothSync
    CComPtr<ISyncClock> m_pSyncClock;

    bool IsFrameLessWindow() const {
        return (m_fFullScreen || AfxGetAppSettings().iCaptionMenuMode == MODE_BORDERLESS);
    }
    bool IsCaptionHidden() const {//If no caption, there is no menu bar. But if is no menu bar, then the caption can be.
        return (!m_fFullScreen && AfxGetAppSettings().iCaptionMenuMode > MODE_HIDEMENU); //!=MODE_SHOWCAPTIONMENU && !=MODE_HIDEMENU
    }
    bool IsMenuHidden() const {
        return (!m_fFullScreen && AfxGetAppSettings().iCaptionMenuMode != MODE_SHOWCAPTIONMENU);
    }
    bool IsSomethingLoaded() const {
        return ((m_iMediaLoadState == MLS_LOADING || m_iMediaLoadState == MLS_LOADED));
    }
    bool IsPlaylistEmpty() {
        return (m_wndPlaylistBar.GetCount() == 0);
    }
    bool IsInteractiveVideo() const {
        return (AfxGetAppSettings().fIntRealMedia && m_fRealMediaGraph || m_fShockwaveGraph);
    }

    CControlBar* m_pLastBar;

protected:
    MPC_LOADSTATE m_iMediaLoadState;
    bool m_bFirstPlay;

    bool m_fAudioOnly;
    dispmode m_dmBeforeFullscreen;
    CString m_LastOpenFile, m_LastOpenBDPath;
    HMONITOR m_LastWindow_HM;

    DVD_DOMAIN m_iDVDDomain;
    DWORD m_iDVDTitle;
    double m_dSpeedRate;
    double m_ZoomX, m_ZoomY, m_PosX, m_PosY;
    int m_AngleX, m_AngleY, m_AngleZ;

    // Operations
    bool OpenMediaPrivate(CAutoPtr<OpenMediaData> pOMD);
    void CloseMediaPrivate();
    void DoTunerScan(TunerScanData* pTSD);

    CWnd* GetModalParent();

    void OpenCreateGraphObject(OpenMediaData* pOMD);
    void OpenFile(OpenFileData* pOFD);
    void OpenDVD(OpenDVDData* pODD);
    void OpenCapture(OpenDeviceData* pODD);
    HRESULT OpenBDAGraph();
    void OpenCustomizeGraph();
    void OpenSetupVideo();
    void OpenSetupAudio();
    void OpenSetupInfoBar();
    void OpenSetupStatsBar();
    void OpenSetupStatusBar();
    // void OpenSetupToolBar();
    void OpenSetupCaptureBar();
    void OpenSetupWindowTitle(CString fn = _T(""));
    void AutoChangeMonitorMode();
    double miFPS;

    bool GraphEventComplete();

    friend class CGraphThread;
    CGraphThread* m_pGraphThread;
    bool m_bOpenedThruThread;

    CAtlArray<REFERENCE_TIME> m_kfs;

    bool m_fOpeningAborted;
    bool m_bWasSnapped;

public:
    void OpenCurPlaylistItem(REFERENCE_TIME rtStart = 0);
    void OpenMedia(CAutoPtr<OpenMediaData> pOMD);
    void PlayFavoriteFile(CString fav);
    void PlayFavoriteDVD(CString fav);
    void ResetDevice();
    void CloseMedia();
    void StartTunerScan(CAutoPtr<TunerScanData> pTSD);
    void StopTunerScan();
    HRESULT SetChannel(int nChannel);

    void AddCurDevToPlaylist();

    bool m_fTrayIcon;
    void ShowTrayIcon(bool fShow);
    void SetTrayTip(CString str);

    CSize GetVideoSize() const;
    void ToggleFullscreen(bool fToNearest, bool fSwitchScreenResWhenHasTo);
    void MoveVideoWindow(bool fShowStats = false);
    void HideVideoWindow(bool fHide);

    OAFilterState GetMediaState() const;
    REFERENCE_TIME GetPos() const;
    REFERENCE_TIME GetDur() const;
    void SeekTo(REFERENCE_TIME rt, bool fSeekToKeyFrame = false);
    void SetPlayingRate(double rate);

    DWORD SetupAudioStreams();
    DWORD SetupSubtitleStreams();

    bool LoadSubtitle(CString fn, ISubStream** actualStream = NULL, bool bAutoLoad = false);
    bool SetSubtitle(int i, bool bIsOffset = false, bool bDisplayMessage = false, bool bApplyDefStyle = false);
    void SetSubtitle(ISubStream* pSubStream, bool bApplyDefStyle = false);
    void ToggleSubtitleOnOff(bool bDisplayMessage = false);
    void ReplaceSubtitle(ISubStream* pSubStreamOld, ISubStream* pSubStreamNew);
    void InvalidateSubtitlePic(DWORD_PTR nSubtitleId = -1, REFERENCE_TIME rtInvalidate = -1);
    void ReloadSubtitle();

    void SetAudioTrackIdx(int index);
    void SetSubtitleTrackIdx(int index);

    void AddFavorite(bool fDisplayMessage = false, bool fShowDialog = true);

    // shaders
    CAtlList<CString> m_shaderlabels[2];
    void SetShaders();
    void UpdateShaders(CString label);

    // capturing
    bool m_fCapturing;
    HRESULT BuildCapture(IPin* pPin, IBaseFilter* pBF[3], const GUID& majortype, AM_MEDIA_TYPE* pmt); // pBF: 0 buff, 1 enc, 2 mux, pmt is for 1 enc
    bool BuildToCapturePreviewPin(IBaseFilter* pVidCap, IPin** pVidCapPin, IPin** pVidPrevPin,
                                  IBaseFilter* pAudCap, IPin** pAudCapPin, IPin** pAudPrevPin);
    bool BuildGraphVideoAudio(int fVPreview, bool fVCapture, int fAPreview, bool fACapture);
    bool DoCapture(), StartCapture(), StopCapture();

    bool DoAfterPlaybackEvent();
    void ParseDirs(CAtlList<CString>& sl);
    bool SearchInDir(bool bDirForward, bool bLoop = false);

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
    virtual void RecalcLayout(BOOL bNotify = TRUE);

    // DVB capture
    void ShowCurrentChannelInfo(bool fShowOSD = true, bool fShowInfoBar = false);

    // Implementation
public:
    virtual ~CMainFrame();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:  // control bar embedded members

    CChildView m_wndView;

    UINT m_nCS;
    CPlayerSeekBar m_wndSeekBar;
    CPlayerToolBar m_wndToolBar;
    CPlayerInfoBar m_wndInfoBar;
    CPlayerInfoBar m_wndStatsBar;
    CPlayerStatusBar m_wndStatusBar;
    CList<CControlBar*> m_bars;

    CPlayerSubresyncBar m_wndSubresyncBar;
    CPlayerPlaylistBar m_wndPlaylistBar;
    CPlayerCaptureBar m_wndCaptureBar;
    CPlayerNavigationBar m_wndNavigationBar;
    CPlayerShaderEditorBar m_wndShaderEditorBar;
    CEditListEditor m_wndEditListEditor;
    CList<CPlayerBar*> m_dockingbars;

    CFileDropTarget m_fileDropTarget;
    // TODO
    DROPEFFECT OnDragEnter(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    DROPEFFECT OnDragOver(COleDataObject* pDataObject, DWORD dwKeyState, CPoint point);
    BOOL OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
    DROPEFFECT OnDropEx(COleDataObject* pDataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point);
    void OnDragLeave();
    DROPEFFECT OnDragScroll(DWORD dwKeyState, CPoint point);

    LPCTSTR GetRecentFile() const;

    friend class CPPagePlayback; // TODO
    friend class CPPageAudioSwitcher; // TODO
    friend class CMPlayerCApp; // TODO

    void RestoreControlBars();
    void SaveControlBars();

    // Generated message map functions

    DECLARE_MESSAGE_MAP()

public:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();

    afx_msg LRESULT OnTaskBarRestart(WPARAM, LPARAM);
    afx_msg LRESULT OnNotifyIcon(WPARAM, LPARAM);
    afx_msg LRESULT OnTaskBarThumbnailsCreate(WPARAM, LPARAM);

    afx_msg LRESULT OnSkypeAttach(WPARAM wParam, LPARAM lParam);

    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnMove(int x, int y);
    afx_msg void OnMoving(UINT fwSide, LPRECT pRect);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSizing(UINT fwSide, LPRECT pRect);

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
    afx_msg LRESULT OnAppCommand(WPARAM wParam, LPARAM lParam);
    afx_msg void OnRawInput(UINT nInputcode, HRAWINPUT hRawInput);

    afx_msg LRESULT OnHotKey(WPARAM wParam, LPARAM lParam);

    afx_msg void OnTimer(UINT_PTR nIDEvent);

    afx_msg LRESULT OnGraphNotify(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnResetDevice(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnResumeFromState(WPARAM wParam, LPARAM lParam);

    BOOL OnButton(UINT id, UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
    afx_msg LRESULT OnXButtonDown(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnXButtonUp(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnXButtonDblClk(WPARAM wParam, LPARAM lParam);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);

    afx_msg LRESULT OnNcHitTest(CPoint point);

    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

    afx_msg void OnInitMenu(CMenu* pMenu);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnUnInitMenuPopup(CMenu* pPopupMenu, UINT nFlags);

    BOOL OnMenu(CMenu* pMenu);
    afx_msg void OnMenuPlayerShort();
    afx_msg void OnMenuPlayerLong();
    afx_msg void OnMenuFilters();

    afx_msg void OnUpdatePlayerStatus(CCmdUI* pCmdUI);

    afx_msg void OnFilePostOpenmedia();
    afx_msg void OnUpdateFilePostOpenmedia(CCmdUI* pCmdUI);
    afx_msg void OnFilePostClosemedia();
    afx_msg void OnUpdateFilePostClosemedia(CCmdUI* pCmdUI);

    afx_msg void OnBossKey();

    afx_msg void OnStreamAudio(UINT nID);
    afx_msg void OnStreamSub(UINT nID);
    afx_msg void OnStreamSubOnOff();
    afx_msg void OnOgmAudio(UINT nID);
    afx_msg void OnOgmSub(UINT nID);
    afx_msg void OnDvdAngle(UINT nID);
    afx_msg void OnDvdAudio(UINT nID);
    afx_msg void OnDvdSub(UINT nID);
    afx_msg void OnDvdSubOnOff();


    // menu item handlers

    afx_msg void OnFileOpenQuick();
    afx_msg void OnFileOpenmedia();
    afx_msg void OnUpdateFileOpen(CCmdUI* pCmdUI);
    afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
    afx_msg void OnFileOpendvd();
    afx_msg void OnFileOpendevice();
    afx_msg void OnFileOpenCD(UINT nID);
    afx_msg void OnFileReopen();
    afx_msg void OnDropFiles(HDROP hDropInfo); // no menu item
    afx_msg void OnFileSaveAs();
    afx_msg void OnUpdateFileSaveAs(CCmdUI* pCmdUI);
    afx_msg void OnFileSaveImage();
    afx_msg void OnFileSaveImageAuto();
    afx_msg void OnUpdateFileSaveImage(CCmdUI* pCmdUI);
    afx_msg void OnFileSaveThumbnails();
    afx_msg void OnUpdateFileSaveThumbnails(CCmdUI* pCmdUI);
    afx_msg void OnFileLoadsubtitle();
    afx_msg void OnUpdateFileLoadsubtitle(CCmdUI* pCmdUI);
    afx_msg void OnFileSavesubtitle();
    afx_msg void OnUpdateFileSavesubtitle(CCmdUI* pCmdUI);
    afx_msg void OnFileISDBSearch();
    afx_msg void OnUpdateFileISDBSearch(CCmdUI* pCmdUI);
    afx_msg void OnFileISDBUpload();
    afx_msg void OnUpdateFileISDBUpload(CCmdUI* pCmdUI);
    afx_msg void OnFileISDBDownload();
    afx_msg void OnUpdateFileISDBDownload(CCmdUI* pCmdUI);
    afx_msg void OnFileProperties();
    afx_msg void OnUpdateFileProperties(CCmdUI* pCmdUI);
    afx_msg void OnFileClosePlaylist();
    afx_msg void OnFileCloseMedia(); // no menu item
    afx_msg void OnUpdateFileClose(CCmdUI* pCmdUI);

    afx_msg void OnViewCaptionmenu();
    afx_msg void OnViewNavigation();
    afx_msg void OnUpdateViewCaptionmenu(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewNavigation(CCmdUI* pCmdUI);
    afx_msg void OnViewControlBar(UINT nID);
    afx_msg void OnUpdateViewControlBar(CCmdUI* pCmdUI);
    afx_msg void OnViewSubresync();
    afx_msg void OnUpdateViewSubresync(CCmdUI* pCmdUI);
    afx_msg void OnViewPlaylist();
    afx_msg void OnUpdateViewPlaylist(CCmdUI* pCmdUI);
    afx_msg void OnViewEditListEditor();
    afx_msg void OnEDLIn();
    afx_msg void OnUpdateEDLIn(CCmdUI* pCmdUI);
    afx_msg void OnEDLOut();
    afx_msg void OnUpdateEDLOut(CCmdUI* pCmdUI);
    afx_msg void OnEDLNewClip();
    afx_msg void OnUpdateEDLNewClip(CCmdUI* pCmdUI);
    afx_msg void OnEDLSave();
    afx_msg void OnUpdateEDLSave(CCmdUI* pCmdUI);
    afx_msg void OnViewCapture();
    afx_msg void OnUpdateViewCapture(CCmdUI* pCmdUI);
    afx_msg void OnViewShaderEditor();
    afx_msg void OnUpdateViewShaderEditor(CCmdUI* pCmdUI);
    afx_msg void OnViewMinimal();
    afx_msg void OnUpdateViewMinimal(CCmdUI* pCmdUI);
    afx_msg void OnViewCompact();
    afx_msg void OnUpdateViewCompact(CCmdUI* pCmdUI);
    afx_msg void OnViewNormal();
    afx_msg void OnUpdateViewNormal(CCmdUI* pCmdUI);
    afx_msg void OnViewFullscreen();
    afx_msg void OnViewFullscreenSecondary();
    afx_msg void OnUpdateViewFullscreen(CCmdUI* pCmdUI);
    afx_msg void OnViewZoom(UINT nID);
    afx_msg void OnUpdateViewZoom(CCmdUI* pCmdUI);
    afx_msg void OnViewZoomAutoFit();
    afx_msg void OnViewZoomAutoFitLarger();
    afx_msg void OnViewDefaultVideoFrame(UINT nID);
    afx_msg void OnUpdateViewDefaultVideoFrame(CCmdUI* pCmdUI);
    afx_msg void OnViewSwitchVideoFrame();
    afx_msg void OnUpdateViewSwitchVideoFrame(CCmdUI* pCmdUI);
    afx_msg void OnViewKeepaspectratio();
    afx_msg void OnUpdateViewKeepaspectratio(CCmdUI* pCmdUI);
    afx_msg void OnViewCompMonDeskARDiff();
    afx_msg void OnUpdateViewCompMonDeskARDiff(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScan(UINT nID);
    afx_msg void OnUpdateViewPanNScan(CCmdUI* pCmdUI);
    afx_msg void OnViewPanNScanPresets(UINT nID);
    afx_msg void OnUpdateViewPanNScanPresets(CCmdUI* pCmdUI);
    afx_msg void OnViewRotate(UINT nID);
    afx_msg void OnUpdateViewRotate(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatio(UINT nID);
    afx_msg void OnUpdateViewAspectRatio(CCmdUI* pCmdUI);
    afx_msg void OnViewAspectRatioNext();
    afx_msg void OnViewOntop(UINT nID);
    afx_msg void OnUpdateViewOntop(CCmdUI* pCmdUI);
    afx_msg void OnViewOptions();
    afx_msg void OnUpdateViewTearingTest(CCmdUI* pCmdUI);
    afx_msg void OnViewTearingTest();
    afx_msg void OnUpdateViewDisplayStats(CCmdUI* pCmdUI);
    afx_msg void OnViewResetStats();
    afx_msg void OnViewDisplayStatsSC();
    afx_msg void OnUpdateViewVSyncOffset(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewFlushGPU(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewSynchronizeVideo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewSynchronizeDisplay(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewSynchronizeNearest(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewD3DFullscreen(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewAlternativeScheduler(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewDisableDesktopComposition(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewAlternativeVSync(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewColorManagementEnable(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementAmbientLight(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementIntent(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementWpAdaptState(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementLookupQuality(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewColorManagementBPC(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewDitheringLevels(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewDitheringTestEnable(CCmdUI* pCmdUI);

    afx_msg void OnUpdateViewFrameInterpolation(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewHighColorResolution(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewDisableInitialColorMixing(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewChromaFix(CCmdUI* pCmdUI);
    afx_msg void OnUpdateView32BitFPSurfaces(CCmdUI* pCmdUI);
    afx_msg void OnUpdateView16BitFPSurfaces(CCmdUI* pCmdUI);
    afx_msg void OnUpdateView10BitUISurfaces(CCmdUI* pCmdUI);
    afx_msg void OnUpdateView8BitUISurfaces(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewEnableFrameTimeCorrection(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncOffsetIncrease(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewVSyncOffsetDecrease(CCmdUI* pCmdUI);

    afx_msg void OnViewSynchronizeVideo();
    afx_msg void OnViewSynchronizeDisplay();
    afx_msg void OnViewSynchronizeNearest();

    afx_msg void OnViewColorManagementEnable(UINT nID);
    afx_msg void OnViewColorManagementInputAuto();
    afx_msg void OnViewColorManagementInputHDTV();
    afx_msg void OnViewColorManagementInputSDTV_NTSC();
    afx_msg void OnViewColorManagementInputSDTV_PAL();
    afx_msg void OnViewColorManagementAmbientLightBright();
    afx_msg void OnViewColorManagementAmbientLightOffice();
    afx_msg void OnViewColorManagementAmbientLightDim();
    afx_msg void OnViewColorManagementAmbientLightDark();
    afx_msg void OnViewColorManagementAmbientLightBypasslinear();
    afx_msg void OnViewColorManagementIntentPerceptual();
    afx_msg void OnViewColorManagementIntentRelativeColorimetric();
    afx_msg void OnViewColorManagementIntentSaturation();
    afx_msg void OnViewColorManagementIntentAbsoluteColorimetric();
    afx_msg void OnViewColorManagementTrcTypePurePower();
    afx_msg void OnViewColorManagementTrcTypeInverse();
    afx_msg void OnViewColorManagementWpAdaptStateFull();
    afx_msg void OnViewColorManagementWpAdaptStateMedium();
    afx_msg void OnViewColorManagementWpAdaptStateNone();
    afx_msg void OnViewColorManagementLookupQuality(UINT nID);
    afx_msg void OnViewColorManagementBPC();

    afx_msg void OnViewDitheringLevels(UINT nID);
    afx_msg void OnViewDitheringTestEnable();

    afx_msg void OnViewFlushGPUBeforeVSync();
    afx_msg void OnViewFlushGPUWait();

    afx_msg void OnViewD3DFullScreen();
    afx_msg void OnViewDisableDesktopComposition();
    afx_msg void OnViewAlternativeVSync();
    afx_msg void OnViewResetDefault();
    afx_msg void OnViewResetOptimal();

    afx_msg void OnViewFrameInterpolation(UINT nID);
    afx_msg void OnViewHighColorResolution();
    afx_msg void OnViewDisableInitialColorMixing();
    afx_msg void OnViewChromaFix(UINT nID);
    afx_msg void OnViewSurfacesQuality(UINT nID);
    afx_msg void OnViewAlternativeScheduler();
    afx_msg void OnViewEnableFrameTimeCorrection();
    afx_msg void OnViewVSyncOffsetIncrease();
    afx_msg void OnViewVSyncOffsetDecrease();
    afx_msg void OnUpdateShaderToggle(CCmdUI* pCmdUI);
    afx_msg void OnUpdateShaderToggleScreenSpace(CCmdUI* pCmdUI);
    afx_msg void OnShaderToggle();
    afx_msg void OnShaderToggleScreenSpace();
    afx_msg void OnUpdateViewRemainingTime(CCmdUI* pCmdUI);
    afx_msg void OnViewRemainingTime();
    afx_msg void OnD3DFullscreenToggle();
    afx_msg void OnGotoSubtitle(UINT nID);
    afx_msg void OnShiftSubtitle(UINT nID);
    afx_msg void OnSubtitleDelay(UINT nID);

    afx_msg void OnPlayPlay();
    afx_msg void OnPlayPause();
    afx_msg void OnPlayPauseI();
    afx_msg void OnPlayPlaypause();
    afx_msg void OnApiPlay();
    afx_msg void OnApiPause();
    afx_msg void OnPlayStop();
    afx_msg void OnUpdatePlayPauseStop(CCmdUI* pCmdUI);
    afx_msg void OnPlayFramestep(UINT nID);
    afx_msg void OnUpdatePlayFramestep(CCmdUI* pCmdUI);
    afx_msg void OnPlaySeek(UINT nID);
    afx_msg void OnPlaySeekKey(UINT nID); // no menu item
    afx_msg void OnUpdatePlaySeek(CCmdUI* pCmdUI);
    afx_msg void OnPlayGoto();
    afx_msg void OnUpdateGoto(CCmdUI* pCmdUI);
    afx_msg void OnPlayChangeRate(UINT nID);
    afx_msg void OnUpdatePlayChangeRate(CCmdUI* pCmdUI);
    afx_msg void OnPlayResetRate();
    afx_msg void OnUpdatePlayResetRate(CCmdUI* pCmdUI);
    afx_msg void OnPlayChangeAudDelay(UINT nID);
    afx_msg void OnUpdatePlayChangeAudDelay(CCmdUI* pCmdUI);
    afx_msg void OnPlayFilters(UINT nID);
    afx_msg void OnUpdatePlayFilters(CCmdUI* pCmdUI);
    afx_msg void OnPlayShaders(UINT nID);
    afx_msg void OnPlayAudio(UINT nID);
    afx_msg void OnPlaySubtitles(UINT nID);
    afx_msg void OnPlayFiltersStreams(UINT nID);
    afx_msg void OnPlayVolume(UINT nID);
    afx_msg void OnPlayVolumeBoost(UINT nID);
    afx_msg void OnUpdatePlayVolumeBoost(CCmdUI* pCmdUI);
    afx_msg void OnCustomChannelMapping();
    afx_msg void OnUpdateCustomChannelMapping(CCmdUI* pCmdUI);
    afx_msg void OnNormalizeRegainVolume(UINT nID);
    afx_msg void OnUpdateNormalizeRegainVolume(CCmdUI* pCmdUI);
    afx_msg void OnPlayColor(UINT nID);
    afx_msg void OnAfterplayback(UINT nID);
    afx_msg void OnUpdateAfterplayback(CCmdUI* pCmdUI);

    afx_msg void OnNavigateSkip(UINT nID);
    afx_msg void OnUpdateNavigateSkip(CCmdUI* pCmdUI);
    afx_msg void OnNavigateSkipFile(UINT nID);
    afx_msg void OnUpdateNavigateSkipFile(CCmdUI* pCmdUI);
    afx_msg void OnNavigateMenu(UINT nID);
    afx_msg void OnUpdateNavigateMenu(CCmdUI* pCmdUI);
    afx_msg void OnNavigateAudio(UINT nID);
    afx_msg void OnNavigateSubpic(UINT nID);
    afx_msg void OnNavigateAngle(UINT nID);
    afx_msg void OnNavigateChapters(UINT nID);
    afx_msg void OnNavigateMenuItem(UINT nID);
    afx_msg void OnUpdateNavigateMenuItem(CCmdUI* pCmdUI);
    afx_msg void OnTunerScan();
    afx_msg void OnUpdateTunerScan(CCmdUI* pCmdUI);

    afx_msg void OnFavoritesAdd();
    afx_msg void OnUpdateFavoritesAdd(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesQuickAddFavorite();
    afx_msg void OnFavoritesOrganize();
    afx_msg void OnUpdateFavoritesOrganize(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesFile(UINT nID);
    afx_msg void OnUpdateFavoritesFile(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDVD(UINT nID);
    afx_msg void OnUpdateFavoritesDVD(CCmdUI* pCmdUI);
    afx_msg void OnFavoritesDevice(UINT nID);
    afx_msg void OnUpdateFavoritesDevice(CCmdUI* pCmdUI);
    afx_msg void OnRecentFileClear();
    afx_msg void OnUpdateRecentFileClear(CCmdUI* pCmdUI);
    afx_msg void OnRecentFile(UINT nID);
    afx_msg void OnUpdateRecentFile(CCmdUI* pCmdUI);

    afx_msg void OnHelpHomepage();
    afx_msg void OnHelpCheckForUpdate();
    afx_msg void OnHelpToolbarImages();
    afx_msg void OnHelpDonate();

    afx_msg void OnClose();

    afx_msg void OnLanguage(UINT nID);
    afx_msg void OnUpdateLanguage(CCmdUI* pCmdUI);

    CMPC_Lcd m_Lcd;

    // ==== Added by CASIMIR666
    HWND            m_hVideoWnd;// Current Video window, can be either a copy of CFullscreenWindow::m_hFullscreenWnd or m_wndView.m_hWnd
    SIZE            m_fullWndSize;
    RECT            m_arcRendererWindowAndVideoArea[2];// used as a pair, these are written and used by MoveVideoWindow()
    CComPtr<IVMRMixerControl9>      m_pMC;
    CComPtr<IMFVideoDisplayControl> m_pMFVDC;
    CComPtr<IMFVideoProcessor>      m_pMFVP;
    CComPtr<IAMLine21Decoder_2>     m_pLN21;
    CVMROSD     m_OSD;
    ATOM        m_u16FullscreenWindowClassAtom;
    bool        m_bRemainingTime;
    int         m_nCurSubtitle;
    long        m_lSubtitleShift;
    __int64     m_rtCurSubPos;
    CString     m_strTitle;
    bool        m_bToggleShader[2];
    bool        m_bInOptions;
    bool        m_bStopTunerScan;
    bool        m_bLockedZoomVideoWindow;
    int         m_nLockedZoomVideoWindow;

    void        SetLoadState(MPC_LOADSTATE iState);
    void        SetPlayState(MPC_PLAYSTATE iState);
    HWND        CreateFullScreenWindow();
    void        SetupEVRColorControl();
    void        SetupVMR9ColorControl();
    void        SetColorControl(DWORD flags, int& brightness, int& contrast, int& hue, int& saturation);
    void        SetClosedCaptions(bool enable);
    LPCTSTR     GetDVDAudioFormatName(DVD_AudioAttributes& ATR) const;
    void        SetAudioDelay(REFERENCE_TIME rtShift);
    void        SetSubtitleDelay(__in const REFERENCE_TIME delay_ms);
    //void      AutoSelectTracks();
    bool        IsRealEngineCompatible(CString strFilename) const;
    void        SetTimersPlay();
    void        KillTimersStop();


    // MPC API functions
    void        ProcessAPICommand(COPYDATASTRUCT* pCDS);
    void        SendAPICommand(MPCAPI_COMMAND nCommand, LPCWSTR fmt, ...);
    void        SendNowPlayingToApi();
    void        SendSubtitleTracksToApi();
    void        SendAudioTracksToApi();
    void        SendPlaylistToApi();
    afx_msg void OnFileOpendirectory();

    void        SendCurrentPositionToApi(bool fNotifySeek = false);
    void        ShowOSDCustomMessageApi(MPC_OSDDATA* osdData);
    void        JumpOfNSeconds(int seconds);

    CString GetVidPos() const;

    ITaskbarList3* m_pTaskbarList;
    HRESULT CreateThumbnailToolbar();
    void UpdateThumbarButton();
    HRESULT UpdateThumbnailClip();

protected:
    // GDI+
    ULONG_PTR m_gdiplusToken;
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    void WTSRegisterSessionNotification();
    void WTSUnRegisterSessionNotification();

    DWORD m_nMenuHideTick;
    UINT m_nSeekDirection;
public:
    afx_msg UINT OnPowerBroadcast(UINT nPowerEvent, UINT nEventData);
    afx_msg void OnSessionChange(UINT nSessionState, UINT nId);

    void EnableShaders1(bool enable);
    void EnableShaders2(bool enable);

    CAtlList<CHdmvClipInfo::PlaylistItem> m_MPLSPlaylist;
    bool m_bIsBDPlay;
    bool OpenBD(CString Path);
};

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

enum {
    WM_RESET_DEVICE = WM_APP + 1
};

enum {
    VIDRNDT_RM_DEFAULT = 0,
    VIDRNDT_RM_DX9 = 1
};

enum {
    VIDRNDT_QT_DEFAULT = 0,
    VIDRNDT_QT_DX9 = 1
};

#define AMBIENT_LIGHT_BRIGHT 0
#define AMBIENT_LIGHT_OFFICE 1
#define AMBIENT_LIGHT_DIM 2
#define AMBIENT_LIGHT_DARK 3
#define WPADAPT_STATE_NONE 0
#define WPADAPT_STATE_MEDIUM 1
#define WPADAPT_STATE_FULL 2

struct Shader {
    CString label;
    CString target;
    CString srcdata;
};

struct CRenderersSettings {
    __declspec(nothrow noalias) void UpdateData(bool bSave);
    __declspec(nothrow noalias) void SetDefault();
    __declspec(nothrow noalias) void SetOptimal();

    // EVR Sync-specific settings
    double fCycleDelta;
    double fTargetSyncOffset;
    double fControlLimit;
    __int32 iLineDelta;
    __int32 iColumnDelta;
    bool bSynchronizeVideo;
    bool bSynchronizeDisplay;
    bool bSynchronizeNearest;

    // general settings
    bool fVMR9AlterativeVSync;
    __int32 iVMR9VSyncOffset;

    bool iVMRFlushGPUBeforeVSync;
    bool iVMRFlushGPUWait;

    bool bD3DFullscreen;
    bool iVMR9HighColorResolution;
    bool iVMR9DisableInitialColorMixing;

    bool iVMRDisableDesktopComposition;
    bool iEVRAlternativeScheduler;
    bool iEVREnableFrameTimeCorrection;

    double dRefreshRateAdjust;
    __int64 GUIDVRendererDevice[2];

    unsigned __int32 iVMR9ColorManagementLookupQuality;
    unsigned __int8 iVMR9ColorManagementAmbientLight;
    unsigned __int8 iVMR9ColorManagementIntent;
    unsigned __int8 iVMR9ColorManagementWpAdaptState;
    unsigned __int8 iVMR9ColorManagementBPC;
    unsigned __int8 iVMR9ColorManagementEnable;
    unsigned __int8 iVMR9ChromaFix;
    unsigned __int8 iVMR9SurfacesQuality;

    unsigned __int8 iVMR9DitheringTestEnable;
    unsigned __int8 iVMR9DitheringLevels;
    unsigned __int8 iVMR9FrameInterpolation;

    bool fVMR9MixerYUV;
    unsigned __int8 MixerBuffers;

    DWORD dwBPresetCustomColors[16];
    DWORD dwBackgoundColor;

    bool fSPCAllowAnimationWhenBuffering;
    bool bPositionRelative;
    unsigned __int8 nSPCMaxRes;
    unsigned __int8 nSPCSize;

    unsigned __int8 iDX9Resizer;
};

struct CRenderersData {
    DWORD m_dwPCIVendor;
    unsigned __int8 m_fDisplayStats;
    bool m_bResetStats;// set to reset the presentation statistics
    bool m_fTearingTest;

    // Hardware feature support
    bool m_bFP32Support;// note: m_bFP32Support is only 32-bit floating point on the mixer output, and it's 16-bit normalized unsigned integer for the rest of the renderer pipeline (this format is only supported on cards that also support 16- and 32-bit floating point surfaces)
    bool m_bFP16Support;
    bool m_bUI10Support;

    __declspec(nothrow noalias) __forceinline CRenderersData()
        : m_dwPCIVendor(0x1002)// temporary, to enable the item in the main menu
        , m_fDisplayStats(0)
        , m_bResetStats(false)
        , m_fTearingTest(false)
        , m_bFP32Support(true)// don't disable hardware features before initializing a renderer
        , m_bFP16Support(true)
        , m_bUI10Support(true) {}
};

extern CRenderersData* GetRenderersData();
extern CRenderersSettings const* GetRenderersSettings();
extern "C" __declspec(nothrow noalias) __int64 PerfCounter100ns();// external asm file for x64

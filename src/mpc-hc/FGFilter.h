/*
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

#pragma once

#define MERIT64(merit)      (((UINT64)(merit)) << 16)
#define MERIT64_DO_NOT_USE  MERIT64(MERIT_DO_NOT_USE)
#define MERIT64_DO_USE      MERIT64(MERIT_DO_NOT_USE + 1)
#define MERIT64_UNLIKELY    (MERIT64(MERIT_UNLIKELY))
#define MERIT64_NORMAL      (MERIT64(MERIT_NORMAL))
#define MERIT64_PREFERRED   (MERIT64(MERIT_PREFERRED))
#define MERIT64_ABOVE_DSHOW (MERIT64(1) << 32)

#include "../filters/renderer/VideoRenderers/VMR9AllocatorPresenter.h"
// {4E4834FA-22C2-40E2-9446-F77DD05D245E}
DEFINE_GUID(CLSID_VMR9AllocatorPresenter,
            0x4E4834FA, 0x22C2, 0x40E2, 0x94, 0x46, 0xF7, 0x7D, 0xD0, 0x5D, 0x24, 0x5E);

#include "../filters/renderer/VideoRenderers/RM9AllocatorPresenter.h"
// {A1542F93-EB53-4E11-8D34-05C57ABA9207}
DEFINE_GUID(CLSID_RM9AllocatorPresenter,
            0xA1542F93, 0xEB53, 0x4E11, 0x8D, 0x34, 0x5, 0xC5, 0x7A, 0xBA, 0x92, 0x7);

#include "../filters/renderer/VideoRenderers/QT9AllocatorPresenter.h"
// {622A4032-70CE-4040-8231-0F24F2886618}
DEFINE_GUID(CLSID_QT9AllocatorPresenter,
            0x622A4032, 0x70CE, 0x4040, 0x82, 0x31, 0xF, 0x24, 0xF2, 0x88, 0x66, 0x18);

#include "../filters/renderer/VideoRenderers/DXRAllocatorPresenter.h"
// {B72EBDD4-831D-440F-A656-B48F5486CD82}
DEFINE_GUID(CLSID_DXRAllocatorPresenter,
            0xB72EBDD4, 0x831D, 0x440F, 0xA6, 0x56, 0xB4, 0x8F, 0x54, 0x86, 0xCD, 0x82);

#include "../filters/renderer/VideoRenderers/madVRAllocatorPresenter.h"
// {C7ED3100-9002-4595-9DCA-B30B30413429}
DEFINE_GUID(CLSID_madVRAllocatorPresenter,
            0xC7ED3100, 0x9002, 0x4595, 0x9D, 0xCA, 0xB3, 0xB, 0x30, 0x41, 0x34, 0x29);

#include "../filters/renderer/VideoRenderers/SyncRenderer.h"
// {F9F62627-E3EF-4A2E-B6C9-5D4C0DC3326B}
DEFINE_GUID(CLSID_SyncAllocatorPresenter,
            0xF9F62627, 0xE3EF, 0x4A2E, 0xB6, 0xC9, 0x5D, 0x4C, 0xD, 0xC3, 0x32, 0x6B);

#include "../filters/renderer/VideoRenderers/EVRAllocatorPresenter.h"
// {7612B889-E070-4BCC-B808-91CB794174AB}
DEFINE_GUID(CLSID_EVRAllocatorPresenter,
            0x7612B889, 0xE070, 0x4BCC, 0xB8, 0x8, 0x91, 0xCB, 0x79, 0x41, 0x74, 0xAB);

class __declspec(novtable) CFGFilter
{
protected:
    CLSID m_clsid;
    CStringW m_name;
    struct {
        union {
            UINT64 val;
            struct {
                UINT64 low: 16, mid: 32, high: 16;
            };
        };
    } m_merit;
    CAtlList<GUID> m_types;
public:
#define FGFType_Registry 0
#define FGFType_Internal 1
#define FGFType_File 2
#define FGFType_VideoRenderer 3
    unsigned __int8 const mk_u8ClassType;

    CFGFilter(const CLSID& clsid, CStringW name = CStringW(), UINT64 merit = MERIT64_DO_USE, unsigned __int8 u8ClassType = 0);
    virtual ~CFGFilter();

    CLSID GetCLSID() const { return m_clsid; }
    CStringW GetName() const { return m_name; }
    UINT64 GetMerit() const { return m_merit.val; }
    DWORD GetMeritForDirectShow() const { return m_merit.mid; }
    const CAtlList<GUID>& GetTypes() const;
    void SetTypes(const CAtlList<GUID>& types);
    void AddType(const GUID& majortype, const GUID& subtype);
    bool CheckTypes(const CAtlArray<GUID>& types, bool fExactMatch);

    CAtlList<CString> m_protocols, m_extensions, m_chkbytes; // TODO: subtype?

    virtual HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks) = 0;
};

class CFGFilterRegistry : public CFGFilter
{
protected:
    CStringW m_DisplayName;
    CComPtr<IMoniker> m_pMoniker;

    void ExtractFilterData(BYTE* p, UINT len);

public:
    CFGFilterRegistry(IMoniker* pMoniker, UINT64 merit = MERIT64_DO_USE);
    CFGFilterRegistry(CStringW DisplayName, UINT64 merit = MERIT64_DO_USE);
    CFGFilterRegistry(const CLSID& clsid, UINT64 merit = MERIT64_DO_USE);

    CStringW GetDisplayName() { return m_DisplayName; }
    IMoniker* GetMoniker() { return m_pMoniker; }

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);
private:
    void QueryProperties();
};

template<class T>
class CFGFilterInternal : public CFGFilter
{
public:
    CFGFilterInternal(CStringW name = CStringW(), UINT64 merit = MERIT64_DO_USE) : CFGFilter(__uuidof(T), name, merit, FGFType_Internal) {}

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks) {
        CheckPointer(ppBF, E_POINTER);

        HRESULT hr = S_OK;
        CComPtr<IBaseFilter> pBF = DEBUG_NEW T(NULL, &hr);
        if (FAILED(hr)) {
            return hr;
        }

        *ppBF = pBF.Detach();

        return hr;
    }
};

class CFGFilterFile : public CFGFilter
{
protected:
    CString m_path;
    HINSTANCE m_hInst;

public:
    CFGFilterFile(const CLSID& clsid, CString path, CStringW name = L"", UINT64 merit = MERIT64_DO_USE);

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);
};

class CFGFilterVideoRenderer : public CFGFilter
{
protected:
    HWND m_hWnd;

public:
    CFGFilterVideoRenderer(HWND hWnd, const CLSID& clsid, CStringW name = L"", UINT64 merit = MERIT64_DO_USE);

    HRESULT Create(IBaseFilter** ppBF, CInterfaceList<IUnknown, &IID_IUnknown>& pUnks);
};

class CFGFilterList
{
    struct filter_t {
        int index;
        CFGFilter* pFGF;
        int group;
        bool exactmatch, autodelete;
    };
    static int filter_cmp(const void* a, const void* b);
    CAtlList<filter_t> m_filters;
    CAtlList<CFGFilter*> m_sortedfilters;

public:
    CFGFilterList();
    virtual ~CFGFilterList();

    bool IsEmpty() { return m_filters.IsEmpty(); }
    void RemoveAll();
    void Insert(CFGFilter* pFGF, int group, bool exactmatch = false, bool autodelete = true);

    POSITION GetHeadPosition();
    CFGFilter* GetNext(POSITION& pos);
};

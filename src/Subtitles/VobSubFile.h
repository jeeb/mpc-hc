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

#include <atlcoll.h>
#include "VobSubImage.h"
#include "../SubPic/ISubPic.h"

#define VOBSUBIDXVER 7

#define ReadBEb(var)              \
    f.Read(&((BYTE*)&var)[0], 1);

#define ReadBEw(var)              \
    f.Read(&((BYTE*)&var)[1], 1); \
    f.Read(&((BYTE*)&var)[0], 1);

#define ReadBEdw(var)             \
    f.Read(&((BYTE*)&var)[3], 1); \
    f.Read(&((BYTE*)&var)[2], 1); \
    f.Read(&((BYTE*)&var)[1], 1); \
    f.Read(&((BYTE*)&var)[0], 1);

extern CString FindLangFromId(WORD id);

class CVobSubSettings
{
protected:
    HRESULT Render(SubPicDesc& spd, RECT& bbox);

public:
    CSize m_size;
    int m_x, m_y;
    CPoint m_org;
    int m_scale_x, m_scale_y;   // % (don't set it to unsigned because as a side effect it will mess up negative coordinates in GetDestrect())
    int m_alpha;                // %
    int m_fSmooth;              // 0: OFF, 1: ON, 2: OLD (means no filtering at all)
    int m_fadein, m_fadeout;    // ms
    bool m_fAlign;
    int m_alignhor, m_alignver; // 0: left/top, 1: center, 2: right/bottom
    unsigned int m_toff;                // ms
    bool m_fOnlyShowForcedSubs;
    bool m_fCustomPal;
    int m_tridx;
    RGBQUAD m_orgpal[16], m_cuspal[4];

    CVobSubImage m_img;

    CVobSubSettings() { InitSettings(); }
    void InitSettings();

    bool GetCustomPal(RGBQUAD* cuspal, int& tridx);
    void SetCustomPal(RGBQUAD* cuspal, int tridx);

    void GetDestrect(CRect& r); // destrect of m_img, considering the current alignment mode
    void GetDestrect(CRect& r, int w, int h); // this will scale it to the frame size of (w, h)

    void SetAlignment(bool fAlign, int x, int y, int hor, int ver);
};

class __declspec(uuid("998D4C9A-460F-4de6-BDCD-35AB24F94ADF"))
    CVobSubFile
    : public CSubPicProviderImpl
    , public CVobSubSettings
    , public ISubStream
{
protected:
    CString m_title;

    void TrimExtension(CString& fn);
    bool ReadIdx(CString fn, int& ver), ReadSub(CString fn), ReadRar(CString fn), ReadIfo(CString fn);
    bool WriteIdx(CString fn, int delay), WriteSub(CString fn);

    CMemFile m_sub;

    BYTE* GetPacket(int idx, int& packetsize, int& datasize, int iLang = -1);
    bool InspectFrame(__in const uintptr_t idx) const;// lighter version of "GetFrame", only for inspector class types
    bool GetFrame(__in const uintptr_t idx);
    bool GetFrameByTimeStamp(__int64 time);
    int GetFrameIdxByTimeStamp(__int64 time) const;

    bool SaveVobSub(CString fn, int delay);
    bool SaveWinSubMux(CString fn, int delay);
    bool SaveScenarist(CString fn, int delay);
    bool SaveMaestro(CString fn, int delay);

public:
    typedef struct {
        __int64 filepos;
        __int64 start, stop;
        bool fForced;
        char vobid, cellid;
        __int64 celltimestamp;
        bool fValid;
    } SubPos;

    typedef struct {
        int id;
        CString name, alt;
        CAtlArray<SubPos> subpos;
    } SubLang;

    int m_iLang;
    SubLang m_langs[32];

public:
    CVobSubFile(CCritSec* pLock);
    virtual ~CVobSubFile();

    bool Copy(CVobSubFile& vsf);

    typedef enum {
        None,
        VobSub,
        WinSubMux,
        Scenarist,
        Maestro
    } SubFormat;

    bool Open(CString fn);
    bool Save(CString fn, int delay = 0, SubFormat sf = VobSub);
    void Close();

    CString GetTitle() { return m_title; }

    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    // CSubPicProviderImpl
    __declspec(nothrow noalias restrict) POSITION GetStartPosition(__in __int64 rt, __in double fps);
    __declspec(nothrow noalias restrict) POSITION GetNext(__in POSITION pos) const;
    __declspec(nothrow noalias) __int64 GetStart(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) __int64 GetStop(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) bool IsAnimated(__in POSITION pos) const;
    __declspec(nothrow noalias) HRESULT Render(__inout SubPicDesc& spd, __in __int64 i64Time, __in double fps, __out_opt RECT& bbox);

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    __declspec(nothrow noalias) size_t GetStreamCount() const;
    __declspec(nothrow noalias) HRESULT GetStreamInfo(__in size_t upStream, __out_opt WCHAR** ppName, __out_opt LCID* pLCID) const;
    __declspec(nothrow noalias) size_t GetStream() const;
    __declspec(nothrow noalias) HRESULT SetStream(__in size_t upStream);
    __declspec(nothrow noalias) HRESULT Reload();
};

class __declspec(uuid("D7FBFB45-2D13-494F-9B3D-FFC9557D5C45"))
    CVobSubStream
    : public CSubPicProviderImpl
    , public CVobSubSettings
    , public ISubStream
{
    CString m_name;

    CCritSec m_csSubPics;
    struct SubPic {
        REFERENCE_TIME tStart, tStop;
        CAtlArray<BYTE> pData;
    };
    CAutoPtrList<SubPic> m_subpics;

public:
    CVobSubStream(CCritSec* pLock);
    virtual ~CVobSubStream();

    void Open(CString name, BYTE* pData, int len);

    void Add(REFERENCE_TIME tStart, REFERENCE_TIME tStop, BYTE* pData, int len);
    void RemoveAll();

    // IUnknown
    __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
    __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

    // ISubPicProvider
    __declspec(nothrow noalias restrict) POSITION GetStartPosition(__in __int64 rt, __in double fps);
    __declspec(nothrow noalias restrict) POSITION GetNext(__in POSITION pos) const;
    __declspec(nothrow noalias) __int64 GetStart(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) __int64 GetStop(__in POSITION pos, __in double fps) const;
    __declspec(nothrow noalias) bool IsAnimated(__in POSITION pos) const;
    __declspec(nothrow noalias) HRESULT Render(__inout SubPicDesc& spd, __in __int64 i64Time, __in double fps, __out_opt RECT& bbox);

    // IPersist
    STDMETHODIMP GetClassID(CLSID* pClassID);

    // ISubStream
    __declspec(nothrow noalias) size_t GetStreamCount() const;
    __declspec(nothrow noalias) HRESULT GetStreamInfo(__in size_t upStream, __out_opt WCHAR** ppName, __out_opt LCID* pLCID) const;
    __declspec(nothrow noalias) size_t GetStream() const;
    __declspec(nothrow noalias) HRESULT SetStream(__in size_t upStream);
    __declspec(nothrow noalias) HRESULT Reload() { return E_NOTIMPL; }
};

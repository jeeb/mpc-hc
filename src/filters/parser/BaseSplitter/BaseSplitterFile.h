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

#pragma once

#ifndef MSVC_STDINT_H
#define MSVC_STDINT_H
#include <stdint.h>
#endif

#include <atlcoll.h>

#define DEFAULT_CACHE_LENGTH 64*1024    // Beliyaal: Changed the default cache length to allow Bluray playback over network

class CBaseSplitterFile
{
    CComPtr<IAsyncReader> m_pAsyncReader;
    CAutoVectorPtr<BYTE> m_pCache;
    int64_t m_cachepos, m_cachelen, m_cachetotal;

    bool m_fStreaming, m_fRandomAccess;
    int64_t m_pos, m_len;

    virtual HRESULT Read(BYTE* pData, int64_t len); // use ByteRead

protected:
    UINT64 m_bitbuff;
    int m_bitlen;

    virtual void OnComplete() {}

public:
    CBaseSplitterFile(IAsyncReader* pReader, HRESULT& hr, int cachelen = DEFAULT_CACHE_LENGTH, bool fRandomAccess = true, bool fStreaming = false);
    virtual ~CBaseSplitterFile() {}

    bool SetCacheSize(int cachelen = DEFAULT_CACHE_LENGTH);

    int64_t GetPos();
    int64_t GetAvailable();
    int64_t GetLength(bool fUpdate = false);
    int64_t GetRemaining() {return max(0, GetLength() - GetPos());}
    virtual void Seek(int64_t pos);

    UINT64 UExpGolombRead();
    INT64 SExpGolombRead();

    UINT64 BitRead(int nBits, bool fPeek = false);
    void BitByteAlign(), BitFlush();
    HRESULT ByteRead(BYTE* pData, int64_t len);

    bool IsStreaming() const {return m_fStreaming;}
    bool IsRandomAccess() const {return m_fRandomAccess;}

    HRESULT HasMoreData(int64_t len = 1, DWORD ms = 1);
};

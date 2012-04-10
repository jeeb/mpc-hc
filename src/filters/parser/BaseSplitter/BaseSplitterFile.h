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

#include <atlcoll.h>

#define DEFAULT_CACHE_LENGTH 64*1024	// Beliyaal: Changed the default cache length to allow Bluray playback over network

class CBaseSplitterFile
{
	CComPtr<IAsyncReader> m_pAsyncReader;
	CAutoVectorPtr<BYTE> m_pCache;
	__int64 m_cachepos, m_cachelen, m_cachetotal;

	bool m_fStreaming, m_fRandomAccess;
	__int64 m_pos, m_len;

	virtual HRESULT Read(BYTE* pData, __int64 len); // use ByteRead

protected:
	UINT64 m_bitbuff;
	int m_bitlen;

	virtual void OnComplete() {}

public:
	CBaseSplitterFile(IAsyncReader* pReader, HRESULT& hr, int cachelen = DEFAULT_CACHE_LENGTH, bool fRandomAccess = true, bool fStreaming = false);
	virtual ~CBaseSplitterFile() {}

	bool SetCacheSize(int cachelen = DEFAULT_CACHE_LENGTH);

	__int64 GetPos();
	__int64 GetAvailable();
	__int64 GetLength(bool fUpdate = false);
	__int64 GetRemaining() {
		return max(0, GetLength() - GetPos());
	}
	virtual void Seek(__int64 pos);

	UINT64 UExpGolombRead();
	INT64 SExpGolombRead();

	UINT64 BitRead(int nBits, bool fPeek = false);
	void BitByteAlign(), BitFlush();
	HRESULT ByteRead(BYTE* pData, __int64 len);

	bool IsStreaming()		const {
		return m_fStreaming;
	}
	bool IsRandomAccess()	const {
		return m_fRandomAccess;
	}

	HRESULT HasMoreData(__int64 len = 1, DWORD ms = 1);
};

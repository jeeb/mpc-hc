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

#include "../BaseSplitter/BaseSplitter.h"
#include "dsm/dsm.h"
#include "../../../DSUtil/DSMPropertyBag.h"

class CDSMSplitterFile : public CBaseSplitterFile
{
    HRESULT Init(IDSMResourceBagImpl& res, IDSMChapterBagImpl& chap);

public:
    CDSMSplitterFile(IAsyncReader* pReader, HRESULT& hr, IDSMResourceBagImpl& res, IDSMChapterBagImpl& chap);

    CAtlMap<BYTE, CMediaType> m_mts;
    REFERENCE_TIME m_rtFirst, m_rtDuration;

    struct SyncPoint {
        REFERENCE_TIME rt;
        int64_t fp;
    };
    CAtlArray<SyncPoint> m_sps;

    typedef CAtlMap<CStringA, CStringW, CStringElementTraits<CStringA>, CStringElementTraits<CStringW> > CStreamInfoMap;
    CStreamInfoMap m_fim;
    CAtlMap<BYTE, CStreamInfoMap> m_sim;

    bool Sync(dsmp_t& type, UINT64& len, int64_t limit = 65536);
    bool Sync(UINT64& syncpos, dsmp_t& type, UINT64& len, int64_t limit = 65536);
    bool Read(int64_t len, BYTE& id, CMediaType& mt);
    bool Read(int64_t len, Packet* p, bool fData = true);
    bool Read(int64_t len, CAtlArray<SyncPoint>& sps);
    bool Read(int64_t len, CStreamInfoMap& im);
    bool Read(int64_t len, IDSMResourceBagImpl& res);
    bool Read(int64_t len, IDSMChapterBagImpl& chap);
    int64_t Read(int64_t len, CStringW& str);

    int64_t FindSyncPoint(REFERENCE_TIME rt);
};

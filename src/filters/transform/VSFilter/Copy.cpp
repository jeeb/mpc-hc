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

#include "stdafx.h"
#include <math.h>
#include "DirectVobSubFilter.h"
#include "Scale2x.h"
#include "../../../DSUtil/DSUtil.h"
#include "../../../DSUtil/MediaTypes.h"

#include <InitGuid.h>
#include "moreuuids.h"

extern __int32 c2y_yb[256];
extern __int32 c2y_yg[256];
extern __int32 c2y_yr[256];
extern void ColorConvInit(__in const bool BT709);

void BltLineRGB32(DWORD* d, BYTE* sub, int w, const GUID& subtype)
{
    if (subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV || subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21) {
        BYTE* db = (BYTE*)d;
        BYTE* dbtend = db + w;

        for (; db < dbtend; sub += 4, db++) {
            if (sub[3] < 0xff) {
                int y = (c2y_yb[sub[0]] + c2y_yg[sub[1]] + c2y_yr[sub[2]] + 0x108000) >> 16;
                *db = y; // w/o colors
            }
        }
    } else if (subtype == MEDIASUBTYPE_YUY2) {
        WORD* ds = (WORD*)d;
        WORD* dstend = ds + w;

        for (; ds < dstend; sub += 4, ds++) {
            if (sub[3] < 0xff) {
                int y = (c2y_yb[sub[0]] + c2y_yg[sub[1]] + c2y_yr[sub[2]] + 0x108000) >> 16;
                *ds = 0x8000 | y; // w/o colors
            }
        }
    } else if (subtype == MEDIASUBTYPE_RGB555) {
        WORD* ds = (WORD*)d;
        WORD* dstend = ds + w;

        for (; ds < dstend; sub += 4, ds++) {
            if (sub[3] < 0xff) {
                *ds = ((*((DWORD*)sub) >> 9) & 0x7c00) | ((*((DWORD*)sub) >> 6) & 0x03e0) | ((*((DWORD*)sub) >> 3) & 0x001f);
            }
        }
    } else if (subtype == MEDIASUBTYPE_RGB565) {
        WORD* ds = (WORD*)d;
        WORD* dstend = ds + w;

        for (; ds < dstend; sub += 4, ds++) {
            if (sub[3] < 0xff) {
                *ds = ((*((DWORD*)sub) >> 8) & 0xf800) | ((*((DWORD*)sub) >> 5) & 0x07e0) | ((*((DWORD*)sub) >> 3) & 0x001f);
            }
        }
    } else if (subtype == MEDIASUBTYPE_RGB24) {
        BYTE* dt = (BYTE*)d;
        BYTE* dstend = dt + w * 3;

        for (; dt < dstend; sub += 4, dt += 3) {
            if (sub[3] < 0xff) {
                dt[0] = sub[0];
                dt[1] = sub[1];
                dt[2] = sub[2];
            }
        }
    } else if (subtype == MEDIASUBTYPE_RGB32 || subtype == MEDIASUBTYPE_ARGB32) {
        DWORD* dstend = d + w;

        for (; d < dstend; sub += 4, d++) {
            if (sub[3] < 0xff) {
                *d = *((DWORD*)sub) & 0xffffff;
            }
        }
    }
}

HRESULT CDirectVobSubFilter::Copy(BYTE* pSub, BYTE* pIn, CSize sub, CSize in, int bpp, const GUID& subtype, DWORD black)
{
    int wIn = in.cx, hIn = in.cy, pitchIn = wIn * bpp >> 3;
    int wSub = sub.cx, hSub = sub.cy, pitchSub = wSub * bpp >> 3;
    bool fScale2x = wIn * 2 <= wSub;

    if (fScale2x) {
        wIn <<= 1, hIn <<= 1;
    }

    int left = ((wSub - wIn) >> 1)&~1;
    int mid = wIn;
    int right = left + ((wSub - wIn) & 1);

    int dpLeft = left * bpp >> 3;
    int dpMid = mid * bpp >> 3;
    int dpRight = right * bpp >> 3;

    ASSERT(wSub >= wIn);

    {
        int i = 0, j = 0;

        j += (hSub - hIn) >> 1;

        for (; i < j; i++, pSub += pitchSub) {
            memsetd(pSub, black, dpLeft + dpMid + dpRight);
        }

        j += hIn;

        if (hIn > hSub) {
            pIn += pitchIn * ((hIn - hSub) >> (fScale2x ? 2 : 1));
        }

        if (fScale2x) {
            Scale2x(subtype,
                    pSub + dpLeft, pitchSub, pIn, pitchIn,
                    in.cx, (min(j, hSub) - i) >> 1);

            for (ptrdiff_t k = min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub) {
                memsetd(pSub, black, dpLeft);
                memsetd(pSub + dpLeft + dpMid, black, dpRight);
            }
        } else {
            for (ptrdiff_t k = min(j, hSub); i < k; i++, pIn += pitchIn, pSub += pitchSub) {
                memsetd(pSub, black, dpLeft);
                memcpy(pSub + dpLeft, pIn, dpMid);
                memsetd(pSub + dpLeft + dpMid, black, dpRight);
            }
        }

        j = hSub;

        for (; i < j; i++, pSub += pitchSub) {
            memsetd(pSub, black, dpLeft + dpMid + dpRight);
        }
    }

    return NOERROR;
}

void CDirectVobSubFilter::PrintMessages(BYTE* pOut)
{
    if (!m_hdc || !m_hbm) {
        return;
    }

    ColorConvInit(false);

    const GUID& subtype = m_pOutput->CurrentMediaType().subtype;

    BITMAPINFOHEADER bihOut;
    ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

    if (!m_fOSD) {
        return;
    }

    CString msg;

    msg.Format(_T("in: %dx%d %s\nout: %dx%d %s\n"),
               m_w, m_h,
               Subtype2String(m_pInput->CurrentMediaType().subtype),
               bihOut.biWidth, bihOut.biHeight,
               Subtype2String(m_pOutput->CurrentMediaType().subtype));

    msg.AppendFormat(_T("real fps: %.3f, current fps: %.3f\nmedia time: %d, subtitle time: %I64d [ms]\nframe number: %d (calculated)\nrate: %.4lf\n"),
                     m_fps, m_fMediaFPSEnabled ? m_MediaFPS : fabs(m_fps),
                     m_tPrev.Millisecs(), CalcCurrentTime() / 10000,
                     (int)(m_tPrev.m_time * m_fps / 10000000.0 + 0.5),
                     m_pInput->CurrentRate());

    CAutoLock cAutoLock(&m_csQueueLock);

    if (m_pSubPicQueue) {
        SubPicQueueStats QStats;
        m_pSubPicQueue->GetStats(&QStats);
        unsigned __int8 u8SubPics = QStats.u8SubPics;
        msg.AppendFormat(L"Queue stats: Buffered %2hu, Queue start %+8.3f s, Queue end %+8.3f s\n", u8SubPics, static_cast<double>(QStats.i64Start) / 10000000.0, static_cast<double>(QStats.i64Stop) / 10000000.0);

        for (unsigned __int8 i = 0; i < u8SubPics; i++) {
            m_pSubPicQueue->GetStats(i, &QStats.i64Start);
            msg.AppendFormat(L"Subtitle %2hu: [%I64d, %I64d) ms\n", i, QStats.i64Start / 10000, QStats.i64Stop / 10000);
        }
    }

    HANDLE hOldBitmap = SelectObject(m_hdc, m_hbm);
    HANDLE hOldFont = SelectObject(m_hdc, m_hfont);

    SetTextColor(m_hdc, 0xffffff);
    SetBkMode(m_hdc, TRANSPARENT);
    SetMapMode(m_hdc, MM_TEXT);

    BITMAP bm;
    GetObject(m_hbm, sizeof(BITMAP), &bm);

    CRect r(0, 0, bm.bmWidth, bm.bmHeight);
    DrawText(m_hdc, msg, msg.GetLength(), &r, DT_CALCRECT | DT_EXTERNALLEADING | DT_NOPREFIX | DT_WORDBREAK);

    r += CPoint(10, 10);
    r &= CRect(0, 0, bm.bmWidth, bm.bmHeight);

    DrawText(m_hdc, msg, msg.GetLength(), &r, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);

    BYTE* pIn = (BYTE*)bm.bmBits;
    int pitchIn = bm.bmWidthBytes;
    int pitchOut = bihOut.biWidth * bihOut.biBitCount >> 3;

    if (subtype == MEDIASUBTYPE_YV12 || subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV || subtype == MEDIASUBTYPE_NV12 || subtype == MEDIASUBTYPE_NV21) {
        pitchOut = bihOut.biWidth;
    }

    pitchIn = (pitchIn + 3)&~3;
    pitchOut = (pitchOut + 3)&~3;

    if (bihOut.biHeight > 0 && bihOut.biCompression <= 3) { // flip if the dst bitmap is flipped rgb (m_hbm is a top-down bitmap, not like the subpictures)
        pOut += pitchOut * (abs(bihOut.biHeight) - 1);
        pitchOut = -pitchOut;
    }

    pIn += pitchIn * r.top;
    pOut += pitchOut * r.top;

    for (int w = min(r.right, m_w), h = r.Height(); h--; pIn += pitchIn, pOut += pitchOut) {
        BltLineRGB32((DWORD*)pOut, pIn, w, subtype);
        memsetd(pIn, 0xff000000, r.right * 4);
    }

    SelectObject(m_hdc, hOldBitmap);
    SelectObject(m_hdc, hOldFont);
}

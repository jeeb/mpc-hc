/*
 * (C) 2010-2013 see Authors.txt
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
#include "SyncClock.h"

CSyncClockFilter::CSyncClockFilter(HRESULT* phr)
    : CBaseFilter(NAME("SyncClock"), nullptr, &m_Lock, CLSID_NULL)
    , m_Clock(static_cast<IBaseFilter*>(this), phr)
{
    AddRef();
}

STDMETHODIMP CSyncClockFilter::AdjustClock(DOUBLE adjustment)
{
    m_Clock.m_dAdjustment = adjustment;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::SetBias(DOUBLE bias)
{
    m_Clock.m_dBias = bias;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::GetBias(DOUBLE* bias)
{
    *bias = m_Clock.m_dBias;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::GetStartTime(REFERENCE_TIME* startTime)
{
    *startTime = m_tStart;
    return S_OK;
}

STDMETHODIMP CSyncClockFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
    CheckPointer(ppv, E_POINTER);

    if (riid == IID_IReferenceClock) {
        return GetInterface(static_cast<IReferenceClock*>(&m_Clock), ppv);
    } else if (riid == IID_ISyncClock) {
        return GetInterface(static_cast<ISyncClock*>(this), ppv);
    } else {
        return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
    }
}

int CSyncClockFilter::GetPinCount()
{
    return 0;
}

CBasePin* CSyncClockFilter::GetPin(int i)
{
    UNREFERENCED_PARAMETER(i);
    return nullptr;
}

// CSyncClock methods
CSyncClock::CSyncClock(LPUNKNOWN pUnk, HRESULT* phr)
    : CBaseReferenceClock(NAME("SyncClock"), pUnk, phr)
    , m_pCurrentRefClock(nullptr)
    , m_pPrevRefClock(nullptr)
    , m_dAdjustment(1.0)
    , m_dBias(1.0)
{
    m_rtPrevTime = m_rtPrivateTime = PerfCounter100ns();
}

REFERENCE_TIME CSyncClock::GetPrivateTime()
{
    CAutoLock cObjectLock(this);

    REFERENCE_TIME rtTime = PerfCounter100ns();
    m_rtPrevTime = rtTime;
    REFERENCE_TIME delta = rtTime - m_rtPrevTime;

    delta = static_cast<REFERENCE_TIME>(static_cast<double>(delta) * m_dAdjustment * m_dBias + 0.5);
    m_rtPrivateTime += delta;
    return m_rtPrivateTime;
}

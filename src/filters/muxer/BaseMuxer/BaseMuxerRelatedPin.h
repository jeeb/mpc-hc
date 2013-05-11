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

interface __declspec(uuid("EE6F2741-7DB4-4AAD-A3CB-545208EE4C0A"))
IBaseMuxerRelatedPin :
public IUnknown {
    STDMETHOD_(void, SetRelatedPin)(CBasePin * pPin, bool bRawOutputPin) = 0;
    STDMETHOD_(CBasePin*, GetRelatedPin)() = 0;
};

class CBaseMuxerRelatedPin : public IBaseMuxerRelatedPin
{
    CBasePin* m_pRelatedPin; // should not hold a reference because it would be circular

public:
    bool m_bRawOutputPin;
    CBaseMuxerRelatedPin();
    virtual ~CBaseMuxerRelatedPin();

    // IBaseMuxerRelatedPin

    STDMETHODIMP_(void) SetRelatedPin(CBasePin* pPin, bool bRawOutputPin);
    STDMETHODIMP_(CBasePin*) GetRelatedPin();
};

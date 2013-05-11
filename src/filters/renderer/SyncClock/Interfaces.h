/*
 * (C) 2010-2012 see Authors.txt
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

const IID IID_ISyncClock = {0xa62888fb, 0x8e37, 0x44d2, {0x88, 0x50, 0xb3, 0xe3, 0xf2, 0xc1, 0x16, 0x9f}};

interface __declspec(uuid("A62888FB-8E37-44d2-8850-B3E3F2C1169F") novtable)
ISyncClock:
public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE AdjustClock(double adjustment) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBias(double bias) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBias(double * bias) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStartTime(REFERENCE_TIME * startTime) = 0;
};

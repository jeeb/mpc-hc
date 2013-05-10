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

interface __declspec(uuid("CD6D2AA5-20D3-4ebe-A8A9-34D3B00CC253") novtable)
ISubRenderCallback :
public IUnknown {
    // nullptr means release current device, textures and other resources
    virtual __declspec(nothrow noalias) STDMETHODIMP SetDevice(IDirect3DDevice9 * dev) = 0;

    // destination video rectangle, will be inside (0, 0)-(width, height)
    // width,height is the size of the entire output window
    virtual __declspec(nothrow noalias) STDMETHODIMP Render(REFERENCE_TIME rtStart, int left, int top, int bottom, int right, int width, int height) = 0;
};

interface __declspec(uuid("E602585E-C05A-4828-AC69-AF92997F2E0C") novtable)
ISubRenderCallback2 :
public ISubRenderCallback {
    virtual __declspec(nothrow noalias) STDMETHODIMP RenderEx(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, REFERENCE_TIME AvgTimePerFrame, int left, int top, int right, int bottom, int width, int height) = 0;
};

interface __declspec(uuid("9CC7F9F7-3ED1-493c-AF65-527EA1D9947F") novtable)
ISubRender :
public IUnknown {
    virtual __declspec(nothrow noalias) STDMETHODIMP SetCallback(ISubRenderCallback * cb) = 0;
};

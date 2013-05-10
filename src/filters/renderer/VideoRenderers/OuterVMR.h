/*
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

#include "AllocatorCommon.h"
#include <vmr9.h>

namespace DSObjects
{
    class COuterVMR// note: this class holds the final reference before destruction of CVMR9AllocatorPresenter, see COuterVMR::Release() for reference
        : public IBaseFilter
        , public IVideoWindow
        , public IBasicVideo2
        , public IKsPropertySet
    {
    public:
        IUnknown* m_pVMR;
        IBaseFilter* m_pBaseFilter;// does not hold a reference inside this class
        ULONG volatile mv_ulReferenceCount;

        __declspec(nothrow noalias) __forceinline COuterVMR() : mv_ulReferenceCount(0) {}// the first reference is created by CVMR9AllocatorPresenter

        // IUnknown
        __declspec(nothrow noalias) STDMETHODIMP QueryInterface(REFIID riid, __deref_out void** ppv);
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) AddRef();
        __declspec(nothrow noalias) STDMETHODIMP_(ULONG) Release();

        // IBaseFilter
        __declspec(nothrow noalias) STDMETHODIMP EnumPins(__out IEnumPins** ppEnum);
        __declspec(nothrow noalias) STDMETHODIMP FindPin(LPCWSTR Id, __out IPin** ppPin);
        __declspec(nothrow noalias) STDMETHODIMP QueryFilterInfo(__out FILTER_INFO* pInfo);
        __declspec(nothrow noalias) STDMETHODIMP JoinFilterGraph(__in_opt IFilterGraph* pGraph, __in_opt LPCWSTR pName);
        __declspec(nothrow noalias) STDMETHODIMP QueryVendorInfo(__out LPWSTR* pVendorInfo);

        // IMediaFilter
        __declspec(nothrow noalias) STDMETHODIMP Stop();
        __declspec(nothrow noalias) STDMETHODIMP Pause();
        __declspec(nothrow noalias) STDMETHODIMP Run(REFERENCE_TIME tStart);
        __declspec(nothrow noalias) STDMETHODIMP GetState(DWORD dwMilliSecsTimeout, __out FILTER_STATE* State);
        __declspec(nothrow noalias) STDMETHODIMP SetSyncSource(__in_opt IReferenceClock* pClock);
        __declspec(nothrow noalias) STDMETHODIMP GetSyncSource(__deref_out_opt IReferenceClock** pClock);

        // IPersist
        __declspec(nothrow noalias) STDMETHODIMP GetClassID(__RPC__out CLSID* pClassID);

        // IVideoWindow
        __declspec(nothrow noalias) STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_Caption(BSTR strCaption) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Caption(BSTR* strCaption) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_WindowStyle(long WindowStyle) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_WindowStyle(long* WindowStyle) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_WindowStyleEx(long WindowStyleEx) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_WindowStyleEx(long* WindowStyleEx) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_AutoShow(long AutoShow) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_AutoShow(long* AutoShow) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_WindowState(long WindowState) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_WindowState(long* WindowState) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_BackgroundPalette(long BackgroundPalette) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_BackgroundPalette(long* pBackgroundPalette) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_Visible(long Visible) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Visible(long* pVisible) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_Left(long Left) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Left(long* pLeft) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_Width(long Width) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Width(long* pWidth);

        __declspec(nothrow noalias) STDMETHODIMP put_Top(long Top) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Top(long* pTop) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_Height(long Height) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Height(long* pHeight);

        __declspec(nothrow noalias) STDMETHODIMP put_Owner(OAHWND Owner) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_Owner(OAHWND* Owner) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_MessageDrain(OAHWND Drain) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_MessageDrain(OAHWND* Drain) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_BorderColor(long* Color) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_BorderColor(long Color) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_FullScreenMode(long* FullScreenMode) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_FullScreenMode(long FullScreenMode) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP SetWindowForeground(long Focus) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP NotifyOwnerMessage(OAHWND hwnd, long uMsg, LONG_PTR wParam, LONG_PTR lParam) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP SetWindowPosition(long Left, long Top, long Width, long Height) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetWindowPosition(long* pLeft, long* pTop, long* pWidth, long* pHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetMinIdealImageSize(long* pWidth, long* pHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetMaxIdealImageSize(long* pWidth, long* pHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetRestorePosition(long* pLeft, long* pTop, long* pWidth, long* pHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP HideCursor(long HideCursor) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP IsCursorHidden(long* CursorHidden) {
            return E_NOTIMPL;
        }

        // IBasicVideo2
        __declspec(nothrow noalias) STDMETHODIMP get_AvgTimePerFrame(REFTIME* pAvgTimePerFrame) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_BitRate(long* pBitRate) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_BitErrorRate(long* pBitErrorRate) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_VideoWidth(long* pVideoWidth) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_VideoHeight(long* pVideoHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_SourceLeft(long SourceLeft) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_SourceLeft(long* pSourceLeft) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_SourceWidth(long SourceWidth) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_SourceWidth(long* pSourceWidth) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_SourceTop(long SourceTop) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_SourceTop(long* pSourceTop) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_SourceHeight(long SourceHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_SourceHeight(long* pSourceHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_DestinationLeft(long DestinationLeft) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_DestinationLeft(long* pDestinationLeft) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_DestinationWidth(long DestinationWidth) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_DestinationWidth(long* pDestinationWidth) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_DestinationTop(long DestinationTop) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_DestinationTop(long* pDestinationTop) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP put_DestinationHeight(long DestinationHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP get_DestinationHeight(long* pDestinationHeight) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP SetSourcePosition(long Left, long Top, long Width, long Height) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetSourcePosition(long* pLeft, long* pTop, long* pWidth, long* pHeight);

        __declspec(nothrow noalias) STDMETHODIMP SetDefaultSourcePosition() {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP SetDestinationPosition(long Left, long Top, long Width, long Height) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetDestinationPosition(long* pLeft, long* pTop, long* pWidth, long* pHeight);

        __declspec(nothrow noalias) STDMETHODIMP SetDefaultDestinationPosition() {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetVideoSize(long* pWidth, long* pHeight);

        __declspec(nothrow noalias) STDMETHODIMP GetVideoPaletteEntries(long StartIndex, long Entries, long* pRetrieved, long* pPalette) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetCurrentImage(long* pBufferSize, long* pDIBImage) {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP IsUsingDefaultSource() {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP IsUsingDefaultDestination() {
            return E_NOTIMPL;
        }
        __declspec(nothrow noalias) STDMETHODIMP GetPreferredAspectRatio(long* plAspectX, long* plAspectY);

        // IKsPropertySet - MacrovisionKicker
        __declspec(nothrow noalias) STDMETHODIMP Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength);
        __declspec(nothrow noalias) STDMETHODIMP Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned);
        __declspec(nothrow noalias) STDMETHODIMP QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport);
    };
}

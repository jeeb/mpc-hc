/*
 * (C) 2009-2012 see Authors.txt
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

#include "BaseClasses/streams.h"
#include <dsound.h>

#include <MMReg.h>  //must be before other Wasapi headers
#include <strsafe.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <FunctionDiscoveryKeys_devpkey.h>

#include "MpcAudioRendererSettingsWnd.h"
#include "SoundTouch/include/SoundTouch.h"

#define MpcAudioRendererName L"MPC Audio Renderer"

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC        10000000.0
#define REFTIMES_PER_MILLISEC   10000.0

// if you get a compilation error on AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED,
// uncomment the #define below
#define AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED  AUDCLNT_ERR(0x019)

class __declspec(uuid("601D2A2B-9CDE-40bd-8650-0485E3522727"))
    CMpcAudioRenderer : public CBaseRenderer
    , public IBasicAudio
    , public ISpecifyPropertyPages2
    , public IMpcAudioRendererFilter
{
public:
    CMpcAudioRenderer(LPUNKNOWN punk, HRESULT* phr);
    virtual ~CMpcAudioRenderer();

    static const AMOVIESETUP_FILTER sudASFilter;

    HRESULT CheckInputType(const CMediaType* mtIn);
    virtual HRESULT CheckMediaType(const CMediaType* pmt);
    virtual HRESULT DoRenderSample(IMediaSample* pMediaSample);
    virtual void OnReceiveFirstSample(IMediaSample* pMediaSample);
    BOOL ScheduleSample(IMediaSample* pMediaSample);
    virtual HRESULT SetMediaType(const CMediaType* pmt);
    virtual HRESULT CompleteConnect(IPin* pReceivePin);

    HRESULT EndOfStream();


    DECLARE_IUNKNOWN

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // === IMediaFilter
    STDMETHOD(Run)(REFERENCE_TIME tStart);
    STDMETHOD(Stop)();
    STDMETHOD(Pause)();

    // === IDispatch (pour IBasicAudio)
    STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
    STDMETHOD(GetTypeInfo)(UINT itinfo, LCID lcid, ITypeInfo** pptinfo);
    STDMETHOD(GetIDsOfNames)(REFIID riid, OLECHAR** rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid);
    STDMETHOD(Invoke)(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult, EXCEPINFO* pexcepinfo, UINT* puArgErr);

    // === IBasicAudio
    STDMETHOD(put_Volume)(long lVolume);
    STDMETHOD(get_Volume)(long* plVolume);
    STDMETHOD(put_Balance)(long lBalance);
    STDMETHOD(get_Balance)(long* plBalance);

    // === ISpecifyPropertyPages2
    STDMETHODIMP GetPages(CAUUID* pPages);
    STDMETHODIMP CreatePage(const GUID& guid, IPropertyPage** ppPage);

    // === IMpcAudioRendererFilter
    STDMETHODIMP Apply();
    STDMETHODIMP SetWasapiMode(BOOL nValue);
    STDMETHODIMP_(BOOL) GetWasapiMode();
    STDMETHODIMP SetMuteFastForward(BOOL nValue);
    STDMETHODIMP_(BOOL) GetMuteFastForward();
    STDMETHODIMP SetSoundDevice(CString nValue);
    STDMETHODIMP_(CString) GetSoundDevice();

    // CMpcAudioRenderer
private:

    HRESULT DoRenderSampleDirectSound(IMediaSample* pMediaSample);

    HRESULT InitCoopLevel();
    HRESULT ClearBuffer();
    HRESULT CreateDSBuffer();
    HRESULT GetReferenceClockInterface(REFIID riid, void** ppv);
    HRESULT WriteSampleToDSBuffer(IMediaSample* pMediaSample, bool* looped);

    LPDIRECTSOUND8 m_pDS;
    LPDIRECTSOUNDBUFFER m_pDSBuffer;
    DWORD m_dwDSWriteOff;
    WAVEFORMATEX* m_pWaveFileFormat;
    int m_nDSBufSize;
    CBaseReferenceClock* m_pReferenceClock;
    double m_dRate;
    long m_lVolume;
    soundtouch::SoundTouch* m_pSoundTouch;
    CCritSec m_csProps;

    // CMpcAudioRenderer WASAPI methods
    HRESULT GetAvailableAudioDevices(IMMDeviceCollection** ppMMDevices);
    HRESULT GetAudioDevice(IMMDevice** ppMMDevice);
    HRESULT CreateAudioClient(IMMDevice* pMMDevice, IAudioClient** ppAudioClient);
    HRESULT InitAudioClient(WAVEFORMATEX* pWaveFormatEx, IAudioClient* pAudioClient, IAudioRenderClient** ppRenderClient);
    HRESULT CheckAudioClient(WAVEFORMATEX* pWaveFormatEx);
    bool CheckFormatChanged(WAVEFORMATEX* pWaveFormatEx, WAVEFORMATEX** ppNewWaveFormatEx);
    HRESULT DoRenderSampleWasapi(IMediaSample* pMediaSample);
    HRESULT GetBufferSize(WAVEFORMATEX* pWaveFormatEx, REFERENCE_TIME* pHnsBufferPeriod);


    // WASAPI variables
    bool m_useWASAPI;
    bool m_useWASAPIAfterRestart;
    bool m_bMuteFastForward;
    bool m_bSamplesNeed24to32Conversion;
    CMediaType m_cmtMediaType;
    CString m_csSound_Device;
    IMMDevice* pMMDevice;
    IAudioClient* pAudioClient;
    IAudioRenderClient* pRenderClient;
    UINT32 nFramesInBuffer;
    REFERENCE_TIME hnsPeriod, hnsActualDuration;
    HANDLE hTask;
    CCritSec m_csCheck;
    UINT32 bufferSize;
    bool isAudioClientStarted;
    DWORD lastBufferTime;

    // avrt.dll (Vista or greater)
    typedef HANDLE(__stdcall* PTR_AvSetMmThreadCharacteristicsW)(LPCWSTR TaskName, LPDWORD TaskIndex);
    typedef BOOL (__stdcall* PTR_AvRevertMmThreadCharacteristics)(HANDLE AvrtHandle);

    HMODULE m_hLibAVRT;
    PTR_AvSetMmThreadCharacteristicsW pfAvSetMmThreadCharacteristicsW;
    PTR_AvRevertMmThreadCharacteristics pfAvRevertMmThreadCharacteristics;

};

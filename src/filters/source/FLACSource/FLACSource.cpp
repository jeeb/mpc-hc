/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include <MMReg.h>
#include <ks.h>
#include <InitGuid.h>
#include <uuids.h>
#include <moreuuids.h>
#include "FLACSource.h"
#include "../../../DSUtil/DSUtil.h"
#include <FLAC/stream_decoder.h>

#define _DECODER_   (FLAC__StreamDecoder*)m_pDecoder

#ifdef REGISTER_FILTER

const AMOVIESETUP_MEDIATYPE sudPinTypesOut[] = {
	{&MEDIATYPE_Audio, &MEDIASUBTYPE_FLAC_FRAMED}
};

const AMOVIESETUP_PIN sudOpPin[] = {
	{L"Output", FALSE, TRUE, FALSE, FALSE, &CLSID_NULL, NULL, countof(sudPinTypesOut), sudPinTypesOut}
};

const AMOVIESETUP_FILTER sudFilter[] = {
	{&__uuidof(CFLACSource), L"MPC - FLAC Source", MERIT_NORMAL, countof(sudOpPin), sudOpPin, CLSID_LegacyAmFilterCategory}
};

CFactoryTemplate g_Templates[] = {
	{sudFilter[0].strName, sudFilter[0].clsID, CreateInstance<CFLACSource>, NULL, &sudFilter[0]}
};

int g_cTemplates = countof(g_Templates);

STDAPI DllRegisterServer()
{
	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{1930D8FF-4739-4e42-9199-3B2EDEAA3BF2}"),
		_T("0"), _T("0,4,,664C6143"));

	SetRegKeyValue(
		_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{1930D8FF-4739-4e42-9199-3B2EDEAA3BF2}"),
		_T("Source Filter"), _T("{1930D8FF-4739-4e42-9199-3B2EDEAA3BF2}"));

	SetRegKeyValue(
		_T("Media Type\\Extensions"), _T(".flac"),
		_T("Source Filter"), _T("{1930D8FF-4739-4e42-9199-3B2EDEAA3BF2}"));

	return AMovieDllRegisterServer2(TRUE);
}

STDAPI DllUnregisterServer()
{
	DeleteRegKey(_T("Media Type\\{e436eb83-524f-11ce-9f53-0020af0ba770}"), _T("{1930D8FF-4739-4e42-9199-3B2EDEAA3BF2}"));
	DeleteRegKey(_T("Media Type\\Extensions"), _T(".flac"));

	return AMovieDllRegisterServer2(FALSE);
}

#include "../../FilterApp.h"

CFilterApp theApp;

#endif


// Declaration for FLAC callbacks
static FLAC__StreamDecoderReadStatus	StreamDecoderRead(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderSeekStatus	StreamDecoderSeek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderTellStatus	StreamDecoderTell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
static FLAC__StreamDecoderLengthStatus	StreamDecoderLength(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
static FLAC__bool						StreamDecoderEof(const FLAC__StreamDecoder *decoder, void *client_data);
static FLAC__StreamDecoderWriteStatus	StreamDecoderWrite(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void								StreamDecoderError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);
static void								StreamDecoderMetadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);


//
// CFLACSource
//

CFLACSource::CFLACSource(LPUNKNOWN lpunk, HRESULT* phr)
	: CBaseSource<CFLACStream>(NAME("CFLACSource"), lpunk, phr, __uuidof(this))
{
}

CFLACSource::~CFLACSource()
{
}

// CFLACStream

CFLACStream::CFLACStream(const WCHAR* wfn, CSource* pParent, HRESULT* phr)
	: CBaseStream(NAME("CFLACStream"), pParent, phr)
	, m_bIsEOF (false)
{
	CAutoLock		cAutoLock(&m_cSharedState);
	CString			fn(wfn);
	CFileException	ex;
	HRESULT			hr = E_FAIL;

	do {
		if (!m_file.Open(fn, CFile::modeRead|CFile::shareDenyNone, &ex)) {
			hr	= AmHresultFromWin32 (ex.m_lOsError);
			break;
		}

		m_pDecoder = FLAC__stream_decoder_new();
		if (!m_pDecoder) {
			break;
		}

		if (FLAC__STREAM_DECODER_INIT_STATUS_OK != FLAC__stream_decoder_init_stream (_DECODER_,
				StreamDecoderRead,
				StreamDecoderSeek,
				StreamDecoderTell,
				StreamDecoderLength,
				StreamDecoderEof,
				StreamDecoderWrite,
				StreamDecoderMetadata,
				StreamDecoderError,
				this)) {
			break;
		}


		if (!FLAC__stream_decoder_process_until_end_of_metadata (_DECODER_) ||
				!FLAC__stream_decoder_seek_absolute (_DECODER_, 0)) {
			break;
		}

		FLAC__stream_decoder_get_decode_position(_DECODER_, &m_llOffset);

		hr = S_OK;
	} while (false);

	if (phr) {
		*phr = hr;
	}
}

CFLACStream::~CFLACStream()
{
}

HRESULT CFLACStream::DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProperties)
{
	ASSERT(pAlloc);
	ASSERT(pProperties);

	HRESULT hr = NOERROR;

	pProperties->cBuffers = 1;
	pProperties->cbBuffer = m_nMaxFrameSize;

	ALLOCATOR_PROPERTIES Actual;
	if (FAILED(hr = pAlloc->SetProperties(pProperties, &Actual))) {
		return hr;
	}

	if (Actual.cbBuffer < pProperties->cbBuffer) {
		return E_FAIL;
	}
	ASSERT(Actual.cBuffers == pProperties->cBuffers);

	return NOERROR;
}

HRESULT CFLACStream::FillBuffer(IMediaSample* pSample, int nFrame, BYTE* pOut, long& len)
{
	FLAC__uint64	llCurPos;
	FLAC__uint64	llNextPos;

	if (m_bDiscontinuity) {
		FLAC__stream_decoder_seek_absolute (_DECODER_, (m_rtPosition * m_i64TotalNumSamples) / m_rtDuration);
	}

	FLAC__stream_decoder_get_decode_position(_DECODER_, &llCurPos);

	FLAC__stream_decoder_skip_single_frame (_DECODER_);
	if (m_bIsEOF) {
		return S_FALSE;
	}
	FLAC__stream_decoder_get_decode_position(_DECODER_, &llNextPos);

	FLAC__uint64	llCurFile = m_file.GetPosition();
	len = llNextPos - llCurPos;
	ASSERT (len > 0);
	if (len <= 0) {
		return S_FALSE;
	}

	m_file.Seek (llCurPos, CFile::begin);
	m_file.Read (pOut, len);
	m_file.Seek (llCurFile, CFile::begin);

	m_AvgTimePerFrame = m_rtDuration * len / (m_llFileSize-m_llOffset);

	return S_OK;
}

HRESULT CFLACStream::GetMediaType(int iPosition, CMediaType* pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (iPosition == 0) {
		pmt->majortype			= MEDIATYPE_Audio;
		pmt->subtype			= MEDIASUBTYPE_FLAC_FRAMED;
		pmt->formattype			= FORMAT_WaveFormatEx;
		WAVEFORMATEX* wfe		= (WAVEFORMATEX*)pmt->AllocFormatBuffer(sizeof(WAVEFORMATEX));
		memset(wfe, 0, sizeof(WAVEFORMATEX));
		wfe->cbSize = sizeof(WAVEFORMATEX);
		wfe->wFormatTag			= WAVE_FORMAT_FLAC;
		wfe->nSamplesPerSec		= m_nSamplesPerSec;
		wfe->nAvgBytesPerSec	= m_nAvgBytesPerSec;
		wfe->nChannels			= m_nChannels;
		wfe->nBlockAlign		= 1;
		wfe->wBitsPerSample		= m_wBitsPerSample;
	} else {
		return VFW_S_NO_MORE_ITEMS;
	}

	pmt->SetTemporalCompression(FALSE);

	return S_OK;
}

HRESULT CFLACStream::CheckMediaType(const CMediaType* pmt)
{
	if (   pmt->majortype  == MEDIATYPE_Audio
			&& pmt->subtype    == MEDIASUBTYPE_FLAC_FRAMED
			&& pmt->formattype == FORMAT_WaveFormatEx
			&& ((WAVEFORMATEX*)pmt->pbFormat)->wFormatTag == WAVE_FORMAT_FLAC) {
		return S_OK;
	} else {
		return E_INVALIDARG;
	}
}

void CFLACStream::UpdateFromMetadata (void* pBuffer)
{
	const FLAC__StreamMetadata* pMetadata = (const FLAC__StreamMetadata*) pBuffer;

	m_nMaxFrameSize			= pMetadata->data.stream_info.max_framesize;
	m_nSamplesPerSec		= pMetadata->data.stream_info.sample_rate;
	m_nChannels				= pMetadata->data.stream_info.channels;
	m_wBitsPerSample		= pMetadata->data.stream_info.bits_per_sample;
	m_i64TotalNumSamples	= pMetadata->data.stream_info.total_samples;
	m_nAvgBytesPerSec		= (m_nChannels * (m_wBitsPerSample >> 3)) * m_nSamplesPerSec;

	// === Init members from base classes
	GetFileSizeEx (m_file.m_hFile, (LARGE_INTEGER*)&m_llFileSize);
	m_rtDuration			= (m_i64TotalNumSamples * UNITS) / m_nSamplesPerSec;
	m_rtStop				= m_rtDuration;
	m_AvgTimePerFrame		= (m_nMaxFrameSize + pMetadata->data.stream_info.min_framesize) * m_rtDuration / 2 / m_llFileSize;
}

FLAC__StreamDecoderReadStatus StreamDecoderRead(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);
	UINT			nRead;

	nRead				= pThis->GetFile()->Read (buffer, *bytes);
	pThis->m_bIsEOF	= (nRead != *bytes);
	*bytes				= nRead;

	return (*bytes == 0) ?  FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM : FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus	StreamDecoderSeek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);

	pThis->m_bIsEOF	= false;
	pThis->GetFile()->Seek (absolute_byte_offset, CFile::begin);
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus	StreamDecoderTell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);
	*absolute_byte_offset = pThis->GetFile()->GetPosition();
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus	StreamDecoderLength(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);
	CFile*			pFile = pThis->GetFile();

	if (pFile == NULL) {
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	} else {
		*stream_length = pFile->GetLength();
		return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
	}
}

FLAC__bool StreamDecoderEof(const FLAC__StreamDecoder *decoder, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);

	return pThis->m_bIsEOF;
}

FLAC__StreamDecoderWriteStatus StreamDecoderWrite(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);
	UNUSED_ALWAYS(pThis);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void StreamDecoderError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
}

void StreamDecoderMetadata(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	CFLACStream*	pThis = static_cast<CFLACStream*> (client_data);

	if (pThis) {
		pThis->UpdateFromMetadata ((void*)metadata);
	}
}

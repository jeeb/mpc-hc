/*
 * $Id$
 *
 * (C) 2006-2011 see AUTHORS
 *
 * This file is part of mplayerc.
 *
 * Mplayerc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mplayerc is distributed in the hope that it will be useful,
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
#include <atlbase.h>
#include <MMReg.h>

#include "PODtypes.h"
#include "avcodec.h"

#include <InitGuid.h>
#include "MPCVideoDecFilter.h"
#include "VideoDecOutputPin.h"
#include "CpuId.h"

#include "ffImgfmt.h"
extern "C"
{
#include "FfmpegContext.h"
#include "libswscale/swscale.h"
}

#include "../../../DSUtil/DSUtil.h"
#include "../../../DSUtil/MediaTypes.h"
#include "../../parser/MpegSplitter/MpegSplitter.h"
#include <moreuuids.h>
#include "DXVADecoderH264.h"
#include "../../../apps/mplayerc/FilterEnum.h"

#include "../../../apps/mplayerc/WinAPIUtils.h"


#define MAX_SUPPORTED_MODE			5
#define ROUND_FRAMERATE(var,FrameRate)	if (labs ((long)(var - FrameRate)) < FrameRate*1/100) var = FrameRate;

typedef struct {
	const int			PicEntryNumber;
	const UINT			PreferedConfigBitstream;
	const GUID*			Decoder[MAX_SUPPORTED_MODE];
	const WORD			RestrictedMode[MAX_SUPPORTED_MODE];
} DXVA_PARAMS;

typedef struct {
	const CLSID*		clsMinorType;
	const enum CodecID	nFFCodec;
	const int			fourcc;
	const DXVA_PARAMS*	DXVAModes;

	int					DXVAModeCount() {
		if (!DXVAModes) {
			return 0;
		}
		for (int i=0; i<MAX_SUPPORTED_MODE; i++) {
			if (DXVAModes->Decoder[i] == &GUID_NULL) {
				return i;
			}
		}
		return MAX_SUPPORTED_MODE;
	}
} FFMPEG_CODECS;

// DXVA modes supported for Mpeg2
DXVA_PARAMS		DXVA_Mpeg2 = {
	9,		// PicEntryNumber
	1,		// PreferedConfigBitstream
	{ &DXVA2_ModeMPEG2_VLD, &GUID_NULL },
	{ DXVA_RESTRICTED_MODE_UNRESTRICTED, 0 } // Restricted mode for DXVA1?
};

// DXVA modes supported for H264
DXVA_PARAMS		DXVA_H264 = {
	16,		// PicEntryNumber
	2,		// PreferedConfigBitstream
	{ &DXVA2_ModeH264_E, &DXVA2_ModeH264_F, &DXVA_Intel_H264_ClearVideo, &GUID_NULL },
	{ DXVA_RESTRICTED_MODE_H264_E, 0}
};

DXVA_PARAMS		DXVA_H264_VISTA = {
	22,		// PicEntryNumber
	2,		// PreferedConfigBitstream
	{ &DXVA2_ModeH264_E, &DXVA2_ModeH264_F, &DXVA_Intel_H264_ClearVideo, &GUID_NULL },
	{ DXVA_RESTRICTED_MODE_H264_E, 0}
};

// DXVA modes supported for VC1
DXVA_PARAMS		DXVA_VC1 = {
	14,		// PicEntryNumber
	1,		// PreferedConfigBitstream
	{ &DXVA2_ModeVC1_D,				&GUID_NULL },
	{ DXVA_RESTRICTED_MODE_VC1_D, 0}
};

FFMPEG_CODECS		ffCodecs[] = {
#if HAS_FFMPEG_VIDEO_DECODERS
	// Flash video
	{ &MEDIASUBTYPE_FLV1, CODEC_ID_FLV1, MAKEFOURCC('F','L','V','1'), NULL },
	{ &MEDIASUBTYPE_flv1, CODEC_ID_FLV1, MAKEFOURCC('f','l','v','1'), NULL },
	{ &MEDIASUBTYPE_FLV4, CODEC_ID_VP6F, MAKEFOURCC('F','L','V','4'), NULL },
	{ &MEDIASUBTYPE_flv4, CODEC_ID_VP6F, MAKEFOURCC('f','l','v','4'), NULL },
	{ &MEDIASUBTYPE_VP6F, CODEC_ID_VP6F, MAKEFOURCC('V','P','6','F'), NULL },
	{ &MEDIASUBTYPE_vp6f, CODEC_ID_VP6F, MAKEFOURCC('v','p','6','f'), NULL },

	// VP5
	{ &MEDIASUBTYPE_VP50, CODEC_ID_VP5,  MAKEFOURCC('V','P','5','0'), NULL },
	{ &MEDIASUBTYPE_vp50, CODEC_ID_VP5,  MAKEFOURCC('v','p','5','0'), NULL },

	// VP6
	{ &MEDIASUBTYPE_VP60, CODEC_ID_VP6,  MAKEFOURCC('V','P','6','0'), NULL },
	{ &MEDIASUBTYPE_vp60, CODEC_ID_VP6,  MAKEFOURCC('v','p','6','0'), NULL },
	{ &MEDIASUBTYPE_VP61, CODEC_ID_VP6,  MAKEFOURCC('V','P','6','1'), NULL },
	{ &MEDIASUBTYPE_vp61, CODEC_ID_VP6,  MAKEFOURCC('v','p','6','1'), NULL },
	{ &MEDIASUBTYPE_VP62, CODEC_ID_VP6,  MAKEFOURCC('V','P','6','2'), NULL },
	{ &MEDIASUBTYPE_vp62, CODEC_ID_VP6,  MAKEFOURCC('v','p','6','2'), NULL },
	{ &MEDIASUBTYPE_VP6A, CODEC_ID_VP6A, MAKEFOURCC('V','P','6','A'), NULL },
	{ &MEDIASUBTYPE_vp6a, CODEC_ID_VP6A, MAKEFOURCC('v','p','6','a'), NULL },

	// VP8
	{ &MEDIASUBTYPE_VP80, CODEC_ID_VP8, MAKEFOURCC('V','P','8','0'), NULL },

	// Xvid
	{ &MEDIASUBTYPE_XVID, CODEC_ID_MPEG4,  MAKEFOURCC('X','V','I','D'), NULL },
	{ &MEDIASUBTYPE_xvid, CODEC_ID_MPEG4,  MAKEFOURCC('x','v','i','d'), NULL },
	{ &MEDIASUBTYPE_XVIX, CODEC_ID_MPEG4,  MAKEFOURCC('X','V','I','X'), NULL },
	{ &MEDIASUBTYPE_xvix, CODEC_ID_MPEG4,  MAKEFOURCC('x','v','i','x'), NULL },

	// DivX
	{ &MEDIASUBTYPE_DX50, CODEC_ID_MPEG4,  MAKEFOURCC('D','X','5','0'), NULL },
	{ &MEDIASUBTYPE_dx50, CODEC_ID_MPEG4,  MAKEFOURCC('d','x','5','0'), NULL },
	{ &MEDIASUBTYPE_DIVX, CODEC_ID_MPEG4,  MAKEFOURCC('D','I','V','X'), NULL },
	{ &MEDIASUBTYPE_divx, CODEC_ID_MPEG4,  MAKEFOURCC('d','i','v','x'), NULL },

	// WMV1/2/3
	{ &MEDIASUBTYPE_WMV1, CODEC_ID_WMV1,  MAKEFOURCC('W','M','V','1'), NULL },
	{ &MEDIASUBTYPE_wmv1, CODEC_ID_WMV1,  MAKEFOURCC('w','m','v','1'), NULL },
	{ &MEDIASUBTYPE_WMV2, CODEC_ID_WMV2,  MAKEFOURCC('W','M','V','2'), NULL },
	{ &MEDIASUBTYPE_wmv2, CODEC_ID_WMV2,  MAKEFOURCC('w','m','v','2'), NULL },
	{ &MEDIASUBTYPE_WMV3, CODEC_ID_WMV3,  MAKEFOURCC('W','M','V','3'), NULL },
	{ &MEDIASUBTYPE_wmv3, CODEC_ID_WMV3,  MAKEFOURCC('w','m','v','3'), NULL },

	// MPEG-2
	{ &MEDIASUBTYPE_MPEG2_VIDEO, CODEC_ID_MPEG2VIDEO,  MAKEFOURCC('M','P','G','2'), &DXVA_Mpeg2 },

	// MSMPEG-4
	{ &MEDIASUBTYPE_DIV3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('D','I','V','3'), NULL },
	{ &MEDIASUBTYPE_div3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('d','i','v','3'), NULL },
	{ &MEDIASUBTYPE_DVX3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('D','V','X','3'), NULL },
	{ &MEDIASUBTYPE_dvx3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('d','v','x','3'), NULL },
	{ &MEDIASUBTYPE_MP43, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('M','P','4','3'), NULL },
	{ &MEDIASUBTYPE_mp43, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('m','p','4','3'), NULL },
	{ &MEDIASUBTYPE_COL1, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('C','O','L','1'), NULL },
	{ &MEDIASUBTYPE_col1, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('c','o','l','1'), NULL },
	{ &MEDIASUBTYPE_DIV4, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('D','I','V','4'), NULL },
	{ &MEDIASUBTYPE_div4, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('d','i','v','4'), NULL },
	{ &MEDIASUBTYPE_DIV5, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('D','I','V','5'), NULL },
	{ &MEDIASUBTYPE_div5, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('d','i','v','5'), NULL },
	{ &MEDIASUBTYPE_DIV6, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('D','I','V','6'), NULL },
	{ &MEDIASUBTYPE_div6, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('d','i','v','6'), NULL },
	{ &MEDIASUBTYPE_AP41, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('A','P','4','1'), NULL },
	{ &MEDIASUBTYPE_ap41, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('a','p','4','1'), NULL },
	{ &MEDIASUBTYPE_MPG3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('M','P','G','3'), NULL },
	{ &MEDIASUBTYPE_mpg3, CODEC_ID_MSMPEG4V3,  MAKEFOURCC('m','p','g','3'), NULL },
	{ &MEDIASUBTYPE_DIV2, CODEC_ID_MSMPEG4V2,  MAKEFOURCC('D','I','V','2'), NULL },
	{ &MEDIASUBTYPE_div2, CODEC_ID_MSMPEG4V2,  MAKEFOURCC('d','i','v','2'), NULL },
	{ &MEDIASUBTYPE_MP42, CODEC_ID_MSMPEG4V2,  MAKEFOURCC('M','P','4','2'), NULL },
	{ &MEDIASUBTYPE_mp42, CODEC_ID_MSMPEG4V2,  MAKEFOURCC('m','p','4','2'), NULL },
	{ &MEDIASUBTYPE_MPG4, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('M','P','G','4'), NULL },
	{ &MEDIASUBTYPE_mpg4, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('m','p','g','4'), NULL },
	{ &MEDIASUBTYPE_DIV1, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('D','I','V','1'), NULL },
	{ &MEDIASUBTYPE_div1, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('d','i','v','1'), NULL },
	{ &MEDIASUBTYPE_MP41, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('M','P','4','1'), NULL },
	{ &MEDIASUBTYPE_mp41, CODEC_ID_MSMPEG4V1,  MAKEFOURCC('m','p','4','1'), NULL },

	// AMV Video
	{ &MEDIASUBTYPE_AMVV, CODEC_ID_AMV,  MAKEFOURCC('A','M','V','V'), NULL },
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

	// H264/AVC
	{ &MEDIASUBTYPE_H264, CODEC_ID_H264, MAKEFOURCC('H','2','6','4'), &DXVA_H264 },
	{ &MEDIASUBTYPE_h264, CODEC_ID_H264, MAKEFOURCC('h','2','6','4'), &DXVA_H264 },
	{ &MEDIASUBTYPE_X264, CODEC_ID_H264, MAKEFOURCC('X','2','6','4'), &DXVA_H264 },
	{ &MEDIASUBTYPE_x264, CODEC_ID_H264, MAKEFOURCC('x','2','6','4'), &DXVA_H264 },
	{ &MEDIASUBTYPE_VSSH, CODEC_ID_H264, MAKEFOURCC('V','S','S','H'), &DXVA_H264 },
	{ &MEDIASUBTYPE_vssh, CODEC_ID_H264, MAKEFOURCC('v','s','s','h'), &DXVA_H264 },
	{ &MEDIASUBTYPE_DAVC, CODEC_ID_H264, MAKEFOURCC('D','A','V','C'), &DXVA_H264 },
	{ &MEDIASUBTYPE_davc, CODEC_ID_H264, MAKEFOURCC('d','a','v','c'), &DXVA_H264 },
	{ &MEDIASUBTYPE_PAVC, CODEC_ID_H264, MAKEFOURCC('P','A','V','C'), &DXVA_H264 },
	{ &MEDIASUBTYPE_pavc, CODEC_ID_H264, MAKEFOURCC('p','a','v','c'), &DXVA_H264 },
	{ &MEDIASUBTYPE_AVC1, CODEC_ID_H264, MAKEFOURCC('A','V','C','1'), &DXVA_H264 },
	{ &MEDIASUBTYPE_avc1, CODEC_ID_H264, MAKEFOURCC('a','v','c','1'), &DXVA_H264 },
	{ &MEDIASUBTYPE_H264_bis, CODEC_ID_H264, MAKEFOURCC('a','v','c','1'), &DXVA_H264 },

#if HAS_FFMPEG_VIDEO_DECODERS
	// SVQ3
	{ &MEDIASUBTYPE_SVQ3, CODEC_ID_SVQ3, MAKEFOURCC('S','V','Q','3'), NULL },

	// SVQ1
	{ &MEDIASUBTYPE_SVQ1, CODEC_ID_SVQ1, MAKEFOURCC('S','V','Q','1'), NULL },

	// H263
	{ &MEDIASUBTYPE_H263, CODEC_ID_H263, MAKEFOURCC('H','2','6','3'), NULL },
	{ &MEDIASUBTYPE_h263, CODEC_ID_H263, MAKEFOURCC('h','2','6','3'), NULL },

	{ &MEDIASUBTYPE_S263, CODEC_ID_H263, MAKEFOURCC('S','2','6','3'), NULL },
	{ &MEDIASUBTYPE_s263, CODEC_ID_H263, MAKEFOURCC('s','2','6','3'), NULL },

	// Real Video
	{ &MEDIASUBTYPE_RV10, CODEC_ID_RV10, MAKEFOURCC('R','V','1','0'), NULL },
	{ &MEDIASUBTYPE_RV20, CODEC_ID_RV20, MAKEFOURCC('R','V','2','0'), NULL },
	{ &MEDIASUBTYPE_RV30, CODEC_ID_RV30, MAKEFOURCC('R','V','3','0'), NULL },
	{ &MEDIASUBTYPE_RV40, CODEC_ID_RV40, MAKEFOURCC('R','V','4','0'), NULL },

	// Theora
	{ &MEDIASUBTYPE_THEORA, CODEC_ID_THEORA, MAKEFOURCC('T','H','E','O'), NULL },
	{ &MEDIASUBTYPE_theora, CODEC_ID_THEORA, MAKEFOURCC('t','h','e','o'), NULL },
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

	// WVC1
	{ &MEDIASUBTYPE_WVC1, CODEC_ID_VC1,  MAKEFOURCC('W','V','C','1'), &DXVA_VC1 },
	{ &MEDIASUBTYPE_wvc1, CODEC_ID_VC1,  MAKEFOURCC('w','v','c','1'), &DXVA_VC1 },

#if HAS_FFMPEG_VIDEO_DECODERS
	// Other MPEG-4
	{ &MEDIASUBTYPE_MP4V, CODEC_ID_MPEG4,  MAKEFOURCC('M','P','4','V'), NULL },
	{ &MEDIASUBTYPE_mp4v, CODEC_ID_MPEG4,  MAKEFOURCC('m','p','4','v'), NULL },
	{ &MEDIASUBTYPE_M4S2, CODEC_ID_MPEG4,  MAKEFOURCC('M','4','S','2'), NULL },
	{ &MEDIASUBTYPE_m4s2, CODEC_ID_MPEG4,  MAKEFOURCC('m','4','s','2'), NULL },
	{ &MEDIASUBTYPE_MP4S, CODEC_ID_MPEG4,  MAKEFOURCC('M','P','4','S'), NULL },
	{ &MEDIASUBTYPE_mp4s, CODEC_ID_MPEG4,  MAKEFOURCC('m','p','4','s'), NULL },
	{ &MEDIASUBTYPE_3IV1, CODEC_ID_MPEG4,  MAKEFOURCC('3','I','V','1'), NULL },
	{ &MEDIASUBTYPE_3iv1, CODEC_ID_MPEG4,  MAKEFOURCC('3','i','v','1'), NULL },
	{ &MEDIASUBTYPE_3IV2, CODEC_ID_MPEG4,  MAKEFOURCC('3','I','V','2'), NULL },
	{ &MEDIASUBTYPE_3iv2, CODEC_ID_MPEG4,  MAKEFOURCC('3','i','v','2'), NULL },
	{ &MEDIASUBTYPE_3IVX, CODEC_ID_MPEG4,  MAKEFOURCC('3','I','V','X'), NULL },
	{ &MEDIASUBTYPE_3ivx, CODEC_ID_MPEG4,  MAKEFOURCC('3','i','v','x'), NULL },
	{ &MEDIASUBTYPE_BLZ0, CODEC_ID_MPEG4,  MAKEFOURCC('B','L','Z','0'), NULL },
	{ &MEDIASUBTYPE_blz0, CODEC_ID_MPEG4,  MAKEFOURCC('b','l','z','0'), NULL },
	{ &MEDIASUBTYPE_DM4V, CODEC_ID_MPEG4,  MAKEFOURCC('D','M','4','V'), NULL },
	{ &MEDIASUBTYPE_dm4v, CODEC_ID_MPEG4,  MAKEFOURCC('d','m','4','v'), NULL },
	{ &MEDIASUBTYPE_FFDS, CODEC_ID_MPEG4,  MAKEFOURCC('F','F','D','S'), NULL },
	{ &MEDIASUBTYPE_ffds, CODEC_ID_MPEG4,  MAKEFOURCC('f','f','d','s'), NULL },
	{ &MEDIASUBTYPE_FVFW, CODEC_ID_MPEG4,  MAKEFOURCC('F','V','F','W'), NULL },
	{ &MEDIASUBTYPE_fvfw, CODEC_ID_MPEG4,  MAKEFOURCC('f','v','f','w'), NULL },
	{ &MEDIASUBTYPE_DXGM, CODEC_ID_MPEG4,  MAKEFOURCC('D','X','G','M'), NULL },
	{ &MEDIASUBTYPE_dxgm, CODEC_ID_MPEG4,  MAKEFOURCC('d','x','g','m'), NULL },
	{ &MEDIASUBTYPE_FMP4, CODEC_ID_MPEG4,  MAKEFOURCC('F','M','P','4'), NULL },
	{ &MEDIASUBTYPE_fmp4, CODEC_ID_MPEG4,  MAKEFOURCC('f','m','p','4'), NULL },
	{ &MEDIASUBTYPE_HDX4, CODEC_ID_MPEG4,  MAKEFOURCC('H','D','X','4'), NULL },
	{ &MEDIASUBTYPE_hdx4, CODEC_ID_MPEG4,  MAKEFOURCC('h','d','x','4'), NULL },
	{ &MEDIASUBTYPE_LMP4, CODEC_ID_MPEG4,  MAKEFOURCC('L','M','P','4'), NULL },
	{ &MEDIASUBTYPE_lmp4, CODEC_ID_MPEG4,  MAKEFOURCC('l','m','p','4'), NULL },
	{ &MEDIASUBTYPE_NDIG, CODEC_ID_MPEG4,  MAKEFOURCC('N','D','I','G'), NULL },
	{ &MEDIASUBTYPE_ndig, CODEC_ID_MPEG4,  MAKEFOURCC('n','d','i','g'), NULL },
	{ &MEDIASUBTYPE_RMP4, CODEC_ID_MPEG4,  MAKEFOURCC('R','M','P','4'), NULL },
	{ &MEDIASUBTYPE_rmp4, CODEC_ID_MPEG4,  MAKEFOURCC('r','m','p','4'), NULL },
	{ &MEDIASUBTYPE_SMP4, CODEC_ID_MPEG4,  MAKEFOURCC('S','M','P','4'), NULL },
	{ &MEDIASUBTYPE_smp4, CODEC_ID_MPEG4,  MAKEFOURCC('s','m','p','4'), NULL },
	{ &MEDIASUBTYPE_SEDG, CODEC_ID_MPEG4,  MAKEFOURCC('S','E','D','G'), NULL },
	{ &MEDIASUBTYPE_sedg, CODEC_ID_MPEG4,  MAKEFOURCC('s','e','d','g'), NULL },
	{ &MEDIASUBTYPE_UMP4, CODEC_ID_MPEG4,  MAKEFOURCC('U','M','P','4'), NULL },
	{ &MEDIASUBTYPE_ump4, CODEC_ID_MPEG4,  MAKEFOURCC('u','m','p','4'), NULL },
	{ &MEDIASUBTYPE_WV1F, CODEC_ID_MPEG4,  MAKEFOURCC('W','V','1','F'), NULL },
	{ &MEDIASUBTYPE_wv1f, CODEC_ID_MPEG4,  MAKEFOURCC('w','v','1','f'), NULL }
#endif /* HAS_FFMPEG_VIDEO_DECODERS */
};

/* Important: the order should be exactly the same as in ffCodecs[] */
const AMOVIESETUP_MEDIATYPE CMPCVideoDecFilter::sudPinTypesIn[] = {
#if HAS_FFMPEG_VIDEO_DECODERS
	// Flash video
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_FLV1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_flv1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_FLV4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_flv4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP6F   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp6f   },

	// VP5
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP50   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp50   },

	// VP6
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP60   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp60   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP61   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp61   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP62   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp62   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP6A   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vp6a   },

	// VP8
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VP80   },

	// Xvid
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_XVID   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_xvid   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_XVIX   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_xvix   },

	// DivX
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DX50   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_dx50   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIVX   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_divx   },

	// WMV1/2/3
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_WMV1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_wmv1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_WMV2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_wmv2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_WMV3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_wmv3   },

	// MPEG-2
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MPEG2_VIDEO },

	// MSMPEG-4
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DVX3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_dvx3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MP43   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mp43   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_COL1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_col1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV5   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div5   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV6   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div6   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_AP41   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_ap41   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MPG3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mpg3   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MP42   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mp42   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MPG4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mpg4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DIV1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_div1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MP41   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mp41   },

	// AMV Video
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_AMVV   },
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

	// H264/AVC
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_H264   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_h264   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_X264   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_x264   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_VSSH   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_vssh   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DAVC   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_davc   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_PAVC   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_pavc   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_AVC1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_avc1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_H264_bis },

#if HAS_FFMPEG_VIDEO_DECODERS
	// SVQ3
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ3   },

	// SVQ1
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_SVQ1   },

	// H263
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_H263   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_h263   },

	{ &MEDIATYPE_Video, &MEDIASUBTYPE_S263   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_s263   },

	// Real video
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_RV10   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_RV20   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_RV30   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_RV40   },

	// Theora
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_THEORA },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_theora },
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

	// VC1
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_WVC1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_wvc1   },

#if HAS_FFMPEG_VIDEO_DECODERS
	// IMPORTANT : some of the last MediaTypes present in next group may be not available in
	// the standalone filter (workaround to prevent GraphEdit crash).
	// Other MPEG-4
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MP4V   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mp4v   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_M4S2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_m4s2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_MP4S   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_mp4s   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3IV1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3iv1   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3IV2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3iv2   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3IVX   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_3ivx   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_BLZ0   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_blz0   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DM4V   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_dm4v   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_FFDS   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_ffds   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_FVFW   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_fvfw   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_DXGM   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_dxgm   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_FMP4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_fmp4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_HDX4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_hdx4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_LMP4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_lmp4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_NDIG   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_ndig   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_RMP4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_rmp4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_SMP4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_smp4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_SEDG   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_sedg   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_UMP4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_ump4   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_WV1F   },
	{ &MEDIATYPE_Video, &MEDIASUBTYPE_wv1f   }
#endif /* HAS_FFMPEG_VIDEO_DECODERS */
};

const int CMPCVideoDecFilter::sudPinTypesInCount = countof(CMPCVideoDecFilter::sudPinTypesIn);

bool*		CMPCVideoDecFilter::FFmpegFilters = NULL;
bool*		CMPCVideoDecFilter::DXVAFilters = NULL;

const AMOVIESETUP_MEDIATYPE CMPCVideoDecFilter::sudPinTypesOut[] = {
	{&MEDIATYPE_Video, &MEDIASUBTYPE_NV12},
	{&MEDIATYPE_Video, &MEDIASUBTYPE_NV24}
};
const int CMPCVideoDecFilter::sudPinTypesOutCount = countof(CMPCVideoDecFilter::sudPinTypesOut);

BOOL CALLBACK EnumFindProcessWnd (HWND hwnd, LPARAM lParam)
{
	DWORD	procid = 0;
	TCHAR	WindowClass [40];
	GetWindowThreadProcessId (hwnd, &procid);
	GetClassName (hwnd, WindowClass, countof(WindowClass));

	if (procid == GetCurrentProcessId() && _tcscmp (WindowClass, _T("MediaPlayerClassicW")) == 0) {
		HWND*		pWnd = (HWND*) lParam;
		*pWnd = hwnd;
		return FALSE;
	}
	return TRUE;
}

CMPCVideoDecFilter::CMPCVideoDecFilter(LPUNKNOWN lpunk, HRESULT* phr)
	: CBaseVideoFilter(NAME("MPC - Video decoder"), lpunk, phr, __uuidof(this))
{
	HWND		hWnd = NULL;

	if(IsVistaOrAbove()) {
		for (int i=0; i<countof(ffCodecs); i++) {
			if(ffCodecs[i].nFFCodec == CODEC_ID_H264) {
				ffCodecs[i].DXVAModes = &DXVA_H264_VISTA;
			}
		}
	}

	if(phr) {
		*phr = S_OK;
	}

	if (m_pOutput)	{
		delete m_pOutput;
	}
	m_pOutput = DNew CVideoDecOutputPin(NAME("CVideoDecOutputPin"), this, phr, L"Output");
	if(!m_pOutput) {
		*phr = E_OUTOFMEMORY;
	}

	m_pCpuId				= DNew CCpuId();
	m_pAVCodec				= NULL;
	m_pAVCtx				= NULL;
	m_pFrame				= NULL;
	m_nCodecNb				= -1;
	m_nCodecId				= -1;
	m_rtAvrTimePerFrame		= 0;
	m_bReorderBFrame		= true;
	m_DXVADecoderGUID		= GUID_NULL;
	m_nActiveCodecs			= MPCVD_H264|MPCVD_VC1|MPCVD_XVID|MPCVD_DIVX|MPCVD_MSMPEG4|MPCVD_FLASH|MPCVD_WMV|MPCVD_H263|MPCVD_SVQ3|MPCVD_AMVV|MPCVD_THEORA|MPCVD_H264_DXVA|MPCVD_VC1_DXVA|MPCVD_VP6|MPCVD_VP8;
	m_rtLastStart			= 0;
	m_nCountEstimated		= 0;

	m_nWorkaroundBug		= FF_BUG_AUTODETECT;
	m_nErrorConcealment		= FF_EC_DEBLOCK | FF_EC_GUESS_MVS;

	m_nThreadNumber			= 0;
	m_nDiscardMode			= AVDISCARD_DEFAULT;
	m_nErrorRecognition		= FF_ER_CAREFUL;
	m_nIDCTAlgo				= FF_IDCT_AUTO;
	m_bDXVACompatible		= true;
	m_pFFBuffer				= NULL;
	m_nFFBufferSize			= 0;
	ResetBuffer();

	m_nWidth				= 0;
	m_nHeight				= 0;
	m_pSwsContext			= NULL;

	m_bUseDXVA = true;
	m_bUseFFmpeg = true;

	m_nDXVAMode				= MODE_SOFTWARE;
	m_pDXVADecoder			= NULL;
	m_pVideoOutputFormat	= NULL;
	m_nVideoOutputCount		= 0;
	m_hDevice				= INVALID_HANDLE_VALUE;

	m_nARMode					= 1;
	m_nDXVACheckCompatibility	= 1; // skip level check by default
	m_nDXVA_SD					= 0;
	m_nPosB						= 1;
	m_sar.SetSize(1,1);

	m_bWaitingForKeyFrame = TRUE;

#ifdef REGISTER_FILTER
	CRegKey key;
	if(ERROR_SUCCESS == key.Open(HKEY_CURRENT_USER, _T("Software\\Gabest\\Filters\\MPC Video Decoder"), KEY_READ)) {
		DWORD dw;
		#if HAS_FFMPEG_VIDEO_DECODERS
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("ThreadNumber"), dw)) {
			m_nThreadNumber = dw;
		}
			#if INTERNAL_DECODER_H264
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("DiscardMode"), dw)) {
			m_nDiscardMode = dw;
		}
			#endif
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("ErrorRecognition"), dw)) {
			m_nErrorRecognition = dw;
		}
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("IDCTAlgo"), dw)) {
			m_nIDCTAlgo = dw;
		}
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("ActiveCodecs"), dw)) {
			m_nActiveCodecs = dw;
		}
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("ARMode"), dw)) {
			m_nARMode = dw;
		}
		#endif
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("DXVACheckCompatibility"), dw)) {
			m_nDXVACheckCompatibility = dw;
		}
		if(ERROR_SUCCESS == key.QueryDWORDValue(_T("DisableDXVA_SD"), dw)) {
			m_nDXVA_SD = dw;
		}
	}
#else
	#if HAS_FFMPEG_VIDEO_DECODERS
	m_nThreadNumber = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("ThreadNumber"), m_nThreadNumber);
		#if INTERNAL_DECODER_H264
	m_nDiscardMode = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("DiscardMode"), m_nDiscardMode);
		#endif
	m_nErrorRecognition = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("ErrorRecognition"), m_nErrorRecognition);
	m_nIDCTAlgo = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("IDCTAlgo"), m_nIDCTAlgo);
	m_nARMode = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("ARMode"), m_nARMode);
	#endif
	m_nDXVACheckCompatibility = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("DXVACheckCompatibility"), m_nDXVACheckCompatibility);
	m_nDXVA_SD = AfxGetApp()->GetProfileInt(_T("Filters\\MPC Video Decoder"), _T("DisableDXVA_SD"), m_nDXVA_SD);
#endif

	if(m_nDXVACheckCompatibility > 3) {
		m_nDXVACheckCompatibility = 1;    // skip level check by default
	}

	ff_avcodec_default_get_buffer		= avcodec_default_get_buffer;
	ff_avcodec_default_release_buffer	= avcodec_default_release_buffer;
	ff_avcodec_default_reget_buffer		= avcodec_default_reget_buffer;

	avcodec_register_all();
	av_log_set_callback(LogLibAVCodec);

	EnumWindows(EnumFindProcessWnd, (LPARAM)&hWnd);
	DetectVideoCard(hWnd);

#ifdef _DEBUG
	// Check codec definition table
	int		nCodecs	  = countof(ffCodecs);
	int		nPinTypes = countof(sudPinTypesIn);
	ASSERT (nCodecs == nPinTypes);
	for (int i=0; i<nPinTypes; i++) {
		ASSERT (ffCodecs[i].clsMinorType == sudPinTypesIn[i].clsMinorType);
	}
#endif
}

void CMPCVideoDecFilter::DetectVideoCard(HWND hWnd)
{
	IDirect3D9* pD3D9;
	m_nPCIVendor = 0;
	m_nPCIDevice = 0;
	m_VideoDriverVersion.HighPart = 0;
	m_VideoDriverVersion.LowPart = 0;

	pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (pD3D9) {
		D3DADAPTER_IDENTIFIER9 adapterIdentifier;
		if (pD3D9->GetAdapterIdentifier(GetAdapter(pD3D9, hWnd), 0, &adapterIdentifier) == S_OK) {
			m_nPCIVendor = adapterIdentifier.VendorId;
			m_nPCIDevice = adapterIdentifier.DeviceId;
			m_VideoDriverVersion = adapterIdentifier.DriverVersion;
			m_strDeviceDescription = adapterIdentifier.Description;
			m_strDeviceDescription.AppendFormat (_T(" (%04X:%04X)"), m_nPCIVendor, m_nPCIDevice);
		}
		pD3D9->Release();
	}
}

CMPCVideoDecFilter::~CMPCVideoDecFilter()
{
	Cleanup();

	SAFE_DELETE(m_pCpuId);
}

bool CMPCVideoDecFilter::IsVideoInterlaced()
{
	// NOT A BUG : always tell DirectShow it's interlaced (progressive flags set in
	// SetTypeSpecificFlags function)
	return true;
};

void CMPCVideoDecFilter::UpdateFrameTime (REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	if (rtStart == _I64_MIN) {
		// If reference time has not been set by splitter, extrapolate start time
		// from last known start time already delivered
		rtStart = m_rtLastStart + (m_rtAvrTimePerFrame / m_dRate) * m_nCountEstimated;
		rtStop  = rtStart + (m_rtAvrTimePerFrame / m_dRate);
		m_nCountEstimated++;
	} else {
		// Known start time, set as new reference
		m_rtLastStart		= rtStart;
		m_nCountEstimated	= 1;
	}
}

void CMPCVideoDecFilter::GetOutputSize(int& w, int& h, int& arx, int& ary, int& RealWidth, int& RealHeight)
{
#if 1
	RealWidth = m_nWidth;
	RealHeight = m_nHeight;
	w = PictWidthRounded();
	h = PictHeightRounded();
#else
	if (m_nDXVAMode == MODE_SOFTWARE) {
		w = m_nWidth;
		h = m_nHeight;
	} else {
		// DXVA surface are multiple of 16 pixels!
		w = PictWidthRounded();
		h = PictHeightRounded();
	}
#endif
}

int CMPCVideoDecFilter::PictWidth()
{
	return m_nWidth;
}

int CMPCVideoDecFilter::PictHeight()
{
	return m_nHeight;
}

int CMPCVideoDecFilter::PictWidthRounded()
{
	// Picture height should be rounded to 16 for DXVA
	return ((m_nWidth + 15) / 16) * 16;
}

int CMPCVideoDecFilter::PictHeightRounded()
{
	// Picture height should be rounded to 16 for DXVA
	return ((m_nHeight + 15) / 16) * 16;
}

int CMPCVideoDecFilter::FindCodec(const CMediaType* mtIn)
{
	for (int i=0; i<countof(ffCodecs); i++)
		if (mtIn->subtype == *ffCodecs[i].clsMinorType) {
#ifndef REGISTER_FILTER
			switch (ffCodecs[i].nFFCodec) {
				case CODEC_ID_H264 :
#if INTERNAL_DECODER_H264_DXVA
					m_bUseDXVA = DXVAFilters && DXVAFilters[TRA_DXVA_H264];
#else
					m_bUseDXVA = false;
#endif
#if INTERNAL_DECODER_H264
					m_bUseFFmpeg = FFmpegFilters && FFmpegFilters[FFM_H264];
#else
					m_bUseFFmpeg = false;
#endif
					break;
				case CODEC_ID_VC1 :
#if INTERNAL_DECODER_VC1_DXVA
					m_bUseDXVA = DXVAFilters && DXVAFilters[TRA_DXVA_VC1];
#else
					m_bUseDXVA = false;
#endif
#if INTERNAL_DECODER_VC1
					m_bUseFFmpeg = FFmpegFilters && FFmpegFilters[FFM_VC1];
#else
					m_bUseFFmpeg = false;
#endif
					break;
				case CODEC_ID_MPEG2VIDEO :
#if INTERNAL_DECODER_MPEG2_DXVA
					m_bUseDXVA = true;
#endif
					m_bUseFFmpeg = false; // No Mpeg2 software support with ffmpeg!
					break;
				default :
					m_bUseDXVA = false;
			}

			return ((m_bUseDXVA || m_bUseFFmpeg) ? i : -1);
#else
			bool	bCodecActivated = false;
			switch (ffCodecs[i].nFFCodec) {
				case CODEC_ID_FLV1 :
				case CODEC_ID_VP6F :
					bCodecActivated = (m_nActiveCodecs & MPCVD_FLASH) != 0;
					break;
				case CODEC_ID_MPEG4 :
					if ((*ffCodecs[i].clsMinorType == MEDIASUBTYPE_XVID) ||
							(*ffCodecs[i].clsMinorType == MEDIASUBTYPE_xvid) ||
							(*ffCodecs[i].clsMinorType == MEDIASUBTYPE_XVIX) ||
							(*ffCodecs[i].clsMinorType == MEDIASUBTYPE_xvix) ) {
						bCodecActivated = (m_nActiveCodecs & MPCVD_XVID) != 0;
					} else if ((*ffCodecs[i].clsMinorType == MEDIASUBTYPE_DX50) ||
							   (*ffCodecs[i].clsMinorType == MEDIASUBTYPE_dx50) ||
							   (*ffCodecs[i].clsMinorType == MEDIASUBTYPE_DIVX) ||
							   (*ffCodecs[i].clsMinorType == MEDIASUBTYPE_divx) ) {
						bCodecActivated = (m_nActiveCodecs & MPCVD_DIVX) != 0;
					}
					break;
				case CODEC_ID_WMV1 :
				case CODEC_ID_WMV2 :
				case CODEC_ID_WMV3 :
					bCodecActivated = (m_nActiveCodecs & MPCVD_WMV) != 0;
					break;
				case CODEC_ID_MSMPEG4V3 :
				case CODEC_ID_MSMPEG4V2 :
				case CODEC_ID_MSMPEG4V1 :
					bCodecActivated = (m_nActiveCodecs & MPCVD_MSMPEG4) != 0;
					break;
				case CODEC_ID_H264 :
					m_bUseDXVA = (m_nActiveCodecs & MPCVD_H264_DXVA) != 0;
					m_bUseFFmpeg = (m_nActiveCodecs & MPCVD_H264) != 0;
					bCodecActivated = m_bUseDXVA || m_bUseFFmpeg;
					break;
				case CODEC_ID_SVQ3 :
				case CODEC_ID_SVQ1 :
					bCodecActivated = (m_nActiveCodecs & MPCVD_SVQ3) != 0;
					break;
				case CODEC_ID_H263 :
					bCodecActivated = (m_nActiveCodecs & MPCVD_H263) != 0;
					break;
				case CODEC_ID_THEORA :
					bCodecActivated = (m_nActiveCodecs & MPCVD_THEORA) != 0;
					break;
				case CODEC_ID_VC1 :
					m_bUseDXVA = (m_nActiveCodecs & MPCVD_VC1_DXVA) != 0;
					m_bUseFFmpeg = (m_nActiveCodecs & MPCVD_VC1) != 0;
					bCodecActivated = m_bUseDXVA || m_bUseFFmpeg;
					break;
				case CODEC_ID_AMV :
					bCodecActivated = (m_nActiveCodecs & MPCVD_AMVV) != 0;
					break;
				case CODEC_ID_VP5  :
				case CODEC_ID_VP6  :
				case CODEC_ID_VP6A :
					bCodecActivated = (m_nActiveCodecs & MPCVD_VP6) != 0;
					break;
				case CODEC_ID_VP8  :
					bCodecActivated = (m_nActiveCodecs & MPCVD_VP8) != 0;
					break;
			}
			return (bCodecActivated ? i : -1);
#endif
		}

	return -1;
}

void CMPCVideoDecFilter::Cleanup()
{
	SAFE_DELETE (m_pDXVADecoder);

	// Release FFMpeg
	if (m_pAVCtx) {
		if (m_pAVCtx->intra_matrix) {
			free(m_pAVCtx->intra_matrix);
		}
		if (m_pAVCtx->inter_matrix) {
			free(m_pAVCtx->inter_matrix);
		}
		if (m_pAVCtx->extradata) {
			free((unsigned char*)m_pAVCtx->extradata);
		}
		if (m_pFFBuffer) {
			free(m_pFFBuffer);
		}

		if (m_pAVCtx->slice_offset) {
			av_freep(&m_pAVCtx->slice_offset);
		}
		if (m_pAVCtx->codec) {
			avcodec_close(m_pAVCtx);
		}

		// Free thread resource if necessary
		FFSetThreadNumber (m_pAVCtx, m_pAVCtx->codec_id, 0);

		av_freep(&m_pAVCtx);
	}
	if (m_pFrame)	{
		av_freep(&m_pFrame);
	}

#if HAS_FFMPEG_VIDEO_DECODERS
	if (m_pSwsContext) {
		sws_freeContext(m_pSwsContext);
		m_pSwsContext = NULL;
	}
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

	m_pAVCodec		= NULL;
	m_pAVCtx		= NULL;
	m_pFrame		= NULL;
	m_pFFBuffer		= NULL;
	m_nFFBufferSize	= 0;
	m_nFFBufferPos	= 0;
	m_nFFPicEnd		= INT_MIN;
	m_nCodecNb		= -1;
	m_nCodecId		= -1;
	SAFE_DELETE_ARRAY (m_pVideoOutputFormat);

	// Release DXVA ressources
	if (m_hDevice != INVALID_HANDLE_VALUE) {
		m_pDeviceManager->CloseDeviceHandle(m_hDevice);
		m_hDevice = INVALID_HANDLE_VALUE;
	}

	m_pDeviceManager		= NULL;
	m_pDecoderService		= NULL;
	m_pDecoderRenderTarget	= NULL;
}

void CMPCVideoDecFilter::CalcAvgTimePerFrame()
{
	CMediaType &mt = m_pInput->CurrentMediaType();
	if (mt.formattype==FORMAT_VideoInfo) {
		m_rtAvrTimePerFrame = ((VIDEOINFOHEADER*)mt.pbFormat)->AvgTimePerFrame;
	} else if (mt.formattype==FORMAT_VideoInfo2) {
		m_rtAvrTimePerFrame = ((VIDEOINFOHEADER2*)mt.pbFormat)->AvgTimePerFrame;
	} else if (mt.formattype==FORMAT_MPEGVideo) {
		m_rtAvrTimePerFrame = ((MPEG1VIDEOINFO*)mt.pbFormat)->hdr.AvgTimePerFrame;
	} else if (mt.formattype==FORMAT_MPEG2Video) {
		m_rtAvrTimePerFrame = ((MPEG2VIDEOINFO*)mt.pbFormat)->hdr.AvgTimePerFrame;
	} else {
		ASSERT (FALSE);
		m_rtAvrTimePerFrame	= 1;
	}

	m_rtAvrTimePerFrame = max (1, m_rtAvrTimePerFrame);
}

void CMPCVideoDecFilter::LogLibAVCodec(void* par,int level,const char *fmt,va_list valist)
{
#if defined(_DEBUG) && 0
	char		Msg [500];
	vsnprintf_s (Msg, sizeof(Msg), _TRUNCATE, fmt, valist);
	TRACE("AVLIB : %s", Msg);
#endif
}

void CMPCVideoDecFilter::OnGetBuffer(AVFrame *pic)
{
	// Callback from FFMpeg to store Ref Time in frame (needed to have correct rtStart after avcodec_decode_video calls)
	//	pic->rtStart	= m_rtStart;
}

STDMETHODIMP CMPCVideoDecFilter::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	return
		QI(IMPCVideoDecFilter)
		QI(ISpecifyPropertyPages)
		QI(ISpecifyPropertyPages2)
		__super::NonDelegatingQueryInterface(riid, ppv);
}



HRESULT CMPCVideoDecFilter::CheckInputType(const CMediaType* mtIn)
{
	for (int i=0; i<sizeof(sudPinTypesIn)/sizeof(AMOVIESETUP_MEDIATYPE); i++) {
		if ((mtIn->majortype == *sudPinTypesIn[i].clsMajorType) &&
				(mtIn->subtype == *sudPinTypesIn[i].clsMinorType)) {
			return S_OK;
		}
	}

	return VFW_E_TYPE_NOT_ACCEPTED;
}

bool CMPCVideoDecFilter::IsMultiThreadSupported(int nCodec)
{
	return
	(
		nCodec==CODEC_ID_H264 || 
		nCodec==CODEC_ID_MPEG1VIDEO ||
		nCodec==CODEC_ID_FFV1 ||
		nCodec==CODEC_ID_DVVIDEO ||
		nCodec==CODEC_ID_VP8
	);
}

HRESULT CMPCVideoDecFilter::SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt)
{
	if (direction == PINDIR_INPUT) {

		int nNewCodec = FindCodec(pmt);

		if (nNewCodec == -1) {
			return VFW_E_TYPE_NOT_ACCEPTED;
		}

		if (nNewCodec != m_nCodecNb) {
			m_nCodecNb	= nNewCodec;

			m_bReorderBFrame	= true;
			m_nCodecId			= ffCodecs[nNewCodec].nFFCodec;
			m_pAVCodec			= avcodec_find_decoder((CodecID)m_nCodecId);
			CheckPointer (m_pAVCodec, VFW_E_UNSUPPORTED_VIDEO);

			m_pAVCtx	= avcodec_alloc_context3(m_pAVCodec);
			CheckPointer (m_pAVCtx,	  E_POINTER);

			int nThreadNumber = m_nThreadNumber ? m_nThreadNumber : m_pCpuId->GetProcessorNumber();
			if ((nThreadNumber > 1) && IsMultiThreadSupported (m_nCodecId)) {
				FFSetThreadNumber(m_pAVCtx, m_nCodecId, IsDXVASupported() ? 1 : nThreadNumber);
			}

			m_pAVCtx->h264_using_dxva = IsDXVASupported();

			m_pFrame = avcodec_alloc_frame();
			CheckPointer (m_pFrame,	  E_POINTER);

			m_h264RandomAccess.flush(m_pAVCtx->thread_count);

			if(pmt->formattype == FORMAT_VideoInfo) {
				VIDEOINFOHEADER*	vih = (VIDEOINFOHEADER*)pmt->pbFormat;
				m_pAVCtx->width		= vih->bmiHeader.biWidth;
				m_pAVCtx->height	= abs(vih->bmiHeader.biHeight);
				m_pAVCtx->codec_tag	= vih->bmiHeader.biCompression;
			} else if(pmt->formattype == FORMAT_VideoInfo2) {
				VIDEOINFOHEADER2*	vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
				m_pAVCtx->width		= vih2->bmiHeader.biWidth;
				m_pAVCtx->height	= abs(vih2->bmiHeader.biHeight);
				m_pAVCtx->codec_tag	= vih2->bmiHeader.biCompression;
			} else if(pmt->formattype == FORMAT_MPEGVideo) {
				MPEG1VIDEOINFO*		mpgv = (MPEG1VIDEOINFO*)pmt->pbFormat;
				m_pAVCtx->width		= mpgv->hdr.bmiHeader.biWidth;
				m_pAVCtx->height	= abs(mpgv->hdr.bmiHeader.biHeight);
				m_pAVCtx->codec_tag	= mpgv->hdr.bmiHeader.biCompression;
			} else if(pmt->formattype == FORMAT_MPEG2Video) {
				MPEG2VIDEOINFO*		mpg2v = (MPEG2VIDEOINFO*)pmt->pbFormat;
				m_pAVCtx->width		= mpg2v->hdr.bmiHeader.biWidth;
				m_pAVCtx->height	= abs(mpg2v->hdr.bmiHeader.biHeight);
				m_pAVCtx->codec_tag	= mpg2v->hdr.bmiHeader.biCompression;

				if (mpg2v->hdr.bmiHeader.biCompression == NULL) {
					m_pAVCtx->codec_tag = pmt->subtype.Data1;
				} else if ( (m_pAVCtx->codec_tag == MAKEFOURCC('a','v','c','1')) || (m_pAVCtx->codec_tag == MAKEFOURCC('A','V','C','1'))) {
					m_pAVCtx->nal_length_size = mpg2v->dwFlags;
					m_bReorderBFrame = false;
				}
			}
			m_nWidth	= m_pAVCtx->width;
			m_nHeight	= m_pAVCtx->height;

			m_pAVCtx->intra_matrix			= (uint16_t*)calloc(sizeof(uint16_t),64);
			m_pAVCtx->inter_matrix			= (uint16_t*)calloc(sizeof(uint16_t),64);
			m_pAVCtx->codec_tag				= ffCodecs[nNewCodec].fourcc;
			m_pAVCtx->workaround_bugs		= m_nWorkaroundBug;
			m_pAVCtx->error_concealment		= m_nErrorConcealment;
			m_pAVCtx->error_recognition		= m_nErrorRecognition;
			m_pAVCtx->idct_algo				= m_nIDCTAlgo;
			m_pAVCtx->skip_loop_filter		= (AVDiscard)m_nDiscardMode;
			m_pAVCtx->dsp_mask				= AV_CPU_FLAG_FORCE | m_pCpuId->GetFeatures();

			m_pAVCtx->debug_mv				= 0;

			m_pAVCtx->opaque				= this;
			m_pAVCtx->get_buffer			= get_buffer;

			AllocExtradata (m_pAVCtx, pmt);
			ConnectTo (m_pAVCtx);
			CalcAvgTimePerFrame();

			if (avcodec_open2(m_pAVCtx, m_pAVCodec, NULL)<0) {
				return VFW_E_INVALIDMEDIATYPE;
			}

			if (IsDXVASupported()) {
				do {
					m_bDXVACompatible = false;

					if(!DXVACheckFramesize(PictWidth(), PictHeight(), m_nPCIVendor)) { // check frame size
						break;
					}

					if (m_nCodecId == CODEC_ID_H264) {
						if (m_nDXVA_SD && PictWidthRounded() < 1280) { // check "Disable DXVA for SD" option
							break;
						}
						int nCompat = FFH264CheckCompatibility (PictWidthRounded(), PictHeightRounded(), m_pAVCtx, (BYTE*)m_pAVCtx->extradata, m_pAVCtx->extradata_size, m_nPCIVendor, m_nPCIDevice, m_VideoDriverVersion);
						if(nCompat) {
							if ( nCompat == DXVA_HIGH_BIT       ||
								 m_nDXVACheckCompatibility == 0 || // full check
								 m_nDXVACheckCompatibility == 1 && nCompat != DXVA_UNSUPPORTED_LEVEL ||   // skip level check
								 m_nDXVACheckCompatibility == 2 && nCompat != DXVA_TOO_MANY_REF_FRAMES) { // skip reference frame check
								break;
							}
						}
					/*} else if (m_nCodecId == CODEC_ID_VC1) {
						if (!VC1CheckCompatibility(m_pAVCtx, (BYTE*)m_pAVCtx->extradata, m_pAVCtx->extradata_size)) {
							break;
						}
					*/
					} else if (m_nCodecId == CODEC_ID_MPEG2VIDEO) {
						// DSP is disable for DXVA decoding (to keep default idct_permutation)
						m_pAVCtx->dsp_mask ^= AV_CPU_FLAG_FORCE;
						if (!MPEG2CheckCompatibility(m_pAVCtx, m_pFrame)) {
							break;
						}
					}

					m_bDXVACompatible = true;
				} while (false);
			}

			if (IsDXVASupported() && !m_bDXVACompatible) {
				m_bUseDXVA = false;
				avcodec_close (m_pAVCtx);
				if ((nThreadNumber > 1) && IsMultiThreadSupported (m_nCodecId)) {
					FFSetThreadNumber(m_pAVCtx, m_nCodecId, nThreadNumber);
				}
				m_pAVCtx->h264_using_dxva = 0;
				if (avcodec_open2(m_pAVCtx, m_pAVCodec, NULL)<0) {
					return VFW_E_INVALIDMEDIATYPE;
				}
			}

			BuildDXVAOutputFormat();
		}
	}

	return __super::SetMediaType(direction, pmt);
}

VIDEO_OUTPUT_FORMATS DXVAFormats[] = { // DXVA2
	{&MEDIASUBTYPE_NV12, 1, 12, 'avxd'},
	{&MEDIASUBTYPE_NV12, 1, 12, 'AVXD'},
	{&MEDIASUBTYPE_NV12, 1, 12, 'AVxD'},
	{&MEDIASUBTYPE_NV12, 1, 12, 'AvXD'}
};

VIDEO_OUTPUT_FORMATS SoftwareFormats[] = { // Software
	{&MEDIASUBTYPE_NV12, 3, 12, '21VN'},
	{&MEDIASUBTYPE_YV12, 3, 12, '21VY'},
	{&MEDIASUBTYPE_YUY2, 1, 16, '2YUY'},
	{&MEDIASUBTYPE_I420, 3, 12, '024I'},
	{&MEDIASUBTYPE_IYUV, 3, 12, 'VUYI'}
};

bool CMPCVideoDecFilter::IsDXVASupported()
{
	if (m_nCodecNb != -1) {
		// Does the codec suppport DXVA ?
		if (ffCodecs[m_nCodecNb].DXVAModes != NULL) {
			// Enabled by user ?
			if (m_bUseDXVA) {
				// is the file compatible ?
				if (m_bDXVACompatible) {
					return true;
				}
			}
		}
	}
	return false;
}

void CMPCVideoDecFilter::BuildDXVAOutputFormat()
{
	int			nPos = 0;

	SAFE_DELETE_ARRAY (m_pVideoOutputFormat);

	m_nVideoOutputCount = (IsDXVASupported() ? ffCodecs[m_nCodecNb].DXVAModeCount() + countof (DXVAFormats) : 0) +
						  (m_bUseFFmpeg   ? countof(SoftwareFormats) : 0);

	m_pVideoOutputFormat	= DNew VIDEO_OUTPUT_FORMATS[m_nVideoOutputCount];

	if (IsDXVASupported()) {
		// Dynamic DXVA media types for DXVA1
		for (nPos=0; nPos<ffCodecs[m_nCodecNb].DXVAModeCount(); nPos++) {
			m_pVideoOutputFormat[nPos].subtype			= ffCodecs[m_nCodecNb].DXVAModes->Decoder[nPos];
			m_pVideoOutputFormat[nPos].biCompression	= 'avxd';
			m_pVideoOutputFormat[nPos].biBitCount		= 12;
			m_pVideoOutputFormat[nPos].biPlanes			= 1;
		}

		// Static list for DXVA2
		memcpy (&m_pVideoOutputFormat[nPos], DXVAFormats, sizeof(DXVAFormats));
		nPos += countof (DXVAFormats);
	}

	// Software rendering
	if (m_bUseFFmpeg) {
		memcpy (&m_pVideoOutputFormat[nPos], SoftwareFormats, sizeof(SoftwareFormats));
	}
}

int CMPCVideoDecFilter::GetPicEntryNumber()
{
	if (IsDXVASupported()) {
		return ffCodecs[m_nCodecNb].DXVAModes->PicEntryNumber;
	} else {
		return 0;
	}
}

void CMPCVideoDecFilter::GetOutputFormats (int& nNumber, VIDEO_OUTPUT_FORMATS** ppFormats)
{
	nNumber		= m_nVideoOutputCount;
	*ppFormats	= m_pVideoOutputFormat;
}

void CMPCVideoDecFilter::AllocExtradata(AVCodecContext* pAVCtx, const CMediaType* pmt)
{
	const BYTE*		data = NULL;
	unsigned int	size = 0;

	if (pmt->formattype==FORMAT_VideoInfo) {
		size = pmt->cbFormat-sizeof(VIDEOINFOHEADER);
		data = size?pmt->pbFormat+sizeof(VIDEOINFOHEADER):NULL;
	} else if (pmt->formattype==FORMAT_VideoInfo2) {
		size = pmt->cbFormat-sizeof(VIDEOINFOHEADER2);
		data = size?pmt->pbFormat+sizeof(VIDEOINFOHEADER2):NULL;
	} else if (pmt->formattype==FORMAT_MPEGVideo) {
		MPEG1VIDEOINFO*		mpeg1info = (MPEG1VIDEOINFO*)pmt->pbFormat;
		if (mpeg1info->cbSequenceHeader) {
			size = mpeg1info->cbSequenceHeader;
			data = mpeg1info->bSequenceHeader;
		}
	} else if (pmt->formattype==FORMAT_MPEG2Video) {
		MPEG2VIDEOINFO*		mpeg2info = (MPEG2VIDEOINFO*)pmt->pbFormat;
		if (mpeg2info->cbSequenceHeader) {
			size = mpeg2info->cbSequenceHeader;
			data = (const uint8_t*)mpeg2info->dwSequenceHeader;
		}
	} else if (pmt->formattype==FORMAT_VorbisFormat2) {
		const VORBISFORMAT2 *vf2=(const VORBISFORMAT2*)pmt->pbFormat;
		UNUSED_ALWAYS(vf2);
		size=pmt->cbFormat-sizeof(VORBISFORMAT2);
		data=size?pmt->pbFormat+sizeof(VORBISFORMAT2):NULL;
	}

	if (size) {
		pAVCtx->extradata_size	= size;
		pAVCtx->extradata		= (const unsigned char*)calloc(1,size+FF_INPUT_BUFFER_PADDING_SIZE);
		memcpy((void*)pAVCtx->extradata, data, size);
	}
}

HRESULT CMPCVideoDecFilter::CompleteConnect(PIN_DIRECTION direction, IPin* pReceivePin)
{
	LOG(_T("CMPCVideoDecFilter::CompleteConnect"));

	if (direction==PINDIR_INPUT && m_pOutput->IsConnected()) {
		ReconnectOutput (m_nWidth, m_nHeight);
	} else if (direction==PINDIR_OUTPUT) {
		if (IsDXVASupported()) {
			if (m_nDXVAMode == MODE_DXVA1) {
				m_pDXVADecoder->ConfigureDXVA1();
			} else if (SUCCEEDED (ConfigureDXVA2 (pReceivePin)) && SUCCEEDED (SetEVRForDXVA2 (pReceivePin)) ) {
				m_nDXVAMode  = MODE_DXVA2;
			}
		}
		if (m_nDXVAMode == MODE_SOFTWARE && (!FFSoftwareCheckCompatibility(m_pAVCtx) || (m_nCodecId == CODEC_ID_MPEG2VIDEO))) {
			return VFW_E_INVALIDMEDIATYPE;
		}

		if (m_nDXVAMode == MODE_SOFTWARE && m_nCodecId == CODEC_ID_H264 && m_pAVCtx->h264_using_dxva) {
			m_bUseDXVA = false;
			avcodec_close (m_pAVCtx);
			int nThreadNumber = m_nThreadNumber ? m_nThreadNumber : m_pCpuId->GetProcessorNumber();
			if ((nThreadNumber > 1) && IsMultiThreadSupported (m_nCodecId)) {
				FFSetThreadNumber(m_pAVCtx, m_nCodecId, nThreadNumber);
			}
			m_pAVCtx->h264_using_dxva = 0;
			if (avcodec_open2(m_pAVCtx, m_pAVCodec, NULL)<0) {
				return VFW_E_INVALIDMEDIATYPE;
			}
		}

		CLSID	ClsidSourceFilter = GetCLSID(m_pInput->GetConnected());
		if((ClsidSourceFilter == __uuidof(CMpegSourceFilter)) || (ClsidSourceFilter == __uuidof(CMpegSplitterFilter))) {
			m_bReorderBFrame = false;
		}
	}

	// Cannot use YUY2 if horizontal or vertical resolution is not even
	if (((m_pOutput->CurrentMediaType().subtype == MEDIASUBTYPE_YUY2) && (m_pAVCtx->width&1 || m_pAVCtx->height&1))) {
		return VFW_E_INVALIDMEDIATYPE;
	}

	return __super::CompleteConnect (direction, pReceivePin);
}

HRESULT CMPCVideoDecFilter::DecideBufferSize(IMemAllocator* pAllocator, ALLOCATOR_PROPERTIES* pProperties)
{
	if (UseDXVA2()) {
		HRESULT					hr;
		ALLOCATOR_PROPERTIES	Actual;

		if(m_pInput->IsConnected() == FALSE) {
			return E_UNEXPECTED;
		}

		pProperties->cBuffers = GetPicEntryNumber();

		if(FAILED(hr = pAllocator->SetProperties(pProperties, &Actual))) {
			return hr;
		}

		return pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer
			   ? E_FAIL
			   : NOERROR;
	} else {
		return __super::DecideBufferSize (pAllocator, pProperties);
	}
}

HRESULT CMPCVideoDecFilter::NewSegment(REFERENCE_TIME rtStart, REFERENCE_TIME rtStop, double dRate)
{
	CAutoLock cAutoLock(&m_csReceive);
	m_nPosB = 1;
	memset (&m_BFrames, 0, sizeof(m_BFrames));
	m_rtLastStart		= 0;
	m_nCountEstimated	= 0;
	m_dRate				= dRate;

	ResetBuffer();

	m_h264RandomAccess.flush(m_pAVCtx->thread_count);

	m_bWaitingForKeyFrame = TRUE;

	if (m_pAVCtx) {
		avcodec_flush_buffers (m_pAVCtx);
	}

	if (m_pDXVADecoder) {
		m_pDXVADecoder->Flush();
	}

	return __super::NewSegment (rtStart, rtStop, dRate);
}

HRESULT CMPCVideoDecFilter::BreakConnect(PIN_DIRECTION dir)
{
	if (dir == PINDIR_INPUT) {
		Cleanup();
	}

	return __super::BreakConnect (dir);
}

void CMPCVideoDecFilter::SetTypeSpecificFlags(IMediaSample* pMS)
{
	if(CComQIPtr<IMediaSample2> pMS2 = pMS) {
		AM_SAMPLE2_PROPERTIES props;
		if(SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
			props.dwTypeSpecificFlags &= ~0x7f;

			if(!m_pFrame->interlaced_frame) {
				props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_WEAVE;
			} else {
				if(m_pFrame->top_field_first) {
					props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_FIELD1FIRST;
				}
			}

			switch (m_pFrame->pict_type) {
				case FF_I_TYPE :
				case FF_SI_TYPE :
					props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_I_SAMPLE;
					break;
				case FF_P_TYPE :
				case FF_SP_TYPE :
					props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_P_SAMPLE;
					break;
				default :
					props.dwTypeSpecificFlags |= AM_VIDEO_FLAG_B_SAMPLE;
					break;
			}

			pMS2->SetProperties(sizeof(props), (BYTE*)&props);
		}
	}
}

#if HAS_FFMPEG_VIDEO_DECODERS
int CMPCVideoDecFilter::GetCspFromMediaType(GUID& subtype)
{
	if (subtype == MEDIASUBTYPE_I420 || subtype == MEDIASUBTYPE_IYUV || subtype == MEDIASUBTYPE_YV12) {
		return FF_CSP_420P|FF_CSP_FLAGS_YUV_ADJ;
	} else if (subtype == MEDIASUBTYPE_NV12) {
		return FF_CSP_NV12;
	} else if (subtype == MEDIASUBTYPE_YUY2) {
		return FF_CSP_YUY2;
	}

	ASSERT (FALSE);
	return FF_CSP_NULL;
}

void swsInitParams(SwsParams *params,int resizeMethod)
{
	memset(params, 0, sizeof(*params));
	params->methodLuma.method = params->methodChroma.method = resizeMethod;
	params->methodLuma.param[0] = params->methodChroma.param[0] = SWS_PARAM_DEFAULT;
	params->methodLuma.param[1] = params->methodChroma.param[1] = SWS_PARAM_DEFAULT;
}
void swsInitParams(SwsParams *params,int resizeMethod,int flags)
{
	swsInitParams(params, resizeMethod);
	params->methodLuma.method |= flags;
	params->methodChroma.method |= flags;
}

void CMPCVideoDecFilter::InitSwscale()
{
	if (m_pSwsContext == NULL) {
		BITMAPINFOHEADER bihOut;
		ExtractBIH(&m_pOutput->CurrentMediaType(), &bihOut);

		int sws_Flags = SWS_BILINEAR | SWS_FULL_CHR_H_INP | SWS_FULL_CHR_H_INT;

		SwsParams params;
		swsInitParams(&params, SWS_BILINEAR, sws_Flags);

		m_nOutCsp	  = GetCspFromMediaType(m_pOutput->CurrentMediaType().subtype);

		m_pSwsContext = sws_getCachedContext(
										NULL,
										m_pAVCtx->width,
										m_pAVCtx->height,
										m_pAVCtx->pix_fmt,
										m_pAVCtx->width,
										m_pAVCtx->height,
										csp_ffdshow2lavc(m_nOutCsp),
										sws_Flags|SWS_PRINT_INFO,
										NULL,
										NULL,
										NULL,
										&params,
										(m_nThreadNumber ? m_nThreadNumber : m_pCpuId->GetProcessorNumber()));

		m_nSwOutBpp		= bihOut.biBitCount;
		m_pOutSize.cx	= bihOut.biWidth;
		m_pOutSize.cy	= abs(bihOut.biHeight);

		int *inv_tbl = NULL, *tbl = NULL;
		int srcRange, dstRange, brightness, contrast, saturation;
		int ret = sws_getColorspaceDetails(m_pSwsContext, &inv_tbl, &srcRange, &tbl, &dstRange, &brightness, &contrast, &saturation);
		if (ret >= 0) {
			sws_setColorspaceDetails(m_pSwsContext, sws_getCoefficients((PictWidthRounded() > 768) ? SWS_CS_ITU709 : SWS_CS_ITU601), srcRange, tbl, dstRange, brightness, contrast, saturation);
		}
	}
}

template<class T> inline T odd2even(T x)
{
	return x&1 ?
		   x + 1 :
		   x;
}
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

HRESULT CMPCVideoDecFilter::SoftwareDecode(IMediaSample* pIn, BYTE* pDataIn, int nSize, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	HRESULT			hr = S_OK;
	int				got_picture;
	int				used_bytes;
	BOOL			bFlush = (pDataIn == NULL);

	AVPacket		avpkt;
	av_init_packet(&avpkt);

	if (!bFlush && m_pAVCtx->codec_id == CODEC_ID_H264) {
		if(!m_h264RandomAccess.searchRecoveryPoint(m_pAVCtx, pDataIn, nSize)) {
			return S_OK;
		}
	}

	while (nSize > 0 || bFlush) {
		if (!bFlush) {
			if (nSize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
				m_nFFBufferSize	= nSize+FF_INPUT_BUFFER_PADDING_SIZE;
				m_pFFBuffer		= (BYTE*)realloc(m_pFFBuffer, m_nFFBufferSize);
			}

			// Required number of additionally allocated bytes at the end of the input bitstream for decoding.
			// This is mainly needed because some optimized bitstream readers read
			// 32 or 64 bit at once and could read over the end.
			// Note: If the first 23 bits of the additional bytes are not 0, then damaged
			// MPEG bitstreams could cause overread and segfault.
			memcpy(m_pFFBuffer, pDataIn, nSize);
			memset(m_pFFBuffer+nSize,0,FF_INPUT_BUFFER_PADDING_SIZE);

			avpkt.data = m_pFFBuffer;
			avpkt.size = nSize;
			avpkt.pts  = rtStart;
			avpkt.dts  = rtStop;
			avpkt.flags = AV_PKT_FLAG_KEY;
		} else {
			avpkt.data = NULL;
			avpkt.size = 0;
		}
		used_bytes = avcodec_decode_video2 (m_pAVCtx, m_pFrame, &got_picture, &avpkt);

		if (used_bytes < 0) {
			return S_OK;
		}

		if ((m_pAVCtx->active_thread_type & FF_THREAD_FRAME || (!got_picture && used_bytes == 0)) || bFlush) {
			nSize = 0;
		} else {
			nSize	-= used_bytes;
			pDataIn	+= used_bytes;
		}

		if (m_pAVCtx->codec_id == CODEC_ID_H264 && got_picture) {
			m_h264RandomAccess.judgeFrameUsability(m_pFrame, &got_picture);
    } else if (m_nCodecId == CODEC_ID_VC1) {
      if (m_bWaitingForKeyFrame && got_picture) {
        if (m_pFrame->key_frame) {
          m_bWaitingForKeyFrame = FALSE;
        } else {
          got_picture = 0;
        }
      }
		}

		if (!got_picture || !m_pFrame->data[0]) {
			bFlush = FALSE;
			continue;
		}

		if(pIn->IsPreroll() == S_OK || rtStart < 0) {
			return S_OK;
		}

		CComPtr<IMediaSample>	pOut;
		BYTE*					pDataOut = NULL;

		UpdateAspectRatio();
		if(FAILED(hr = GetDeliveryBuffer(m_pAVCtx->width, m_pAVCtx->height, &pOut)) || FAILED(hr = pOut->GetPointer(&pDataOut))) {
			return hr;
		}

		rtStart = m_pFrame->reordered_opaque;
		rtStop  = m_pFrame->reordered_opaque + m_rtAvrTimePerFrame;
		ReorderBFrames(rtStart, rtStop);

		pOut->SetTime(&rtStart, &rtStop);
		pOut->SetMediaTime(NULL, NULL);

#if HAS_FFMPEG_VIDEO_DECODERS
		if (m_pSwsContext == NULL) {
			InitSwscale();
		}
		if (m_pSwsContext != NULL) {
			uint8_t*	dst[4];
			int			srcStride[4];
			int			dstStride[4];

			const TcspInfo *outcspInfo=csp_getInfo(m_nOutCsp);

			if (m_nOutCsp == FF_CSP_YUY2) {
				dst[0] = pDataOut;
				dst[1] = dst[2] = dst[3] = NULL;
				srcStride[0] = m_pFrame->linesize[0];
				srcStride[1] = m_pFrame->linesize[1];
				srcStride[2] = m_pFrame->linesize[2];
				srcStride[3] = m_pFrame->linesize[3];
				dstStride[0] = (m_nSwOutBpp>>3) * (m_pOutSize.cx);
				dstStride[1] = dstStride[2] = dstStride[3] = 0;
			} else {
				for (int i=0; i<4; i++) {
					srcStride[i]=(stride_t)m_pFrame->linesize[i];
					dstStride[i]=m_pOutSize.cx>>outcspInfo->shiftX[i];
					if (i==0) {
						dst[i]=pDataOut;
					} else {
						dst[i]=dst[i-1]+dstStride[i-1]*(m_pOutSize.cy>>outcspInfo->shiftY[i-1]);
					}
				}
				int nTempCsp = m_nOutCsp;
				if(outcspInfo->id==FF_CSP_420P) {
					csp_yuv_adj_to_plane(nTempCsp,outcspInfo,odd2even(m_pOutSize.cy),(unsigned char**)dst,(stride_t*)dstStride);
				} else {
					csp_yuv_adj_to_plane(nTempCsp,outcspInfo,m_pAVCtx->height,(unsigned char**)dst,(stride_t*)dstStride);
				}
			}

			// We crash inside this function
			// In swscale.c: Function 'simpleCopy'
			// Line: 1961 - Buffer Overrun
			// This might be ffmpeg fault or more likely mpchc is not reinitializing ffmpeg correctly during display change (moving mpchc window from display A to display B)
			sws_scale(m_pSwsContext, m_pFrame->data, srcStride, 0, m_pAVCtx->height, dst, dstStride);
		}
#endif /* HAS_FFMPEG_VIDEO_DECODERS */

#if defined(_DEBUG) && 0
		static REFERENCE_TIME	rtLast = 0;
		TRACE ("Deliver : %10I64d - %10I64d   (%10I64d)  {%10I64d}\n", rtStart, rtStop,
			   rtStop - rtStart, rtStart - rtLast);
		rtLast = rtStart;
#endif

		SetTypeSpecificFlags (pOut);
		hr = m_pOutput->Deliver(pOut);
	}

	return hr;
}

bool CMPCVideoDecFilter::FindPicture(int nIndex, int nStartCode)
{
	DWORD		dw			= 0;

	for (int i=0; i<m_nFFBufferPos-nIndex; i++) {
		dw = (dw<<8) + m_pFFBuffer[i+nIndex];
		if (i >= 4) {
			if (m_nFFPicEnd == INT_MIN) {
				if ( (dw & 0xffffff00) == 0x00000100 &&
						(dw & 0x000000FF) == (DWORD)nStartCode ) {
					m_nFFPicEnd = i+nIndex-3;
				}
			} else {
				if ( (dw & 0xffffff00) == 0x00000100 &&
						( (dw & 0x000000FF) == (DWORD)nStartCode ||  (dw & 0x000000FF) == 0xB3 )) {
					m_nFFPicEnd = i+nIndex-3;
					return true;
				}
			}
		}

	}

	return false;
}

void CMPCVideoDecFilter::ResetBuffer()
{
	m_nFFBufferPos		= 0;
	m_nFFPicEnd			= INT_MIN;

	for (int i=0; i<MAX_BUFF_TIME; i++) {
		m_FFBufferTime[i].nBuffPos	= INT_MIN;
		m_FFBufferTime[i].rtStart	= _I64_MIN;
		m_FFBufferTime[i].rtStop	= _I64_MIN;
	}
}

void CMPCVideoDecFilter::PushBufferTime(int nPos, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	for (int i=0; i<MAX_BUFF_TIME; i++) {
		if (m_FFBufferTime[i].nBuffPos == INT_MIN) {
			m_FFBufferTime[i].nBuffPos	= nPos;
			m_FFBufferTime[i].rtStart	= rtStart;
			m_FFBufferTime[i].rtStop	= rtStop;
			break;
		}
	}
}

void CMPCVideoDecFilter::PopBufferTime(int nPos)
{
	int		nDestPos = 0;
	int		i		 = 0;

	// Shift buffer time list
	while (i<MAX_BUFF_TIME && m_FFBufferTime[i].nBuffPos!=INT_MIN) {
		if (m_FFBufferTime[i].nBuffPos >= nPos) {
			m_FFBufferTime[nDestPos].nBuffPos	= m_FFBufferTime[i].nBuffPos - nPos;
			m_FFBufferTime[nDestPos].rtStart	= m_FFBufferTime[i].rtStart;
			m_FFBufferTime[nDestPos].rtStop		= m_FFBufferTime[i].rtStop;
			nDestPos++;
		}
		i++;
	}

	// Free unused slots
	for (i=nDestPos; i<MAX_BUFF_TIME; i++) {
		m_FFBufferTime[i].nBuffPos	= INT_MIN;
		m_FFBufferTime[i].rtStart	= _I64_MIN;
		m_FFBufferTime[i].rtStop	= _I64_MIN;
	}
}

bool CMPCVideoDecFilter::AppendBuffer (BYTE* pDataIn, int nSize, REFERENCE_TIME rtStart, REFERENCE_TIME rtStop)
{
	if (rtStart != _I64_MIN) {
		PushBufferTime (m_nFFBufferPos, rtStart, rtStop);
	}

	if (m_nFFBufferPos+nSize+FF_INPUT_BUFFER_PADDING_SIZE > m_nFFBufferSize) {
		m_nFFBufferSize = m_nFFBufferPos+nSize+FF_INPUT_BUFFER_PADDING_SIZE;
		m_pFFBuffer		= (BYTE*)realloc(m_pFFBuffer, m_nFFBufferSize);
	}

	memcpy(m_pFFBuffer+m_nFFBufferPos, pDataIn, nSize);

	m_nFFBufferPos += nSize;

	return true;
}

void CMPCVideoDecFilter::ShrinkBuffer()
{
	int			nRemaining = m_nFFBufferPos-m_nFFPicEnd;

	ASSERT (m_nFFPicEnd != INT_MIN);

	PopBufferTime (m_nFFPicEnd);
	memcpy (m_pFFBuffer, m_pFFBuffer+m_nFFPicEnd, nRemaining);
	m_nFFBufferPos	= nRemaining;

	m_nFFPicEnd = (m_pFFBuffer[3] == 0x00) ?  0 : INT_MIN;
}

HRESULT CMPCVideoDecFilter::Transform(IMediaSample* pIn)
{
	CAutoLock cAutoLock(&m_csReceive);
	HRESULT			hr;
	BYTE*			pDataIn;
	int				nSize;
	REFERENCE_TIME	rtStart = _I64_MIN;
	REFERENCE_TIME	rtStop  = _I64_MIN;

	if(FAILED(hr = pIn->GetPointer(&pDataIn))) {
		return hr;
	}

	nSize		= pIn->GetActualDataLength();
	hr = pIn->GetTime(&rtStart, &rtStop);

	// FIXE THIS PART TO EVO_SUPPORT (insure m_rtAvrTimePerFrame is not estimated if not needed!!)
	//if (rtStart != _I64_MIN)
	//{
	//	// Estimate rtStart/rtStop if not set by parser (EVO support)
	//	if (m_nCountEstimated > 0)
	//	{
	//		m_rtAvrTimePerFrame = (rtStart - m_rtLastStart) / m_nCountEstimated;

	//		ROUND_FRAMERATE (m_rtAvrTimePerFrame, 417083);	// 23.97 fps
	//		ROUND_FRAMERATE (m_rtAvrTimePerFrame, 333667);	// 29.97 fps
	//		ROUND_FRAMERATE (m_rtAvrTimePerFrame, 400000);	// 25.00 fps
	//	}
	//	m_rtLastStart		= rtStart;
	//	m_nCountEstimated	= 0;
	//}
	//else
	//{
	//	m_nCountEstimated++;
	//	rtStart = rtStop = m_rtLastStart + m_nCountEstimated*m_rtAvrTimePerFrame;
	//}

	if(FAILED(hr)) {
		rtStart = rtStop = _I64_MIN;
	}

	if (rtStop <= rtStart && rtStop != _I64_MIN) {
		rtStop = rtStart + m_rtAvrTimePerFrame / m_dRate;
	}
	if(m_nDXVAMode == MODE_SOFTWARE) {
		UpdateFrameTime(rtStart, rtStop);
	}

	m_pAVCtx->reordered_opaque  = rtStart;
	m_pAVCtx->reordered_opaque2 = rtStop;

	if (m_pAVCtx->has_b_frames) {
		m_BFrames[m_nPosB].rtStart	= rtStart;
		m_BFrames[m_nPosB].rtStop	= rtStop;
		m_nPosB						= 1-m_nPosB;
	}

	//m_rtStart	= rtStart;

	//DumpBuffer (pDataIn, nSize);
	//TRACE ("Receive : %10I64d - %10I64d   (%10I64d)  Size=%d\n", rtStart, rtStop, rtStop - rtStart, nSize);

	//char		strMsg[300];
	//FILE* hFile = fopen ("d:\\receive.txt", "at");
	//sprintf (strMsg, "Receive : %10I64d - %10I64d   Size=%d\n", (rtStart + m_rtAvrTimePerFrame/2) / m_rtAvrTimePerFrame, rtStart, nSize);
	//fwrite (strMsg, strlen(strMsg), 1, hFile);
	//fclose (hFile);

	//char		strMsg[300];
	//FILE* hFile = fopen ("receive.bin", "ab");
	//fwrite (pDataIn, nSize, 1, hFile);
	//fclose (hFile);

	switch (m_nDXVAMode) {
		case MODE_SOFTWARE :
			hr = SoftwareDecode (pIn, pDataIn, nSize, rtStart, rtStop);
			break;
		case MODE_DXVA1 :
		case MODE_DXVA2 :
			CheckPointer (m_pDXVADecoder, E_UNEXPECTED);
			UpdateAspectRatio();

			// Change aspect ratio for DXVA1
			if ((m_nDXVAMode == MODE_DXVA1) &&
					ReconnectOutput(PictWidthRounded(), PictHeightRounded(), true, PictWidth(), PictHeight()) == S_OK) {
				m_pDXVADecoder->ConfigureDXVA1();
			}

			if (m_pAVCtx->codec_id == CODEC_ID_MPEG2VIDEO) {
				AppendBuffer (pDataIn, nSize, rtStart, rtStop);
				hr = S_OK;

				while (FindPicture (max (m_nFFBufferPos-nSize-4, 0), 0x00)) {
					if (m_FFBufferTime[0].nBuffPos != INT_MIN && m_FFBufferTime[0].nBuffPos < m_nFFPicEnd) {
						rtStart = m_FFBufferTime[0].rtStart;
						rtStop  = m_FFBufferTime[0].rtStop;
					} else {
						rtStart = rtStop = _I64_MIN;
					}
					hr = m_pDXVADecoder->DecodeFrame (m_pFFBuffer, m_nFFPicEnd, rtStart, rtStop);
					ShrinkBuffer();
				}
			} else {
				hr = m_pDXVADecoder->DecodeFrame (pDataIn, nSize, rtStart, rtStop);
			}
			break;
		default :
			ASSERT (FALSE);
			hr = E_UNEXPECTED;
	}

	return hr;
}

void CMPCVideoDecFilter::UpdateAspectRatio()
{
	if(((m_nARMode) && (m_pAVCtx)) && ((m_pAVCtx->sample_aspect_ratio.num>0) && (m_pAVCtx->sample_aspect_ratio.den>0))) {
		CSize SAR(m_pAVCtx->sample_aspect_ratio.num, m_pAVCtx->sample_aspect_ratio.den);
		if(m_sar != SAR) {
			m_sar = SAR;
			CSize aspect(m_nWidth * SAR.cx, m_nHeight * SAR.cy);
			int lnko = LNKO(aspect.cx, aspect.cy);
			if(lnko > 1) {
				aspect.cx /= lnko, aspect.cy /= lnko;
			}
			SetAspect(aspect);
		}
	}
}

void CMPCVideoDecFilter::ReorderBFrames(REFERENCE_TIME& rtStart, REFERENCE_TIME& rtStop)
{
	// Re-order B-frames if needed
	if (m_pAVCtx->has_b_frames && m_bReorderBFrame) {
		rtStart	= m_BFrames [m_nPosB].rtStart;
		rtStop	= m_BFrames [m_nPosB].rtStop;
	}
}

void CMPCVideoDecFilter::FillInVideoDescription(DXVA2_VideoDesc *pDesc)
{
	memset (pDesc, 0, sizeof(DXVA2_VideoDesc));
	pDesc->SampleWidth			= PictWidthRounded();
	pDesc->SampleHeight			= PictHeightRounded();
	pDesc->Format				= D3DFMT_A8R8G8B8;
	pDesc->UABProtectionLevel	= 1;
}

BOOL CMPCVideoDecFilter::IsSupportedDecoderMode(const GUID& mode)
{
	if (IsDXVASupported()) {
		for (int i=0; i<MAX_SUPPORTED_MODE; i++) {
			if (*ffCodecs[m_nCodecNb].DXVAModes->Decoder[i] == GUID_NULL) {
				break;
			} else if (*ffCodecs[m_nCodecNb].DXVAModes->Decoder[i] == mode) {
				return true;
			}
		}
	}

	return false;
}

BOOL CMPCVideoDecFilter::IsSupportedDecoderConfig(const D3DFORMAT nD3DFormat, const DXVA2_ConfigPictureDecode& config, bool& bIsPrefered)
{
	bool	bRet = false;

	bRet = (nD3DFormat == MAKEFOURCC('N', 'V', '1', '2') || nD3DFormat == MAKEFOURCC('I', 'M', 'C', '3'));

	bIsPrefered = (config.ConfigBitstreamRaw == ffCodecs[m_nCodecNb].DXVAModes->PreferedConfigBitstream);
	LOG (_T("IsSupportedDecoderConfig  0x%08x  %d"), nD3DFormat, bRet);
	return bRet;
}

HRESULT CMPCVideoDecFilter::FindDXVA2DecoderConfiguration(IDirectXVideoDecoderService *pDecoderService,
		const GUID& guidDecoder,
		DXVA2_ConfigPictureDecode *pSelectedConfig,
		BOOL *pbFoundDXVA2Configuration)
{
	HRESULT hr = S_OK;
	UINT cFormats = 0;
	UINT cConfigurations = 0;
	bool bIsPrefered = false;

	D3DFORMAT                   *pFormats = NULL;			// size = cFormats
	DXVA2_ConfigPictureDecode   *pConfig = NULL;			// size = cConfigurations

	// Find the valid render target formats for this decoder GUID.
	hr = pDecoderService->GetDecoderRenderTargets(guidDecoder, &cFormats, &pFormats);
	LOG (_T("GetDecoderRenderTargets => %d"), cFormats);

	if (SUCCEEDED(hr)) {
		// Look for a format that matches our output format.
		for (UINT iFormat = 0; iFormat < cFormats;  iFormat++) {
			LOG (_T("Try to negociate => 0x%08x"), pFormats[iFormat]);

			// Fill in the video description. Set the width, height, format, and frame rate.
			FillInVideoDescription(&m_VideoDesc); // Private helper function.
			m_VideoDesc.Format = pFormats[iFormat];

			// Get the available configurations.
			hr = pDecoderService->GetDecoderConfigurations(guidDecoder, &m_VideoDesc, NULL, &cConfigurations, &pConfig);

			if (FAILED(hr)) {
				continue;
			}

			// Find a supported configuration.
			for (UINT iConfig = 0; iConfig < cConfigurations; iConfig++) {
				if (IsSupportedDecoderConfig(pFormats[iFormat], pConfig[iConfig], bIsPrefered)) {
					// This configuration is good.
					if (bIsPrefered || !*pbFoundDXVA2Configuration) {
						*pbFoundDXVA2Configuration = TRUE;
						*pSelectedConfig = pConfig[iConfig];
					}

					if (bIsPrefered) {
						break;
					}
				}
			}

			CoTaskMemFree(pConfig);
		} // End of formats loop.
	}

	CoTaskMemFree(pFormats);

	// Note: It is possible to return S_OK without finding a configuration.
	return hr;
}

HRESULT CMPCVideoDecFilter::ConfigureDXVA2(IPin *pPin)
{
	HRESULT hr						 = S_OK;
	UINT	cDecoderGuids			 = 0;
	BOOL	bFoundDXVA2Configuration = FALSE;
	GUID	guidDecoder				 = GUID_NULL;

	DXVA2_ConfigPictureDecode config;
	ZeroMemory(&config, sizeof(config));

	CComPtr<IMFGetService>					pGetService;
	CComPtr<IDirect3DDeviceManager9>		pDeviceManager;
	CComPtr<IDirectXVideoDecoderService>	pDecoderService;
	GUID*									pDecoderGuids = NULL;
	HANDLE									hDevice = INVALID_HANDLE_VALUE;

	// Query the pin for IMFGetService.
	hr = pPin->QueryInterface(__uuidof(IMFGetService), (void**)&pGetService);

	// Get the Direct3D device manager.
	if (SUCCEEDED(hr)) {
		hr = pGetService->GetService(
				 MR_VIDEO_ACCELERATION_SERVICE,
				 __uuidof(IDirect3DDeviceManager9),
				 (void**)&pDeviceManager);
	}

	// Open a new device handle.
	if (SUCCEEDED(hr)) {
		hr = pDeviceManager->OpenDeviceHandle(&hDevice);
	}

	// Get the video decoder service.
	if (SUCCEEDED(hr)) {
		hr = pDeviceManager->GetVideoService(
				 hDevice,
				 __uuidof(IDirectXVideoDecoderService),
				 (void**)&pDecoderService);
	}

	// Get the decoder GUIDs.
	if (SUCCEEDED(hr)) {
		hr = pDecoderService->GetDecoderDeviceGuids(&cDecoderGuids, &pDecoderGuids);
	}

	if (SUCCEEDED(hr)) {
		// Look for the decoder GUIDs we want.
		for (UINT iGuid = 0; iGuid < cDecoderGuids; iGuid++) {
			// Do we support this mode?
			if (!IsSupportedDecoderMode(pDecoderGuids[iGuid])) {
				continue;
			}

			// Find a configuration that we support.
			hr = FindDXVA2DecoderConfiguration(pDecoderService, pDecoderGuids[iGuid], &config, &bFoundDXVA2Configuration);

			if (FAILED(hr)) {
				break;
			}

			// Patch for the Sandy Bridge (prevent crash on Mode_E, fixme later)
			// known device IDs for SB integrated graphics are: 258, 274, 278, 290, 294
			if (m_nPCIVendor == PCIV_Intel && m_nPCIDevice>=258 && m_nPCIDevice<=294 && pDecoderGuids[iGuid] == DXVA2_ModeH264_E) {
				bFoundDXVA2Configuration = false;
			}

			if (bFoundDXVA2Configuration) {
				// Found a good configuration. Save the GUID.
				guidDecoder = pDecoderGuids[iGuid];
				break;
			}
		}
	}

	if (pDecoderGuids) {
		CoTaskMemFree(pDecoderGuids);
	}
	if (!bFoundDXVA2Configuration) {
		hr = E_FAIL; // Unable to find a configuration.
	}

	if (SUCCEEDED(hr)) {
		// Store the things we will need later.
		m_pDeviceManager	= pDeviceManager;
		m_pDecoderService	= pDecoderService;

		m_DXVA2Config		= config;
		m_DXVADecoderGUID	= guidDecoder;
		m_hDevice			= hDevice;
	}

	if (FAILED(hr)) {
		if (hDevice != INVALID_HANDLE_VALUE) {
			pDeviceManager->CloseDeviceHandle(hDevice);
		}
	}

	return hr;
}

HRESULT CMPCVideoDecFilter::SetEVRForDXVA2(IPin *pPin)
{
	HRESULT hr = S_OK;

	CComPtr<IMFGetService>						pGetService;
	CComPtr<IDirectXVideoMemoryConfiguration>	pVideoConfig;
	CComPtr<IMFVideoDisplayControl>				pVdc;

	// Query the pin for IMFGetService.
	hr = pPin->QueryInterface(__uuidof(IMFGetService), (void**)&pGetService);

	// Get the IDirectXVideoMemoryConfiguration interface.
	if (SUCCEEDED(hr)) {
		hr = pGetService->GetService(
				 MR_VIDEO_ACCELERATION_SERVICE,
				 __uuidof(IDirectXVideoMemoryConfiguration),
				 (void**)&pVideoConfig);

		if (SUCCEEDED (pGetService->GetService(MR_VIDEO_RENDER_SERVICE, __uuidof(IMFVideoDisplayControl), (void**)&pVdc))) {
			HWND	hWnd;
			if (SUCCEEDED (pVdc->GetVideoWindow(&hWnd))) {
				DetectVideoCard(hWnd);
			}
		}
	}

	// Notify the EVR.
	if (SUCCEEDED(hr)) {
		DXVA2_SurfaceType surfaceType;

		for (DWORD iTypeIndex = 0; ; iTypeIndex++) {
			hr = pVideoConfig->GetAvailableSurfaceTypeByIndex(iTypeIndex, &surfaceType);

			if (FAILED(hr)) {
				break;
			}

			if (surfaceType == DXVA2_SurfaceType_DecoderRenderTarget) {
				hr = pVideoConfig->SetSurfaceType(DXVA2_SurfaceType_DecoderRenderTarget);
				break;
			}
		}
	}

	return hr;
}

HRESULT CMPCVideoDecFilter::CreateDXVA2Decoder(UINT nNumRenderTargets, IDirect3DSurface9** pDecoderRenderTargets)
{
	HRESULT							hr;
	CComPtr<IDirectXVideoDecoder>	pDirectXVideoDec;

	m_pDecoderRenderTarget	= NULL;

	if (m_pDXVADecoder) {
		m_pDXVADecoder->SetDirectXVideoDec (NULL);
	}

	hr = m_pDecoderService->CreateVideoDecoder (m_DXVADecoderGUID, &m_VideoDesc, &m_DXVA2Config,
			pDecoderRenderTargets, nNumRenderTargets, &pDirectXVideoDec);

	if (SUCCEEDED (hr)) {
		if (!m_pDXVADecoder) {
			m_pDXVADecoder	= CDXVADecoder::CreateDecoder (this, pDirectXVideoDec, &m_DXVADecoderGUID, GetPicEntryNumber(), &m_DXVA2Config);
			if (m_pDXVADecoder) {
				m_pDXVADecoder->SetExtraData ((BYTE*)m_pAVCtx->extradata, m_pAVCtx->extradata_size);
			}
		}

		m_pDXVADecoder->SetDirectXVideoDec (pDirectXVideoDec);
	}

	return hr;
}

HRESULT CMPCVideoDecFilter::FindDXVA1DecoderConfiguration(IAMVideoAccelerator* pAMVideoAccelerator, const GUID* guidDecoder, DDPIXELFORMAT* pPixelFormat)
{
	HRESULT			hr				= E_FAIL;
	DWORD			dwFormats		= 0;
	DDPIXELFORMAT*	pPixelFormats	= NULL;


	pAMVideoAccelerator->GetUncompFormatsSupported (guidDecoder, &dwFormats, NULL);
	if (dwFormats > 0) {
		// Find the valid render target formats for this decoder GUID.
		pPixelFormats = DNew DDPIXELFORMAT[dwFormats];
		hr = pAMVideoAccelerator->GetUncompFormatsSupported (guidDecoder, &dwFormats, pPixelFormats);
		if (SUCCEEDED(hr)) {
			// Look for a format that matches our output format.
			for (DWORD iFormat = 0; iFormat < dwFormats; iFormat++) {
				if (pPixelFormats[iFormat].dwFourCC == MAKEFOURCC ('N', 'V', '1', '2')) {
					memcpy (pPixelFormat, &pPixelFormats[iFormat], sizeof(DDPIXELFORMAT));
					SAFE_DELETE_ARRAY(pPixelFormats)
					return S_OK;
				}
			}

			SAFE_DELETE_ARRAY(pPixelFormats);
			hr = E_FAIL;
		}
	}

	return hr;
}

HRESULT CMPCVideoDecFilter::CheckDXVA1Decoder(const GUID *pGuid)
{
	if (m_nCodecNb != -1) {
		for (int i=0; i<MAX_SUPPORTED_MODE; i++)
			if (*ffCodecs[m_nCodecNb].DXVAModes->Decoder[i] == *pGuid) {
				return S_OK;
			}
	}

	return E_INVALIDARG;
}

void CMPCVideoDecFilter::SetDXVA1Params(const GUID* pGuid, DDPIXELFORMAT* pPixelFormat)
{
	m_DXVADecoderGUID		= *pGuid;
	memcpy (&m_PixelFormat, pPixelFormat, sizeof (DDPIXELFORMAT));
}

WORD CMPCVideoDecFilter::GetDXVA1RestrictedMode()
{
	if (m_nCodecNb != -1) {
		for (int i=0; i<MAX_SUPPORTED_MODE; i++)
			if (*ffCodecs[m_nCodecNb].DXVAModes->Decoder[i] == m_DXVADecoderGUID) {
				return ffCodecs[m_nCodecNb].DXVAModes->RestrictedMode [i];
			}
	}

	return DXVA_RESTRICTED_MODE_UNRESTRICTED;
}

HRESULT CMPCVideoDecFilter::CreateDXVA1Decoder(IAMVideoAccelerator*  pAMVideoAccelerator, const GUID* pDecoderGuid, DWORD dwSurfaceCount)
{
	if (m_pDXVADecoder && m_DXVADecoderGUID	== *pDecoderGuid) {
		return S_OK;
	}
	SAFE_DELETE (m_pDXVADecoder);

	if (!m_bUseDXVA) {
		return E_FAIL;
	}

	m_nDXVAMode			= MODE_DXVA1;
	m_DXVADecoderGUID	= *pDecoderGuid;
	m_pDXVADecoder		= CDXVADecoder::CreateDecoder (this, pAMVideoAccelerator, &m_DXVADecoderGUID, dwSurfaceCount);
	if (m_pDXVADecoder) {
		m_pDXVADecoder->SetExtraData ((BYTE*)m_pAVCtx->extradata, m_pAVCtx->extradata_size);
	}

	return S_OK;
}

// ISpecifyPropertyPages2

STDMETHODIMP CMPCVideoDecFilter::GetPages(CAUUID* pPages)
{
	CheckPointer(pPages, E_POINTER);

#ifdef REGISTER_FILTER
	pPages->cElems		= 2;
#else
	pPages->cElems		= 1;
#endif

	pPages->pElems		= (GUID*)CoTaskMemAlloc(sizeof(GUID) * pPages->cElems);
	pPages->pElems[0]	= __uuidof(CMPCVideoDecSettingsWnd);
	if (pPages->cElems>1) {
		pPages->pElems[1]	= __uuidof(CMPCVideoDecCodecWnd);
	}

	return S_OK;
}

STDMETHODIMP CMPCVideoDecFilter::CreatePage(const GUID& guid, IPropertyPage** ppPage)
{
	CheckPointer(ppPage, E_POINTER);

	if(*ppPage != NULL) {
		return E_INVALIDARG;
	}

	HRESULT hr;

	if(guid == __uuidof(CMPCVideoDecSettingsWnd)) {
		(*ppPage = DNew CInternalPropertyPageTempl<CMPCVideoDecSettingsWnd>(NULL, &hr))->AddRef();
	} else if(guid == __uuidof(CMPCVideoDecCodecWnd)) {
		(*ppPage = DNew CInternalPropertyPageTempl<CMPCVideoDecCodecWnd>(NULL, &hr))->AddRef();
	}

	return *ppPage ? S_OK : E_FAIL;
}

// IFFmpegDecFilter
STDMETHODIMP CMPCVideoDecFilter::Apply()
{
#ifdef REGISTER_FILTER
	CRegKey key;
	if(ERROR_SUCCESS == key.Create(HKEY_CURRENT_USER, _T("Software\\Gabest\\Filters\\MPC Video Decoder"))) {
		key.SetDWORDValue(_T("ThreadNumber"), m_nThreadNumber);
		key.SetDWORDValue(_T("DiscardMode"), m_nDiscardMode);
		key.SetDWORDValue(_T("ErrorRecognition"), m_nErrorRecognition);
		key.SetDWORDValue(_T("IDCTAlgo"), m_nIDCTAlgo);
		key.SetDWORDValue(_T("ActiveCodecs"), m_nActiveCodecs);
		key.SetDWORDValue(_T("ARMode"), m_nARMode);
		key.SetDWORDValue(_T("DXVACheckCompatibility"), m_nDXVACheckCompatibility);
		key.SetDWORDValue(_T("DisableDXVA_SD"), m_nDXVA_SD);
	}
#else
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("ThreadNumber"), m_nThreadNumber);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("DiscardMode"), m_nDiscardMode);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("ErrorRecognition"), m_nErrorRecognition);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("IDCTAlgo"), m_nIDCTAlgo);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("ARMode"), m_nARMode);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("DXVACheckCompatibility"), m_nDXVACheckCompatibility);
	AfxGetApp()->WriteProfileInt(_T("Filters\\MPC Video Decoder"), _T("DisableDXVA_SD"), m_nDXVA_SD);
#endif

	return S_OK;
}

STDMETHODIMP CMPCVideoDecFilter::SetThreadNumber(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nThreadNumber = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetThreadNumber()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nThreadNumber;
}

STDMETHODIMP CMPCVideoDecFilter::SetDiscardMode(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nDiscardMode = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetDiscardMode()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nDiscardMode;
}

STDMETHODIMP CMPCVideoDecFilter::SetErrorRecognition(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nErrorRecognition = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetErrorRecognition()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nErrorRecognition;
}

STDMETHODIMP CMPCVideoDecFilter::SetIDCTAlgo(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nIDCTAlgo = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetIDCTAlgo()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nIDCTAlgo;
}

STDMETHODIMP_(GUID*) CMPCVideoDecFilter::GetDXVADecoderGuid()
{
	if (m_pGraph == NULL) {
		return NULL;
	} else {
		return &m_DXVADecoderGUID;
	}
}

STDMETHODIMP CMPCVideoDecFilter::SetActiveCodecs(MPC_VIDEO_CODEC nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nActiveCodecs = (int)nValue;
	return S_OK;
}

STDMETHODIMP_(MPC_VIDEO_CODEC) CMPCVideoDecFilter::GetActiveCodecs()
{
	CAutoLock cAutoLock(&m_csProps);
	return (MPC_VIDEO_CODEC)m_nActiveCodecs;
}

STDMETHODIMP_(LPCTSTR) CMPCVideoDecFilter::GetVideoCardDescription()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_strDeviceDescription;
}

STDMETHODIMP CMPCVideoDecFilter::SetARMode(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nARMode = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetARMode()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nARMode;
}

STDMETHODIMP CMPCVideoDecFilter::SetDXVACheckCompatibility(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nDXVACheckCompatibility = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetDXVACheckCompatibility()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nDXVACheckCompatibility;
}

STDMETHODIMP CMPCVideoDecFilter::SetDXVA_SD(int nValue)
{
	CAutoLock cAutoLock(&m_csProps);
	m_nDXVA_SD = nValue;
	return S_OK;
}

STDMETHODIMP_(int) CMPCVideoDecFilter::GetDXVA_SD()
{
	CAutoLock cAutoLock(&m_csProps);
	return m_nDXVA_SD;
}

/*
 * copyright (c) 2001 Fabrice Bellard
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_AVCODEC_H
#define AVCODEC_AVCODEC_H

#ifdef __cplusplus
    #define FF_EXPORT extern "C" 
#else
    #define FF_EXPORT 
#endif

/**
 * @file
 * external API header
 */


#include <errno.h>
#include "libavutil/samplefmt.h"
#include "libavutil/avutil.h"
#include "libavutil/cpu.h"
#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

#include "libavcodec/version.h"
/**
 * @defgroup libavc Encoding/Decoding Library
 * @{
 *
 * @defgroup lavc_decoding Decoding
 * @{
 * @}
 *
 * @defgroup lavc_encoding Encoding
 * @{
 * @}
 *
 * @defgroup lavc_codec Codecs
 * @{
 * @defgroup lavc_codec_native Native Codecs
 * @{
 * @}
 * @defgroup lavc_codec_wrappers External library wrappers
 * @{
 * @}
 * @defgroup lavc_codec_hwaccel Hardware Accelerators bridge
 * @{
 * @}
 * @}
 * @defgroup lavc_internal Internal
 * @{
 * @}
 * @}
 *
 */


/**
 * Identify the syntax and semantics of the bitstream.
 * The principle is roughly:
 * Two decoders with the same ID can decode the same streams.
 * Two encoders with the same ID can encode compatible streams.
 * There may be slight deviations from the principle due to implementation
 * details.
 *
 * If you add a codec ID to this list, add it so that
 * 1. no value of a existing codec ID changes (that would break ABI),
 * 2. it is as close as possible to similar codecs.
 */
enum CodecID {
    CODEC_ID_NONE,

    /* video codecs */
    CODEC_ID_MPEG1VIDEO,
    CODEC_ID_MPEG2VIDEO, ///< preferred ID for MPEG-1/2 video decoding
    CODEC_ID_MPEG2VIDEO_XVMC,
    CODEC_ID_H261,
    CODEC_ID_H263,
    CODEC_ID_RV10,
    CODEC_ID_RV20,
    CODEC_ID_MJPEG,
    CODEC_ID_MJPEGB,
    CODEC_ID_LJPEG,
    CODEC_ID_SP5X,
    CODEC_ID_JPEGLS,
    CODEC_ID_MPEG4,
    CODEC_ID_RAWVIDEO,
    CODEC_ID_MSMPEG4V1,
    CODEC_ID_MSMPEG4V2,
    CODEC_ID_MSMPEG4V3,
    CODEC_ID_WMV1,
    CODEC_ID_WMV2,
    CODEC_ID_H263P,
    CODEC_ID_H263I,
    CODEC_ID_FLV1,
    CODEC_ID_SVQ1,
    CODEC_ID_SVQ3,
    CODEC_ID_DVVIDEO,
    CODEC_ID_HUFFYUV,
    CODEC_ID_CYUV,
    CODEC_ID_H264,
    CODEC_ID_INDEO3,
    CODEC_ID_VP3,
    CODEC_ID_THEORA,
    CODEC_ID_ASV1,
    CODEC_ID_ASV2,
    CODEC_ID_FFV1,
    CODEC_ID_4XM,
    CODEC_ID_VCR1,
    CODEC_ID_CLJR,
    CODEC_ID_MDEC,
    CODEC_ID_ROQ,
    CODEC_ID_INTERPLAY_VIDEO,
    CODEC_ID_XAN_WC3,
    CODEC_ID_XAN_WC4,
    CODEC_ID_RPZA,
    CODEC_ID_CINEPAK,
    CODEC_ID_WS_VQA,
    CODEC_ID_MSRLE,
    CODEC_ID_MSVIDEO1,
    CODEC_ID_IDCIN,
    CODEC_ID_8BPS,
    CODEC_ID_SMC,
    CODEC_ID_FLIC,
    CODEC_ID_TRUEMOTION1,
    CODEC_ID_VMDVIDEO,
    CODEC_ID_MSZH,
    CODEC_ID_ZLIB,
    CODEC_ID_QTRLE,
    CODEC_ID_SNOW,
    CODEC_ID_TSCC,
    CODEC_ID_ULTI,
    CODEC_ID_QDRAW,
    CODEC_ID_VIXL,
    CODEC_ID_QPEG,
    CODEC_ID_PNG,
    CODEC_ID_PPM,
    CODEC_ID_PBM,
    CODEC_ID_PGM,
    CODEC_ID_PGMYUV,
    CODEC_ID_PAM,
    CODEC_ID_FFVHUFF,
    CODEC_ID_RV30,
    CODEC_ID_RV40,
    CODEC_ID_VC1,
    CODEC_ID_WMV3,
    CODEC_ID_LOCO,
    CODEC_ID_WNV1,
    CODEC_ID_AASC,
    CODEC_ID_INDEO2,
    CODEC_ID_FRAPS,
    CODEC_ID_TRUEMOTION2,
    CODEC_ID_BMP,
    CODEC_ID_CSCD,
    CODEC_ID_MMVIDEO,
    CODEC_ID_ZMBV,
    CODEC_ID_AVS,
    CODEC_ID_SMACKVIDEO,
    CODEC_ID_NUV,
    CODEC_ID_KMVC,
    CODEC_ID_FLASHSV,
    CODEC_ID_CAVS,
    CODEC_ID_JPEG2000,
    CODEC_ID_VMNC,
    CODEC_ID_VP5,
    CODEC_ID_VP6,
    CODEC_ID_VP6F,
    CODEC_ID_TARGA,
    CODEC_ID_DSICINVIDEO,
    CODEC_ID_TIERTEXSEQVIDEO,
    CODEC_ID_TIFF,
    CODEC_ID_GIF,
#if LIBAVCODEC_VERSION_MAJOR == 53
    CODEC_ID_FFH264,
#endif
    CODEC_ID_DXA,
    CODEC_ID_DNXHD,
    CODEC_ID_THP,
    CODEC_ID_SGI,
    CODEC_ID_C93,
    CODEC_ID_BETHSOFTVID,
    CODEC_ID_PTX,
    CODEC_ID_TXD,
    CODEC_ID_VP6A,
    CODEC_ID_AMV,
    CODEC_ID_VB,
    CODEC_ID_PCX,
    CODEC_ID_SUNRAST,
    CODEC_ID_INDEO4,
    CODEC_ID_INDEO5,
    CODEC_ID_MIMIC,
    CODEC_ID_RL2,
#if LIBAVCODEC_VERSION_MAJOR == 53
    CODEC_ID_8SVX_EXP,
    CODEC_ID_8SVX_FIB,
#endif
    CODEC_ID_ESCAPE124,
    CODEC_ID_DIRAC,
    CODEC_ID_BFI,
    CODEC_ID_CMV,
    CODEC_ID_MOTIONPIXELS,
    CODEC_ID_TGV,
    CODEC_ID_TGQ,
    CODEC_ID_TQI,
    CODEC_ID_AURA,
    CODEC_ID_AURA2,
    CODEC_ID_V210X,
    CODEC_ID_TMV,
    CODEC_ID_V210,
    CODEC_ID_DPX,
    CODEC_ID_MAD,
    CODEC_ID_FRWU,
    CODEC_ID_FLASHSV2,
    CODEC_ID_CDGRAPHICS,
    CODEC_ID_R210,
    CODEC_ID_ANM,
    CODEC_ID_BINKVIDEO,
    CODEC_ID_IFF_ILBM,
    CODEC_ID_IFF_BYTERUN1,
    CODEC_ID_KGV1,
    CODEC_ID_YOP,
    CODEC_ID_VP8,
    CODEC_ID_PICTOR,
    CODEC_ID_ANSI,
    CODEC_ID_A64_MULTI,
    CODEC_ID_A64_MULTI5,
    CODEC_ID_R10K,
    CODEC_ID_MXPEG,
    CODEC_ID_LAGARITH,
    CODEC_ID_PRORES,
    CODEC_ID_JV,
    CODEC_ID_DFA,
    CODEC_ID_WMV3IMAGE,
    CODEC_ID_VC1IMAGE,
#if LIBAVCODEC_VERSION_MAJOR == 53
    CODEC_ID_G723_1,
    CODEC_ID_G729,
#endif
    CODEC_ID_UTVIDEO,
    CODEC_ID_BMV_VIDEO,
    CODEC_ID_VBLE,
    CODEC_ID_DXTORY,
    CODEC_ID_V410,

    /* various PCM "codecs" */
    CODEC_ID_FIRST_AUDIO = 0x10000,     ///< A dummy id pointing at the start of audio codecs
    CODEC_ID_PCM_S16LE = 0x10000,
    CODEC_ID_PCM_S16BE,
    CODEC_ID_PCM_U16LE,
    CODEC_ID_PCM_U16BE,
    CODEC_ID_PCM_S8,
    CODEC_ID_PCM_U8,
    CODEC_ID_PCM_MULAW,
    CODEC_ID_PCM_ALAW,
    CODEC_ID_PCM_S32LE,
    CODEC_ID_PCM_S32BE,
    CODEC_ID_PCM_U32LE,
    CODEC_ID_PCM_U32BE,
    CODEC_ID_PCM_S24LE,
    CODEC_ID_PCM_S24BE,
    CODEC_ID_PCM_U24LE,
    CODEC_ID_PCM_U24BE,
    CODEC_ID_PCM_S24DAUD,
    CODEC_ID_PCM_ZORK,
    CODEC_ID_PCM_S16LE_PLANAR,
    CODEC_ID_PCM_DVD,
    CODEC_ID_PCM_F32BE,
    CODEC_ID_PCM_F32LE,
    CODEC_ID_PCM_F64BE,
    CODEC_ID_PCM_F64LE,
    CODEC_ID_PCM_BLURAY,
    CODEC_ID_PCM_LXF,
    CODEC_ID_S302M,
    CODEC_ID_PCM_S8_PLANAR,

    /* various ADPCM codecs */
    CODEC_ID_ADPCM_IMA_QT = 0x11000,
    CODEC_ID_ADPCM_IMA_WAV,
    CODEC_ID_ADPCM_IMA_DK3,
    CODEC_ID_ADPCM_IMA_DK4,
    CODEC_ID_ADPCM_IMA_WS,
    CODEC_ID_ADPCM_IMA_SMJPEG,
    CODEC_ID_ADPCM_MS,
    CODEC_ID_ADPCM_4XM,
    CODEC_ID_ADPCM_XA,
    CODEC_ID_ADPCM_ADX,
    CODEC_ID_ADPCM_EA,
    CODEC_ID_ADPCM_G726,
    CODEC_ID_ADPCM_CT,
    CODEC_ID_ADPCM_SWF,
    CODEC_ID_ADPCM_YAMAHA,
    CODEC_ID_ADPCM_SBPRO_4,
    CODEC_ID_ADPCM_SBPRO_3,
    CODEC_ID_ADPCM_SBPRO_2,
    CODEC_ID_ADPCM_THP,
    CODEC_ID_ADPCM_IMA_AMV,
    CODEC_ID_ADPCM_EA_R1,
    CODEC_ID_ADPCM_EA_R3,
    CODEC_ID_ADPCM_EA_R2,
    CODEC_ID_ADPCM_IMA_EA_SEAD,
    CODEC_ID_ADPCM_IMA_EA_EACS,
    CODEC_ID_ADPCM_EA_XAS,
    CODEC_ID_ADPCM_EA_MAXIS_XA,
    CODEC_ID_ADPCM_IMA_ISS,
    CODEC_ID_ADPCM_G722,

    /* AMR */
    CODEC_ID_AMR_NB = 0x12000,
    CODEC_ID_AMR_WB,

    /* RealAudio codecs*/
    CODEC_ID_RA_144 = 0x13000,
    CODEC_ID_RA_288,

    /* various DPCM codecs */
    CODEC_ID_ROQ_DPCM = 0x14000,
    CODEC_ID_INTERPLAY_DPCM,
    CODEC_ID_XAN_DPCM,
    CODEC_ID_SOL_DPCM,

    /* audio codecs */
    CODEC_ID_MP2 = 0x15000,
    CODEC_ID_MP3, ///< preferred ID for decoding MPEG audio layer 1, 2 or 3
    CODEC_ID_AAC,
    CODEC_ID_AC3,
    CODEC_ID_DTS,
    CODEC_ID_VORBIS,
    CODEC_ID_DVAUDIO,
    CODEC_ID_WMAV1,
    CODEC_ID_WMAV2,
    CODEC_ID_MACE3,
    CODEC_ID_MACE6,
    CODEC_ID_VMDAUDIO,
#if LIBAVCODEC_VERSION_MAJOR == 53
    CODEC_ID_SONIC,
    CODEC_ID_SONIC_LS,
#endif
    CODEC_ID_FLAC,
    CODEC_ID_MP3ADU,
    CODEC_ID_MP3ON4,
    CODEC_ID_SHORTEN,
    CODEC_ID_ALAC,
    CODEC_ID_WESTWOOD_SND1,
    CODEC_ID_GSM, ///< as in Berlin toast format
    CODEC_ID_QDM2,
    CODEC_ID_COOK,
    CODEC_ID_TRUESPEECH,
    CODEC_ID_TTA,
    CODEC_ID_SMACKAUDIO,
    CODEC_ID_QCELP,
    CODEC_ID_WAVPACK,
    CODEC_ID_DSICINAUDIO,
    CODEC_ID_IMC,
    CODEC_ID_MUSEPACK7,
    CODEC_ID_MLP,
    CODEC_ID_GSM_MS, /* as found in WAV */
    CODEC_ID_ATRAC3,
    CODEC_ID_VOXWARE,
    CODEC_ID_APE,
    CODEC_ID_NELLYMOSER,
    CODEC_ID_MUSEPACK8,
    CODEC_ID_SPEEX,
    CODEC_ID_WMAVOICE,
    CODEC_ID_WMAPRO,
    CODEC_ID_WMALOSSLESS,
    CODEC_ID_ATRAC3P,
    CODEC_ID_EAC3,
    CODEC_ID_SIPR,
    CODEC_ID_MP1,
    CODEC_ID_TWINVQ,
    CODEC_ID_TRUEHD,
    CODEC_ID_MP4ALS,
    CODEC_ID_ATRAC1,
    CODEC_ID_BINKAUDIO_RDFT,
    CODEC_ID_BINKAUDIO_DCT,
    CODEC_ID_AAC_LATM,
    CODEC_ID_QDMC,
    CODEC_ID_CELT,
#if LIBAVCODEC_VERSION_MAJOR > 53
    CODEC_ID_G723_1,
    CODEC_ID_G729,
    CODEC_ID_8SVX_EXP,
    CODEC_ID_8SVX_FIB,
#endif
    CODEC_ID_BMV_AUDIO,

    /* subtitle codecs */
    CODEC_ID_FIRST_SUBTITLE = 0x17000,          ///< A dummy ID pointing at the start of subtitle codecs.
    CODEC_ID_DVD_SUBTITLE = 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  ///< raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_HDMV_PGS_SUBTITLE,
    CODEC_ID_DVB_TELETEXT,
    CODEC_ID_SRT,

    /* other specific kind of codecs (generally used for attachments) */
    CODEC_ID_FIRST_UNKNOWN = 0x18000,           ///< A dummy ID pointing at the start of various fake codecs.
    CODEC_ID_TTF = 0x18000,

    CODEC_ID_PROBE = 0x19000, ///< codec_id is not known (like CODEC_ID_NONE) but lavf should attempt to identify it

    CODEC_ID_MPEG2TS = 0x20000, /**< _FAKE_ codec to indicate a raw MPEG-2 TS
                                * stream (only used by libavformat) */
    CODEC_ID_MPEG4SYSTEMS = 0x20001, /**< _FAKE_ codec to indicate a MPEG-4 Systems
                                * stream (only used by libavformat) */
    CODEC_ID_FFMETADATA = 0x21000,   ///< Dummy codec for streams containing only metadata information.
};

#if FF_API_OLD_SAMPLE_FMT
#define SampleFormat AVSampleFormat

#define SAMPLE_FMT_NONE AV_SAMPLE_FMT_NONE
#define SAMPLE_FMT_U8   AV_SAMPLE_FMT_U8
#define SAMPLE_FMT_S16  AV_SAMPLE_FMT_S16
#define SAMPLE_FMT_S32  AV_SAMPLE_FMT_S32
#define SAMPLE_FMT_FLT  AV_SAMPLE_FMT_FLT
#define SAMPLE_FMT_DBL  AV_SAMPLE_FMT_DBL
#define SAMPLE_FMT_NB   AV_SAMPLE_FMT_NB
#endif

#if FF_API_OLD_AUDIOCONVERT
#include "libavutil/audioconvert.h"

/* Audio channel masks */
#define CH_FRONT_LEFT            AV_CH_FRONT_LEFT
#define CH_FRONT_RIGHT           AV_CH_FRONT_RIGHT
#define CH_FRONT_CENTER          AV_CH_FRONT_CENTER
#define CH_LOW_FREQUENCY         AV_CH_LOW_FREQUENCY
#define CH_BACK_LEFT             AV_CH_BACK_LEFT
#define CH_BACK_RIGHT            AV_CH_BACK_RIGHT
#define CH_FRONT_LEFT_OF_CENTER  AV_CH_FRONT_LEFT_OF_CENTER
#define CH_FRONT_RIGHT_OF_CENTER AV_CH_FRONT_RIGHT_OF_CENTER
#define CH_BACK_CENTER           AV_CH_BACK_CENTER
#define CH_SIDE_LEFT             AV_CH_SIDE_LEFT
#define CH_SIDE_RIGHT            AV_CH_SIDE_RIGHT
#define CH_TOP_CENTER            AV_CH_TOP_CENTER
#define CH_TOP_FRONT_LEFT        AV_CH_TOP_FRONT_LEFT
#define CH_TOP_FRONT_CENTER      AV_CH_TOP_FRONT_CENTER
#define CH_TOP_FRONT_RIGHT       AV_CH_TOP_FRONT_RIGHT
#define CH_TOP_BACK_LEFT         AV_CH_TOP_BACK_LEFT
#define CH_TOP_BACK_CENTER       AV_CH_TOP_BACK_CENTER
#define CH_TOP_BACK_RIGHT        AV_CH_TOP_BACK_RIGHT
#define CH_STEREO_LEFT           AV_CH_STEREO_LEFT
#define CH_STEREO_RIGHT          AV_CH_STEREO_RIGHT

/** Channel mask value used for AVCodecContext.request_channel_layout
    to indicate that the user requests the channel order of the decoder output
    to be the native codec channel order. */
#define CH_LAYOUT_NATIVE         AV_CH_LAYOUT_NATIVE

/* Audio channel convenience macros */
#define CH_LAYOUT_MONO           AV_CH_LAYOUT_MONO
#define CH_LAYOUT_STEREO         AV_CH_LAYOUT_STEREO
#define CH_LAYOUT_2_1            AV_CH_LAYOUT_2_1
#define CH_LAYOUT_SURROUND       AV_CH_LAYOUT_SURROUND
#define CH_LAYOUT_4POINT0        AV_CH_LAYOUT_4POINT0
#define CH_LAYOUT_2_2            AV_CH_LAYOUT_2_2
#define CH_LAYOUT_QUAD           AV_CH_LAYOUT_QUAD
#define CH_LAYOUT_5POINT0        AV_CH_LAYOUT_5POINT0
#define CH_LAYOUT_5POINT1        AV_CH_LAYOUT_5POINT1
#define CH_LAYOUT_5POINT0_BACK   AV_CH_LAYOUT_5POINT0_BACK
#define CH_LAYOUT_5POINT1_BACK   AV_CH_LAYOUT_5POINT1_BACK
#define CH_LAYOUT_7POINT0        AV_CH_LAYOUT_7POINT0
#define CH_LAYOUT_7POINT1        AV_CH_LAYOUT_7POINT1
#define CH_LAYOUT_7POINT1_WIDE   AV_CH_LAYOUT_7POINT1_WIDE
#define CH_LAYOUT_STEREO_DOWNMIX AV_CH_LAYOUT_STEREO_DOWNMIX
#endif

#if FF_API_OLD_DECODE_AUDIO
/* in bytes */
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 384000 // 1 second of 96khz 32bit audio
#endif

/**
 * Required number of additionally allocated bytes at the end of the input bitstream for decoding.
 * This is mainly needed because some optimized bitstream readers read
 * 32 or 64 bit at once and could read over the end.<br>
 * Note: If the first 23 bits of the additional bytes are not 0, then damaged
 * MPEG bitstreams could cause overread and segfault.
 */
#define FF_INPUT_BUFFER_PADDING_SIZE 16

/**
 * minimum encoding buffer size
 * Used to avoid some checks during header writing.
 */
#define FF_MIN_BUFFER_SIZE 16384


/**
 * motion estimation type.
 */
enum Motion_Est_ID {
    ME_ZERO = 1,    ///< no search, that is use 0,0 vector whenever one is needed
    ME_FULL,
    ME_LOG,
    ME_PHODS,
    ME_EPZS,        ///< enhanced predictive zonal search
    ME_X1,          ///< reserved for experiments
    ME_HEX,         ///< hexagon based search
    ME_UMH,         ///< uneven multi-hexagon search
    ME_ITER,        ///< iterative search
    ME_TESA,        ///< transformed exhaustive search algorithm
};

enum AVDiscard{
    /* We leave some space between them for extensions (drop some
     * keyframes for intra-only or drop just some bidir frames). */
    AVDISCARD_NONE   =-16, ///< discard nothing
    AVDISCARD_DEFAULT=  0, ///< discard useless packets like 0 size packets in avi
    AVDISCARD_NONREF =  8, ///< discard all non reference
    AVDISCARD_BIDIR  = 16, ///< discard all bidirectional frames
    AVDISCARD_NONKEY = 32, ///< discard all frames except keyframes
    AVDISCARD_ALL    = 48, ///< discard all
};

enum AVColorPrimaries{
    AVCOL_PRI_BT709      =1, ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
    AVCOL_PRI_UNSPECIFIED=2,
    AVCOL_PRI_BT470M     =4,
    AVCOL_PRI_BT470BG    =5, ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
    AVCOL_PRI_SMPTE170M  =6, ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
    AVCOL_PRI_SMPTE240M  =7, ///< functionally identical to above
    AVCOL_PRI_FILM       =8,
    AVCOL_PRI_NB           , ///< Not part of ABI
};

enum AVColorTransferCharacteristic{
    AVCOL_TRC_BT709      =1, ///< also ITU-R BT1361
    AVCOL_TRC_UNSPECIFIED=2,
    AVCOL_TRC_GAMMA22    =4, ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
    AVCOL_TRC_GAMMA28    =5, ///< also ITU-R BT470BG
    AVCOL_TRC_SMPTE240M  =7,
    AVCOL_TRC_NB           , ///< Not part of ABI
};

enum AVColorSpace{
    AVCOL_SPC_RGB        =0,
    AVCOL_SPC_BT709      =1, ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
    AVCOL_SPC_UNSPECIFIED=2,
    AVCOL_SPC_FCC        =4,
    AVCOL_SPC_BT470BG    =5, ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
    AVCOL_SPC_SMPTE170M  =6, ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC / functionally identical to above
    AVCOL_SPC_SMPTE240M  =7,
    AVCOL_SPC_YCGCO      =8,
    AVCOL_SPC_NB           , ///< Not part of ABI
};

enum AVColorRange{
    AVCOL_RANGE_UNSPECIFIED=0,
    AVCOL_RANGE_MPEG       =1, ///< the normal 219*2^(n-8) "MPEG" YUV ranges
    AVCOL_RANGE_JPEG       =2, ///< the normal     2^n-1   "JPEG" YUV ranges
    AVCOL_RANGE_NB           , ///< Not part of ABI
};

/**
 *  X   X      3 4 X      X are luma samples,
 *             1 2        1-6 are possible chroma positions
 *  X   X      5 6 X      0 is undefined/unknown position
 */
enum AVChromaLocation{
    AVCHROMA_LOC_UNSPECIFIED=0,
    AVCHROMA_LOC_LEFT       =1, ///< mpeg2/4, h264 default
    AVCHROMA_LOC_CENTER     =2, ///< mpeg1, jpeg, h263
    AVCHROMA_LOC_TOPLEFT    =3, ///< DV
    AVCHROMA_LOC_TOP        =4,
    AVCHROMA_LOC_BOTTOMLEFT =5,
    AVCHROMA_LOC_BOTTOM     =6,
    AVCHROMA_LOC_NB           , ///< Not part of ABI
};

enum AVAudioServiceType {
    AV_AUDIO_SERVICE_TYPE_MAIN              = 0,
    AV_AUDIO_SERVICE_TYPE_EFFECTS           = 1,
    AV_AUDIO_SERVICE_TYPE_VISUALLY_IMPAIRED = 2,
    AV_AUDIO_SERVICE_TYPE_HEARING_IMPAIRED  = 3,
    AV_AUDIO_SERVICE_TYPE_DIALOGUE          = 4,
    AV_AUDIO_SERVICE_TYPE_COMMENTARY        = 5,
    AV_AUDIO_SERVICE_TYPE_EMERGENCY         = 6,
    AV_AUDIO_SERVICE_TYPE_VOICE_OVER        = 7,
    AV_AUDIO_SERVICE_TYPE_KARAOKE           = 8,
    AV_AUDIO_SERVICE_TYPE_NB                   , ///< Not part of ABI
};

typedef enum {
    VIDEO_FULL_RANGE_TV         = 0,
    VIDEO_FULL_RANGE_PC         = 1,
    VIDEO_FULL_RANGE_INVALID    = 2
} VideoFullRangeType;

typedef struct RcOverride{
    int start_frame;
    int end_frame;
    int qscale; // If this is 0 then quality_factor will be used instead.
    float quality_factor;
} RcOverride;

#define FF_MAX_B_FRAMES 16

/* encoding support
   These flags can be passed in AVCodecContext.flags before initialization.
   Note: Not everything is supported yet.
*/

#define CODEC_FLAG_QSCALE 0x0002  ///< Use fixed qscale.
#define CODEC_FLAG_4MV    0x0004  ///< 4 MV per MB allowed / advanced prediction for H.263.
#define CODEC_FLAG_QPEL   0x0010  ///< Use qpel MC.
#define CODEC_FLAG_GMC    0x0020  ///< Use GMC.
#define CODEC_FLAG_MV0    0x0040  ///< Always try a MB with MV=<0,0>.
/**
 * The parent program guarantees that the input for B-frames containing
 * streams is not written to for at least s->max_b_frames+1 frames, if
 * this is not set the input will be copied.
 */
#define CODEC_FLAG_INPUT_PRESERVED 0x0100
#define CODEC_FLAG_PASS1           0x0200   ///< Use internal 2pass ratecontrol in first pass mode.
#define CODEC_FLAG_PASS2           0x0400   ///< Use internal 2pass ratecontrol in second pass mode.
#define CODEC_FLAG_GRAY            0x2000   ///< Only decode/encode grayscale.
#define CODEC_FLAG_EMU_EDGE        0x4000   ///< Don't draw edges.
#define CODEC_FLAG_PSNR            0x8000   ///< error[?] variables will be set during encoding.
#define CODEC_FLAG_TRUNCATED       0x00010000 /** Input bitstream might be truncated at a random
                                                  location instead of only at frame boundaries. */
#define CODEC_FLAG_NORMALIZE_AQP  0x00020000 ///< Normalize adaptive quantization.
#define CODEC_FLAG_INTERLACED_DCT 0x00040000 ///< Use interlaced DCT.
#define CODEC_FLAG_LOW_DELAY      0x00080000 ///< Force low delay.
#define CODEC_FLAG_GLOBAL_HEADER  0x00400000 ///< Place global headers in extradata instead of every keyframe.
#define CODEC_FLAG_BITEXACT       0x00800000 ///< Use only bitexact stuff (except (I)DCT).
/* Fx : Flag for h263+ extra options */
#define CODEC_FLAG_AC_PRED        0x01000000 ///< H.263 advanced intra coding / MPEG-4 AC prediction
#define CODEC_FLAG_LOOP_FILTER    0x00000800 ///< loop filter
#define CODEC_FLAG_INTERLACED_ME  0x20000000 ///< interlaced motion estimation
#define CODEC_FLAG_CLOSED_GOP     0x80000000
#define CODEC_FLAG2_FAST          0x00000001 ///< Allow non spec compliant speedup tricks.
#define CODEC_FLAG2_NO_OUTPUT     0x00000004 ///< Skip bitstream encoding.
#define CODEC_FLAG2_LOCAL_HEADER  0x00000008 ///< Place global headers at every keyframe instead of in extradata.
#if FF_API_MPV_GLOBAL_OPTS
#define CODEC_FLAG_CBP_RD         0x04000000 ///< Use rate distortion optimization for cbp.
#define CODEC_FLAG_QP_RD          0x08000000 ///< Use rate distortion optimization for qp selectioon.
#define CODEC_FLAG2_STRICT_GOP    0x00000002 ///< Strictly enforce GOP size.
#define CODEC_FLAG2_SKIP_RD       0x00004000 ///< RD optimal MB level residual skipping
#endif
#define CODEC_FLAG2_CHUNKS        0x00008000 ///< Input bitstream might be truncated at a packet boundaries instead of only at frame boundaries.

/* Unsupported options :
 *              Syntax Arithmetic coding (SAC)
 *              Reference Picture Selection
 *              Independent Segment Decoding */
/* /Fx */
/* codec capabilities */

#define CODEC_CAP_DRAW_HORIZ_BAND 0x0001 ///< Decoder can use draw_horiz_band callback.
/**
 * Codec uses get_buffer() for allocating buffers and supports custom allocators.
 * If not set, it might not use get_buffer() at all or use operations that
 * assume the buffer was allocated by avcodec_default_get_buffer.
 */
#define CODEC_CAP_DR1             0x0002
#define CODEC_CAP_TRUNCATED       0x0008
/* Codec can export data for HW decoding (XvMC). */
#define CODEC_CAP_HWACCEL         0x0010
/**
 * Encoder or decoder requires flushing with NULL input at the end in order to
 * give the complete and correct output.
 *
 * NOTE: If this flag is not set, the codec is guaranteed to never be fed with
 *       with NULL data. The user can still send NULL data to the public encode
 *       or decode function, but libavcodec will not pass it along to the codec
 *       unless this flag is set.
 *
 * Decoders:
 * The decoder has a non-zero delay and needs to be fed with avpkt->data=NULL,
 * avpkt->size=0 at the end to get the delayed data until the decoder no longer
 * returns frames.
 *
 * Encoders:
 * The encoder needs to be fed with NULL data at the end of encoding until the
 * encoder no longer returns data.
 *
 * NOTE: For encoders implementing the AVCodec.encode2() function, setting this
 *       flag also means that the encoder must set the pts and duration for
 *       each output packet. If this flag is not set, the pts and duration will
 *       be determined by libavcodec from the input frame.
 */
#define CODEC_CAP_DELAY           0x0020
/**
 * Codec can be fed a final frame with a smaller size.
 * This can be used to prevent truncation of the last audio samples.
 */
#define CODEC_CAP_SMALL_LAST_FRAME 0x0040
/**
 * Codec can export data for HW decoding (VDPAU).
 */
#define CODEC_CAP_HWACCEL_VDPAU    0x0080
/**
 * Codec can output multiple frames per AVPacket
 * Normally demuxers return one frame at a time, demuxers which do not do
 * are connected to a parser to split what they return into proper frames.
 * This flag is reserved to the very rare category of codecs which have a
 * bitstream that cannot be split into frames without timeconsuming
 * operations like full decoding. Demuxers carring such bitstreams thus
 * may return multiple frames in a packet. This has many disadvantages like
 * prohibiting stream copy in many cases thus it should only be considered
 * as a last resort.
 */
#define CODEC_CAP_SUBFRAMES        0x0100
/**
 * Codec is experimental and is thus avoided in favor of non experimental
 * encoders
 */
#define CODEC_CAP_EXPERIMENTAL     0x0200
/**
 * Codec should fill in channel configuration and samplerate instead of container
 */
#define CODEC_CAP_CHANNEL_CONF     0x0400
/**
 * Codec is able to deal with negative linesizes
 */
#define CODEC_CAP_NEG_LINESIZES    0x0800
/**
 * Codec supports frame-level multithreading.
 */
#define CODEC_CAP_FRAME_THREADS    0x1000
/**
 * Codec supports slice-based (or partition-based) multithreading.
 */
#define CODEC_CAP_SLICE_THREADS    0x2000
/**
 * Codec supports changed parameters at any point.
 */
#define CODEC_CAP_PARAM_CHANGE     0x4000
/**
 * Codec supports avctx->thread_count == 0 (auto).
 */
#define CODEC_CAP_AUTO_THREADS     0x8000
/**
 * Audio encoder supports receiving a different number of samples in each call.
 */
#define CODEC_CAP_VARIABLE_FRAME_SIZE 0x10000

//The following defines may change, don't expect compatibility if you use them.
#define MB_TYPE_INTRA4x4   0x0001
#define MB_TYPE_INTRA16x16 0x0002 //FIXME H.264-specific
#define MB_TYPE_INTRA_PCM  0x0004 //FIXME H.264-specific
#define MB_TYPE_16x16      0x0008
#define MB_TYPE_16x8       0x0010
#define MB_TYPE_8x16       0x0020
#define MB_TYPE_8x8        0x0040
#define MB_TYPE_INTERLACED 0x0080
#define MB_TYPE_DIRECT2    0x0100 //FIXME
#define MB_TYPE_ACPRED     0x0200
#define MB_TYPE_GMC        0x0400
#define MB_TYPE_SKIP       0x0800
#define MB_TYPE_P0L0       0x1000
#define MB_TYPE_P1L0       0x2000
#define MB_TYPE_P0L1       0x4000
#define MB_TYPE_P1L1       0x8000
#define MB_TYPE_L0         (MB_TYPE_P0L0 | MB_TYPE_P1L0)
#define MB_TYPE_L1         (MB_TYPE_P0L1 | MB_TYPE_P1L1)
#define MB_TYPE_L0L1       (MB_TYPE_L0   | MB_TYPE_L1)
#define MB_TYPE_QUANT      0x00010000
#define MB_TYPE_CBP        0x00020000
//Note bits 24-31 are reserved for codec specific use (h264 ref0, mpeg1 0mv, ...)

/**
 * Pan Scan area.
 * This specifies the area which should be displayed.
 * Note there may be multiple such areas for one frame.
 */
typedef struct AVPanScan{
    /**
     * id
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int id;

    /**
     * width and height in 1/16 pel
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int width;
    int height;

    /**
     * position of the top left corner in 1/16 pel for up to 3 fields/frames
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int16_t position[3][2];
}AVPanScan;

#define FF_QSCALE_TYPE_MPEG1 0
#define FF_QSCALE_TYPE_MPEG2 1
#define FF_QSCALE_TYPE_H264  2
#define FF_QSCALE_TYPE_VP56  3

#define FF_BUFFER_TYPE_INTERNAL 1
#define FF_BUFFER_TYPE_USER     2 ///< direct rendering buffers (image is (de)allocated by user)
#define FF_BUFFER_TYPE_SHARED   4 ///< Buffer from somewhere else; don't deallocate image (data/base), all other tables are not shared.
#define FF_BUFFER_TYPE_COPY     8 ///< Just a (modified) copy of some other buffer, don't deallocate anything.

#define FF_BUFFER_HINTS_VALID    0x01 // Buffer hints value is meaningful (if 0 ignore).
#define FF_BUFFER_HINTS_READABLE 0x02 // Codec will read from buffer.
#define FF_BUFFER_HINTS_PRESERVE 0x04 // User must not alter buffer content.
#define FF_BUFFER_HINTS_REUSABLE 0x08 // Codec will reuse the buffer (update).

enum AVPacketSideDataType {
    AV_PKT_DATA_PALETTE,
    AV_PKT_DATA_NEW_EXTRADATA,
    AV_PKT_DATA_PARAM_CHANGE,
    AV_PKT_DATA_H263_MB_INFO,
};

typedef struct AVPacket {
    /**
     * Presentation timestamp in AVStream->time_base units; the time at which
     * the decompressed packet will be presented to the user.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     * pts MUST be larger or equal to dts as presentation cannot happen before
     * decompression, unless one wants to view hex dumps. Some formats misuse
     * the terms dts and pts/cts to mean something different. Such timestamps
     * must be converted to true pts/dts before they are stored in AVPacket.
     */
    int64_t pts;
    /**
     * Decompression timestamp in AVStream->time_base units; the time at which
     * the packet is decompressed.
     * Can be AV_NOPTS_VALUE if it is not stored in the file.
     */
    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;
    /**
     * A combination of AV_PKT_FLAG values
     */
    int   flags;
    /**
     * Additional packet data that can be provided by the container.
     * Packet can contain several types of side information.
     */
    struct {
        uint8_t *data;
        int      size;
        enum AVPacketSideDataType type;
    } *side_data;
    int side_data_elems;

    /**
     * Duration of this packet in AVStream->time_base units, 0 if unknown.
     * Equals next_pts - this_pts in presentation order.
     */
    int   duration;
    void  (*destruct)(struct AVPacket *);
    void  *priv;
    int64_t pos;                            ///< byte position in stream, -1 if unknown

    /**
     * Time difference in AVStream->time_base units from the pts of this
     * packet to the point at which the output from the decoder has converged
     * independent from the availability of previous frames. That is, the
     * frames are virtually identical no matter if decoding started from
     * the very first frame or from this keyframe.
     * Is AV_NOPTS_VALUE if unknown.
     * This field is not the display duration of the current packet.
     * This field has no meaning if the packet does not have AV_PKT_FLAG_KEY
     * set.
     *
     * The purpose of this field is to allow seeking in streams that have no
     * keyframes in the conventional sense. It corresponds to the
     * recovery point SEI in H.264 and match_time_delta in NUT. It is also
     * essential for some types of subtitle streams to ensure that all
     * subtitles are correctly displayed after seeking.
     */
    int64_t convergence_duration;
} AVPacket;
#define AV_PKT_FLAG_KEY     0x0001 ///< The packet contains a keyframe
#define AV_PKT_FLAG_CORRUPT 0x0002 ///< The packet content is corrupted

/**
 * An AV_PKT_DATA_PARAM_CHANGE side data packet is laid out as follows:
 * u32le param_flags
 * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT)
 *     s32le channel_count
 * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT)
 *     u64le channel_layout
 * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE)
 *     s32le sample_rate
 * if (param_flags & AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS)
 *     s32le width
 *     s32le height
 */

/**
 * An AV_PKT_DATA_H263_MB_INFO side data packet contains a number of
 * structures with info about macroblocks relevant to splitting the
 * packet into smaller packets on macroblock edges (e.g. as for RFC 2190).
 * That is, it does not necessarily contain info about all macroblocks,
 * as long as the distance between macroblocks in the info is smaller
 * than the target payload size.
 * Each MB info structure is 12 bytes, and is laid out as follows:
 * u32le bit offset from the start of the packet
 * u8    current quantizer at the start of the macroblock
 * u8    GOB number
 * u16le macroblock address within the GOB
 * u8    horizontal MV predictor
 * u8    vertical MV predictor
 * u8    horizontal MV predictor for block number 3
 * u8    vertical MV predictor for block number 3
 */

enum AVSideDataParamChangeFlags {
    AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT  = 0x0001,
    AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT = 0x0002,
    AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE    = 0x0004,
    AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS     = 0x0008,
};

/**
 * Audio Video Frame.
 * New fields can be added to the end of AVFRAME with minor version
 * bumps. Removal, reordering and changes to existing fields require
 * a major version bump.
 * sizeof(AVFrame) must not be used outside libav*.
 */
typedef struct AVFrame {
#define AV_NUM_DATA_POINTERS 8
    /**
     * pointer to the picture/channel planes.
     * This might be different from the first allocated byte
     * - encoding: Set by user
     * - decoding: set by AVCodecContext.get_buffer()
     */
    uint8_t *data[AV_NUM_DATA_POINTERS];

    /**
     * Size, in bytes, of the data for each picture/channel plane.
     *
     * For audio, only linesize[0] may be set. For planar audio, each channel
     * plane must be the same size.
     *
     * - encoding: Set by user
     * - decoding: set by AVCodecContext.get_buffer()
     */
    int linesize[AV_NUM_DATA_POINTERS];

    /**
     * pointers to the data planes/channels.
     *
     * For video, this should simply point to data[].
     *
     * For planar audio, each channel has a separate data pointer, and
     * linesize[0] contains the size of each channel buffer.
     * For packed audio, there is just one data pointer, and linesize[0]
     * contains the total size of the buffer for all channels.
     *
     * Note: Both data and extended_data will always be set by get_buffer(),
     * but for planar audio with more channels that can fit in data,
     * extended_data must be used by the decoder in order to access all
     * channels.
     *
     * encoding: unused
     * decoding: set by AVCodecContext.get_buffer()
     */
    uint8_t **extended_data;

    /**
     * width and height of the video frame
     * - encoding: unused
     * - decoding: Read by user.
     */
    int width, height;

    /**
     * number of audio samples (per channel) described by this frame
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    int nb_samples;

    /**
     * format of the frame, -1 if unknown or unset
     * Values correspond to enum PixelFormat for video frames,
     * enum AVSampleFormat for audio)
     * - encoding: unused
     * - decoding: Read by user.
     */
    int format;

    /**
     * 1 -> keyframe, 0-> not
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    int key_frame;

    /**
     * Picture type of the frame, see ?_TYPE below.
     * - encoding: Set by libavcodec. for coded_picture (and set by user for input).
     * - decoding: Set by libavcodec.
     */
    enum AVPictureType pict_type;

    /**
     * pointer to the first allocated byte of the picture. Can be used in get_buffer/release_buffer.
     * This isn't used by libavcodec unless the default get/release_buffer() is used.
     * - encoding:
     * - decoding:
     */
    uint8_t *base[AV_NUM_DATA_POINTERS];

    /**
     * sample aspect ratio for the video frame, 0/1 if unknown\unspecified
     * - encoding: unused
     * - decoding: Read by user.
     */
    AVRational sample_aspect_ratio;

    /**
     * presentation timestamp in time_base units (time when frame should be shown to user)
     * If AV_NOPTS_VALUE then frame_rate = 1/time_base will be assumed.
     * - encoding: MUST be set by user.
     * - decoding: Set by libavcodec.
     */
    int64_t pts;

    /**
     * reordered pts from the last AVPacket that has been input into the decoder
     * - encoding: unused
     * - decoding: Read by user.
     */
    int64_t pkt_pts;

    /**
     * dts from the last AVPacket that has been input into the decoder
     * - encoding: unused
     * - decoding: Read by user.
     */
    int64_t pkt_dts;

    /**
     * picture number in bitstream order
     * - encoding: set by
     * - decoding: Set by libavcodec.
     */
    int coded_picture_number;
    /**
     * picture number in display order
     * - encoding: set by
     * - decoding: Set by libavcodec.
     */
    int display_picture_number;

    /**
     * quality (between 1 (good) and FF_LAMBDA_MAX (bad))
     * - encoding: Set by libavcodec. for coded_picture (and set by user for input).
     * - decoding: Set by libavcodec.
     */
    int quality;

    /**
     * is this picture used as reference
     * The values for this are the same as the MpegEncContext.picture_structure
     * variable, that is 1->top field, 2->bottom field, 3->frame/both fields.
     * Set to 4 for delayed, non-reference frames.
     * - encoding: unused
     * - decoding: Set by libavcodec. (before get_buffer() call)).
     */
    int reference;

    /**
     * QP table
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    int8_t *qscale_table;
    /**
     * QP store stride
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    int qstride;

    /**
     *
     */
    int qscale_type;

    /**
     * mbskip_table[mb]>=1 if MB didn't change
     * stride= mb_width = (width+15)>>4
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    uint8_t *mbskip_table;

    /**
     * motion vector table
     * @code
     * example:
     * int mv_sample_log2= 4 - motion_subsample_log2;
     * int mb_width= (width+15)>>4;
     * int mv_stride= (mb_width << mv_sample_log2) + 1;
     * motion_val[direction][x + y*mv_stride][0->mv_x, 1->mv_y];
     * @endcode
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int16_t (*motion_val[2])[2];

    /**
     * macroblock type table
     * mb_type_base + mb_width + 2
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    uint32_t *mb_type;

    /**
     * DCT coefficients
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    short *dct_coeff;

    /**
     * motion reference frame index
     * the order in which these are stored can depend on the codec.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int8_t *ref_index[2];

    /**
     * for some private data of the user
     * - encoding: unused
     * - decoding: Set by user.
     */
    void *opaque;

    /**
     * error
     * - encoding: Set by libavcodec. if flags&CODEC_FLAG_PSNR.
     * - decoding: unused
     */
    uint64_t error[AV_NUM_DATA_POINTERS];

    /**
     * type of the buffer (to keep track of who has to deallocate data[*])
     * - encoding: Set by the one who allocates it.
     * - decoding: Set by the one who allocates it.
     * Note: User allocated (direct rendering) & internal buffers cannot coexist currently.
     */
    int type;

    /**
     * When decoding, this signals how much the picture must be delayed.
     * extra_delay = repeat_pict / (2*fps)
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    int repeat_pict;

    /**
     * The content of the picture is interlaced.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec. (default 0)
     */
    int interlaced_frame;

    /**
     * If the content is interlaced, is top field displayed first.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    int top_field_first;

    /**
     * Tell user application that palette has changed from previous frame.
     * - encoding: ??? (no palette-enabled encoder yet)
     * - decoding: Set by libavcodec. (default 0).
     */
    int palette_has_changed;

    /**
     * codec suggestion on buffer type if != 0
     * - encoding: unused
     * - decoding: Set by libavcodec. (before get_buffer() call)).
     */
    int buffer_hints;

    /**
     * Pan scan.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    AVPanScan *pan_scan;

    /**
     * reordered opaque 64bit (generally an integer or a double precision float
     * PTS but can be anything).
     * The user sets AVCodecContext.reordered_opaque to represent the input at
     * that time,
     * the decoder reorders values as needed and sets AVFrame.reordered_opaque
     * to exactly one of the values provided by the user through AVCodecContext.reordered_opaque
     * @deprecated in favor of pkt_pts
     * - encoding: unused
     * - decoding: Read by user.
     */
    int64_t reordered_opaque;
    int64_t reordered_opaque2; /* ffdshow custom code */
    int64_t reordered_opaque3; /* ffdshow custom code */

    /**
     * hardware accelerator private data (Libav-allocated)
     * - encoding: unused
     * - decoding: Set by libavcodec
     */
    void *hwaccel_picture_private;

    /**
     * the AVCodecContext which ff_thread_get_buffer() was last called on
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    struct AVCodecContext *owner;

    /**
     * used by multithreading to store frame-specific info
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    void *thread_opaque;

    /**
     * log2 of the size of the block which a single vector in motion_val represents:
     * (4->16x16, 3->8x8, 2-> 4x4, 1-> 2x2)
     * - encoding: unused
     * - decoding: Set by libavcodec.
     */
    uint8_t motion_subsample_log2;

    /* ffdshow custom code (begin) */
    int mb_width,mb_height,mb_stride;
    int num_sprite_warping_points,real_sprite_warping_points;
    int play_flags;

    int h264_poc_decoded;
    int h264_poc_outputed;
    int h264_frame_num_decoded;
    int h264_max_frame_num;

    int mpeg2_sequence_end_flag;
    /* ffdshow custom code (end) */
} AVFrame;

struct AVCodecInternal;

enum AVFieldOrder {
    AV_FIELD_UNKNOWN,
    AV_FIELD_PROGRESSIVE,
    AV_FIELD_TT,          //< Top coded_first, top displayed first
    AV_FIELD_BB,          //< Bottom coded first, bottom displayed first
    AV_FIELD_TB,          //< Top coded first, bottom displayed first
    AV_FIELD_BT,          //< Bottom coded first, top displayed first
};

/**
 * main external API structure.
 * New fields can be added to the end with minor version bumps.
 * Removal, reordering and changes to existing fields require a major
 * version bump.
 * sizeof(AVCodecContext) must not be used outside libav*.
 */
struct TlibavcodecExt;
typedef struct AVCodecContext {
    /**
     * information on struct for av_log
     * - set by avcodec_alloc_context3
     */
    const AVClass *av_class;
    int log_level_offset;

    enum AVMediaType codec_type; /* see AVMEDIA_TYPE_xxx */
    struct AVCodec  *codec;
    char             codec_name[32];
    enum CodecID     codec_id; /* see CODEC_ID_xxx */

    /**
     * fourcc (LSB first, so "ABCD" -> ('D'<<24) + ('C'<<16) + ('B'<<8) + 'A').
     * This is used to work around some encoder bugs.
     * A demuxer should set this to what is stored in the field used to identify the codec.
     * If there are multiple such fields in a container then the demuxer should choose the one
     * which maximizes the information about the used codec.
     * If the codec tag field in a container is larger than 32 bits then the demuxer should
     * remap the longer ID to 32 bits with a table or other structure. Alternatively a new
     * extra_codec_tag + size could be added but for this a clear advantage must be demonstrated
     * first.
     * - encoding: Set by user, if not then the default based on codec_id will be used.
     * - decoding: Set by user, will be converted to uppercase by libavcodec during init.
     */
    unsigned int codec_tag;

    /**
     * fourcc from the AVI stream header (LSB first, so "ABCD" -> ('D'<<24) + ('C'<<16) + ('B'<<8) + 'A').
     * This is used to work around some encoder bugs.
     * - encoding: unused
     * - decoding: Set by user, will be converted to uppercase by libavcodec during init.
     */
    unsigned int stream_codec_tag;

    /**
     * Some codecs need additional format info. It is stored here.
     * If any muxer uses this then ALL demuxers/parsers AND encoders for the
     * specific codec MUST set it correctly otherwise stream copy breaks.
     * In general use of this field by muxers is not recommended.
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec. (FIXME: Is this OK?)
     */
    int sub_id;

    void *priv_data;

    /**
     * Private context used for internal data.
     *
     * Unlike priv_data, this is not codec-specific. It is used in general
     * libavcodec functions.
     */
    struct AVCodecInternal *internal;

    /**
     * Private data of the user, can be used to carry app specific stuff.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    struct TlibavcodecExt *opaque;

    /**
     * the average bitrate
     * - encoding: Set by user; unused for constant quantizer encoding.
     * - decoding: Set by libavcodec. 0 or some bitrate if this info is available in the stream.
     */
    int bit_rate;

    /**
     * number of bits the bitstream is allowed to diverge from the reference.
     *           the reference can be CBR (for CBR pass1) or VBR (for pass2)
     * - encoding: Set by user; unused for constant quantizer encoding.
     * - decoding: unused
     */
    int bit_rate_tolerance;

    /**
     * Global quality for codecs which cannot change it per frame.
     * This should be proportional to MPEG-1/2/4 qscale.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int global_quality;

    /**
     * - encoding: Set by user.
     * - decoding: unused
     */
    int compression_level;
#define FF_COMPRESSION_DEFAULT -1

    /**
     * CODEC_FLAG_*.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int flags;

    /**
     * CODEC_FLAG2_*
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int flags2;

    /**
     * some codecs need / can use extradata like Huffman tables.
     * mjpeg: Huffman tables
     * rv10: additional flags
     * mpeg4: global headers (they can be in the bitstream or here)
     * The allocated memory should be FF_INPUT_BUFFER_PADDING_SIZE bytes larger
     * than extradata_size to avoid prolems if it is read with the bitstream reader.
     * The bytewise contents of extradata must not depend on the architecture or CPU endianness.
     * - encoding: Set/allocated/freed by libavcodec.
     * - decoding: Set/allocated/freed by user.
     */
    const uint8_t *extradata;
    int extradata_size;

    /**
     * This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identically 1.
     * - encoding: MUST be set by user.
     * - decoding: Set by libavcodec.
     */
    AVRational time_base;

    /**
     * For some codecs, the time base is closer to the field rate than the frame rate.
     * Most notably, H.264 and MPEG-2 specify time_base as half of frame duration
     * if no telecine is used ...
     *
     * Set to time_base ticks per frame. Default 1, e.g., H.264/MPEG-2 set it to 2.
     */
    int ticks_per_frame;

    /**
     * Encoder delay.
     *
     * Video:
     *   Number of frames the decoded output will be delayed relative to the
     *   encoded input.
     *
     * Audio:
     *   Number of "priming" samples added to the beginning of the stream
     *   during encoding. The decoded output will be delayed by this many
     *   samples relative to the input to the encoder. Note that this field is
     *   purely informational and does not directly affect the pts output by
     *   the encoder, which should always be based on the actual presentation
     *   time, including any delay.
     *
     * - encoding: Set by libavcodec.
     * - decoding: unused
     */
    int delay;


    /* video only */
    /**
     * picture width / height.
     * - encoding: MUST be set by user.
     * - decoding: Set by libavcodec.
     * Note: For compatibility it is possible to set this instead of
     * coded_width/height before decoding.
     */
    int width, height;

    /**
     * Bitstream width / height, may be different from width/height if lowres enabled.
     * - encoding: unused
     * - decoding: Set by user before init if known. Codec should override / dynamically change if needed.
     */
    int coded_width, coded_height;

#define FF_ASPECT_EXTENDED 15

    /**
     * the number of pictures in a group of pictures, or 0 for intra_only
     * - encoding: Set by user.
     * - decoding: unused
     */
    int gop_size;

    /**
     * Pixel format, see PIX_FMT_xxx.
     * May be set by the demuxer if known from headers.
     * May be overriden by the decoder if it knows better.
     * - encoding: Set by user.
     * - decoding: Set by user if known, overridden by libavcodec if known
     */
    enum PixelFormat pix_fmt;

    /**
     * Motion estimation algorithm used for video coding.
     * 1 (zero), 2 (full), 3 (log), 4 (phods), 5 (epzs), 6 (x1), 7 (hex),
     * 8 (umh), 9 (iter), 10 (tesa) [7, 8, 10 are x264 specific, 9 is snow specific]
     * - encoding: MUST be set by user.
     * - decoding: unused
     */
    int me_method;

    /**
     * If non NULL, 'draw_horiz_band' is called by the libavcodec
     * decoder to draw a horizontal band. It improves cache usage. Not
     * all codecs can do that. You must check the codec capabilities
     * beforehand.
     * When multithreading is used, it may be called from multiple threads
     * at the same time; threads might draw different parts of the same AVFrame,
     * or multiple AVFrames, and there is no guarantee that slices will be drawn
     * in order.
     * The function is also used by hardware acceleration APIs.
     * It is called at least once during frame decoding to pass
     * the data needed for hardware render.
     * In that mode instead of pixel data, AVFrame points to
     * a structure specific to the acceleration API. The application
     * reads the structure and can change some fields to indicate progress
     * or mark state.
     * - encoding: unused
     * - decoding: Set by user.
     * @param height the height of the slice
     * @param y the y position of the slice
     * @param type 1->top field, 2->bottom field, 3->frame
     * @param offset offset into the AVFrame.data from which the slice should be read
     */
    void (*draw_horiz_band)(struct AVCodecContext *s,
                            const AVFrame *src, int offset[AV_NUM_DATA_POINTERS],
                            int y, int type, int height);

    /**
     * callback to negotiate the pixelFormat
     * @param fmt is the list of formats which are supported by the codec,
     * it is terminated by -1 as 0 is a valid format, the formats are ordered by quality.
     * The first is always the native one.
     * @return the chosen format
     * - encoding: unused
     * - decoding: Set by user, if not set the native format will be chosen.
     */
    enum PixelFormat (*get_format)(struct AVCodecContext *s, const enum PixelFormat * fmt);

    /**
     * maximum number of B-frames between non-B-frames
     * Note: The output will be delayed by max_b_frames+1 relative to the input.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int max_b_frames;

    /**
     * qscale factor between IP and B-frames
     * If > 0 then the last P-frame quantizer will be used (q= lastp_q*factor+offset).
     * If < 0 then normal ratecontrol will be done (q= -normal_q*factor+offset).
     * - encoding: Set by user.
     * - decoding: unused
     */
    float b_quant_factor;

    /** obsolete FIXME remove */
    int rc_strategy;
#define FF_RC_STRATEGY_XVID 1

    int b_frame_strategy;

#if FF_API_MPV_GLOBAL_OPTS
    /**
     * luma single coefficient elimination threshold
     * - encoding: Set by user.
     * - decoding: unused
     */
    attribute_deprecated int luma_elim_threshold;

    /**
     * chroma single coeff elimination threshold
     * - encoding: Set by user.
     * - decoding: unused
     */
    attribute_deprecated int chroma_elim_threshold;
#endif

    /**
     * qscale offset between IP and B-frames
     * - encoding: Set by user.
     * - decoding: unused
     */
    float b_quant_offset;

    /**
     * Size of the frame reordering buffer in the decoder.
     * For MPEG-2 it is 1 IPB or 0 low delay IP.
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    int has_b_frames;

    /**
     * 0-> h263 quant 1-> mpeg quant
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mpeg_quant;

    /**
     * qscale factor between P and I-frames
     * If > 0 then the last p frame quantizer will be used (q= lastp_q*factor+offset).
     * If < 0 then normal ratecontrol will be done (q= -normal_q*factor+offset).
     * - encoding: Set by user.
     * - decoding: unused
     */
    float i_quant_factor;

    /**
     * qscale offset between P and I-frames
     * - encoding: Set by user.
     * - decoding: unused
     */
    float i_quant_offset;

    /**
     * luminance masking (0-> disabled)
     * - encoding: Set by user.
     * - decoding: unused
     */
    float lumi_masking;

    /**
     * temporary complexity masking (0-> disabled)
     * - encoding: Set by user.
     * - decoding: unused
     */
    float temporal_cplx_masking;

    /**
     * spatial complexity masking (0-> disabled)
     * - encoding: Set by user.
     * - decoding: unused
     */
    float spatial_cplx_masking;

    /**
     * p block masking (0-> disabled)
     * - encoding: Set by user.
     * - decoding: unused
     */
    float p_masking;

    /**
     * darkness masking (0-> disabled)
     * - encoding: Set by user.
     * - decoding: unused
     */
    float dark_masking;

    /**
     * slice count
     * - encoding: Set by libavcodec.
     * - decoding: Set by user (or 0).
     */
    int slice_count;
    /**
     * prediction method (needed for huffyuv)
     * - encoding: Set by user.
     * - decoding: unused
     */
     int prediction_method;
#define FF_PRED_LEFT   0
#define FF_PRED_PLANE  1
#define FF_PRED_MEDIAN 2

    /**
     * slice offsets in the frame in bytes
     * - encoding: Set/allocated by libavcodec.
     * - decoding: Set/allocated by user (or NULL).
     */
    int *slice_offset;

    /**
     * sample aspect ratio (0 if unknown)
     * That is the width of a pixel divided by the height of the pixel.
     * Numerator and denominator must be relatively prime and smaller than 256 for some video standards.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    AVRational sample_aspect_ratio;

    /* ffdshow custom code
     * Regarding MPEG-2, currently there are two ways of encoding SAR.
     * Of course one is wrong. However, considerable number of videos are encoded in a wrong way.
     * We set the spec compliant value in sample_aspect_ratio and
     * wrong spec-value in sample_aspect_ratio2.
     */
    AVRational sample_aspect_ratio2;

    /**
     * motion estimation comparison function
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_cmp;
    /**
     * subpixel motion estimation comparison function
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_sub_cmp;
    /**
     * macroblock comparison function (not supported yet)
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mb_cmp;
    /**
     * interlaced DCT comparison function
     * - encoding: Set by user.
     * - decoding: unused
     */
    int ildct_cmp;
#define FF_CMP_SAD    0
#define FF_CMP_SSE    1
#define FF_CMP_SATD   2
#define FF_CMP_DCT    3
#define FF_CMP_PSNR   4
#define FF_CMP_BIT    5
#define FF_CMP_RD     6
#define FF_CMP_ZERO   7
#define FF_CMP_VSAD   8
#define FF_CMP_VSSE   9
#define FF_CMP_NSSE   10
#define FF_CMP_W53    11
#define FF_CMP_W97    12
#define FF_CMP_DCTMAX 13
#define FF_CMP_DCT264 14
#define FF_CMP_CHROMA 256

    /**
     * ME diamond size & shape
     * - encoding: Set by user.
     * - decoding: unused
     */
    int dia_size;

    /**
     * amount of previous MV predictors (2a+1 x 2a+1 square)
     * - encoding: Set by user.
     * - decoding: unused
     */
    int last_predictor_count;

    /**
     * prepass for motion estimation
     * - encoding: Set by user.
     * - decoding: unused
     */
    int pre_me;

    /**
     * motion estimation prepass comparison function
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_pre_cmp;

    /**
     * ME prepass diamond size & shape
     * - encoding: Set by user.
     * - decoding: unused
     */
    int pre_dia_size;

    /**
     * subpel ME quality
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_subpel_quality;

    /**
     * DTG active format information (additional aspect ratio
     * information only used in DVB MPEG-2 transport streams)
     * 0 if not set.
     *
     * - encoding: unused
     * - decoding: Set by decoder.
     */
    int dtg_active_format;
#define FF_DTG_AFD_SAME         8
#define FF_DTG_AFD_4_3          9
#define FF_DTG_AFD_16_9         10
#define FF_DTG_AFD_14_9         11
#define FF_DTG_AFD_4_3_SP_14_9  13
#define FF_DTG_AFD_16_9_SP_14_9 14
#define FF_DTG_AFD_SP_4_3       15

    /**
     * maximum motion estimation search range in subpel units
     * If 0 then no limit.
     *
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_range;

    /**
     * intra quantizer bias
     * - encoding: Set by user.
     * - decoding: unused
     */
    int intra_quant_bias;
#define FF_DEFAULT_QUANT_BIAS 999999

    /**
     * inter quantizer bias
     * - encoding: Set by user.
     * - decoding: unused
     */
    int inter_quant_bias;

#if FF_API_COLOR_TABLE_ID
    /**
     * color table ID
     * - encoding: unused
     * - decoding: Which clrtable should be used for 8bit RGB images.
     *             Tables have to be stored somewhere. FIXME
     */
    attribute_deprecated int color_table_id;
#endif

    /**
     * slice flags
     * - encoding: unused
     * - decoding: Set by user.
     */
    int slice_flags;
#define SLICE_FLAG_CODED_ORDER    0x0001 ///< draw_horiz_band() is called in coded order instead of display
#define SLICE_FLAG_ALLOW_FIELD    0x0002 ///< allow draw_horiz_band() with field slices (MPEG2 field pics)
#define SLICE_FLAG_ALLOW_PLANE    0x0004 ///< allow draw_horiz_band() with 1 component at a time (SVQ1)

    /**
     * XVideo Motion Acceleration
     * - encoding: forbidden
     * - decoding: set by decoder
     */
    int xvmc_acceleration;

    /**
     * macroblock decision mode
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mb_decision;
#define FF_MB_DECISION_SIMPLE 0        ///< uses mb_cmp
#define FF_MB_DECISION_BITS   1        ///< chooses the one which needs the fewest bits
#define FF_MB_DECISION_RD     2        ///< rate distortion

    /**
     * custom intra quantization matrix
     * - encoding: Set by user, can be NULL.
     * - decoding: Set by libavcodec.
     */
    uint16_t *intra_matrix;

    /**
     * custom inter quantization matrix
     * - encoding: Set by user, can be NULL.
     * - decoding: Set by libavcodec.
     */
    uint16_t *inter_matrix;

    /**
     * scene change detection threshold
     * 0 is default, larger means fewer detected scene changes.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int scenechange_threshold;

    /**
     * noise reduction strength
     * - encoding: Set by user.
     * - decoding: unused
     */
    int noise_reduction;

#if FF_API_INTER_THRESHOLD
    /**
     * @deprecated this field is unused
     */
    attribute_deprecated int inter_threshold;
#endif

#if FF_API_MPV_GLOBAL_OPTS
    /**
     * @deprecated use mpegvideo private options instead
     */
    attribute_deprecated int quantizer_noise_shaping;
#endif

    /**
     * Motion estimation threshold below which no motion estimation is
     * performed, but instead the user specified motion vectors are used.
     *
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_threshold;

    /**
     * Macroblock threshold below which the user specified macroblock types will be used.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mb_threshold;

    /**
     * precision of the intra DC coefficient - 8
     * - encoding: Set by user.
     * - decoding: unused
     */
    int intra_dc_precision;

    /**
     * Number of macroblock rows at the top which are skipped.
     * - encoding: unused
     * - decoding: Set by user.
     */
    int skip_top;

    /**
     * Number of macroblock rows at the bottom which are skipped.
     * - encoding: unused
     * - decoding: Set by user.
     */
    int skip_bottom;

    /**
     * Border processing masking, raises the quantizer for mbs on the borders
     * of the picture.
     * - encoding: Set by user.
     * - decoding: unused
     */
    float border_masking;

    /**
     * minimum MB lagrange multipler
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mb_lmin;

    /**
     * maximum MB lagrange multipler
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mb_lmax;

    /**
     *
     * - encoding: Set by user.
     * - decoding: unused
     */
    int me_penalty_compensation;

    /**
     *
     * - encoding: Set by user.
     * - decoding: unused
     */
    int bidir_refine;

    /**
     *
     * - encoding: Set by user.
     * - decoding: unused
     */
    int brd_scale;

    /**
     * minimum GOP size
     * - encoding: Set by user.
     * - decoding: unused
     */
    int keyint_min;

    /**
     * number of reference frames
     * - encoding: Set by user.
     * - decoding: Set by lavc.
     */
    int refs;

    /**
     * chroma qp offset from luma
     * - encoding: Set by user.
     * - decoding: unused
     */
    int chromaoffset;

    /**
     * Multiplied by qscale for each frame and added to scene_change_score.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int scenechange_factor;

    /**
     *
     * Note: Value depends upon the compare function used for fullpel ME.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int mv0_threshold;

    /**
     * Adjust sensitivity of b_frame_strategy 1.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int b_sensitivity;

    /**
     * Chromaticity coordinates of the source primaries.
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    enum AVColorPrimaries color_primaries;

    /**
     * Color Transfer Characteristic.
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    enum AVColorTransferCharacteristic color_trc;

    /**
     * YUV colorspace type.
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    enum AVColorSpace colorspace;

    /**
     * MPEG vs JPEG YUV range.
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    enum AVColorRange color_range;

    /**
     * This defines the location of chroma samples.
     * - encoding: Set by user
     * - decoding: Set by libavcodec
     */
    enum AVChromaLocation chroma_sample_location;

    /**
     * Number of slices.
     * Indicates number of picture subdivisions. Used for parallelized
     * decoding.
     * - encoding: Set by user
     * - decoding: unused
     */
    int slices;

    /** Field order
     * - encoding: set by libavcodec
     * - decoding: Set by libavcodec
     */
    enum AVFieldOrder field_order;

    /* audio only */
    int sample_rate; ///< samples per second
    int channels;    ///< number of audio channels

    /**
     * audio sample format
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    enum AVSampleFormat sample_fmt;  ///< sample format

    /* The following data should not be initialized. */
    /**
     * Samples per packet, initialized when calling 'init'.
     */
    int frame_size;
    int frame_number;   ///< audio or video frame number

    /**
     * number of bytes per packet if constant and known or 0
     * Used by some WAV based audio codecs.
     */
    int block_align;

    /**
     * Audio cutoff bandwidth (0 means "automatic")
     * - encoding: Set by user.
     * - decoding: unused
     */
    int cutoff;

#if FF_API_REQUEST_CHANNELS
    /**
     * Decoder should decode to this many channels if it can (0 for default)
     * - encoding: unused
     * - decoding: Set by user.
     * @deprecated Deprecated in favor of request_channel_layout.
     */
    int request_channels;
#endif

    /**
     * Audio channel layout.
     * - encoding: set by user.
     * - decoding: set by libavcodec.
     */
    uint64_t channel_layout;

    /**
     * Request decoder to use this channel layout if it can (0 for default)
     * - encoding: unused
     * - decoding: Set by user.
     */
    uint64_t request_channel_layout;

    /**
     * Type of service that the audio stream conveys.
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
    enum AVAudioServiceType audio_service_type;

    /**
     * Used to request a sample format from the decoder.
     * - encoding: unused.
     * - decoding: Set by user.
     */
    enum AVSampleFormat request_sample_fmt;

    /**
     * Called at the beginning of each frame to get a buffer for it.
     *
     * The function will set AVFrame.data[], AVFrame.linesize[].
     * AVFrame.extended_data[] must also be set, but it should be the same as
     * AVFrame.data[] except for planar audio with more channels than can fit
     * in AVFrame.data[]. In that case, AVFrame.data[] shall still contain as
     * many data pointers as it can hold.
     *
     * if CODEC_CAP_DR1 is not set then get_buffer() must call
     * avcodec_default_get_buffer() instead of providing buffers allocated by
     * some other means.
     *
     * AVFrame.data[] should be 32- or 16-byte-aligned unless the CPU doesn't
     * need it. avcodec_default_get_buffer() aligns the output buffer properly,
     * but if get_buffer() is overridden then alignment considerations should
     * be taken into account.
     *
     * @see avcodec_default_get_buffer()
     *
     * Video:
     *
     * If pic.reference is set then the frame will be read later by libavcodec.
     * avcodec_align_dimensions2() should be used to find the required width and
     * height, as they normally need to be rounded up to the next multiple of 16.
     *
     * If frame multithreading is used and thread_safe_callbacks is set,
     * it may be called from a different thread, but not from more than one at
     * once. Does not need to be reentrant.
     *
     * @see release_buffer(), reget_buffer()
     * @see avcodec_align_dimensions2()
     *
     * Audio:
     *
     * Decoders request a buffer of a particular size by setting
     * AVFrame.nb_samples prior to calling get_buffer(). The decoder may,
     * however, utilize only part of the buffer by setting AVFrame.nb_samples
     * to a smaller value in the output frame.
     *
     * Decoders cannot use the buffer after returning from
     * avcodec_decode_audio4(), so they will not call release_buffer(), as it
     * is assumed to be released immediately upon return.
     *
     * As a convenience, av_samples_get_buffer_size() and
     * av_samples_fill_arrays() in libavutil may be used by custom get_buffer()
     * functions to find the required data size and to fill data pointers and
     * linesize. In AVFrame.linesize, only linesize[0] may be set for audio
     * since all planes must be the same size.
     *
     * @see av_samples_get_buffer_size(), av_samples_fill_arrays()
     *
     * - encoding: unused
     * - decoding: Set by libavcodec, user can override.
     */
    int (*get_buffer)(struct AVCodecContext *c, AVFrame *pic);

    /**
     * Called to release buffers which were allocated with get_buffer.
     * A released buffer can be reused in get_buffer().
     * pic.data[*] must be set to NULL.
     * May be called from a different thread if frame multithreading is used,
     * but not by more than one thread at once, so does not need to be reentrant.
     * - encoding: unused
     * - decoding: Set by libavcodec, user can override.
     */
    void (*release_buffer)(struct AVCodecContext *c, AVFrame *pic);

    /**
     * Called at the beginning of a frame to get cr buffer for it.
     * Buffer type (size, hints) must be the same. libavcodec won't check it.
     * libavcodec will pass previous buffer in pic, function should return
     * same buffer or new buffer with old frame "painted" into it.
     * If pic.data[0] == NULL must behave like get_buffer().
     * if CODEC_CAP_DR1 is not set then reget_buffer() must call
     * avcodec_default_reget_buffer() instead of providing buffers allocated by
     * some other means.
     * - encoding: unused
     * - decoding: Set by libavcodec, user can override.
     */
    int (*reget_buffer)(struct AVCodecContext *c, AVFrame *pic);


    /* - encoding parameters */
    float qcompress;  ///< amount of qscale change between easy & hard scenes (0.0-1.0)
    float qblur;      ///< amount of qscale smoothing over time (0.0-1.0)

    /**
     * minimum quantizer
     * - encoding: Set by user.
     * - decoding: unused
     */
    int qmin;

    /**
     * maximum quantizer
     * - encoding: Set by user.
     * - decoding: unused
     */
    int qmax;

    /**
     * maximum quantizer difference between frames
     * - encoding: Set by user.
     * - decoding: unused
     */
    int max_qdiff;

    /**
     * ratecontrol qmin qmax limiting method
     * 0-> clipping, 1-> use a nice continous function to limit qscale wthin qmin/qmax.
     * - encoding: Set by user.
     * - decoding: unused
     */
    float rc_qsquish;

    float rc_qmod_amp;
    int rc_qmod_freq;

    /**
     * decoder bitstream buffer size
     * - encoding: Set by user.
     * - decoding: unused
     */
    int rc_buffer_size;

    /**
     * ratecontrol override, see RcOverride
     * - encoding: Allocated/set/freed by user.
     * - decoding: unused
     */
    int rc_override_count;
    RcOverride *rc_override;

    /**
     * rate control equation
     * - encoding: Set by user
     * - decoding: unused
     */
    const char *rc_eq;

    /**
     * maximum bitrate
     * - encoding: Set by user.
     * - decoding: unused
     */
    int rc_max_rate;

    /**
     * minimum bitrate
     * - encoding: Set by user.
     * - decoding: unused
     */
    int rc_min_rate;

    float rc_buffer_aggressivity;

    /**
     * initial complexity for pass1 ratecontrol
     * - encoding: Set by user.
     * - decoding: unused
     */
    float rc_initial_cplx;

    /**
     * Ratecontrol attempt to use, at maximum, <value> of what can be used without an underflow.
     * - encoding: Set by user.
     * - decoding: unused.
     */
    float rc_max_available_vbv_use;

    /**
     * Ratecontrol attempt to use, at least, <value> times the amount needed to prevent a vbv overflow.
     * - encoding: Set by user.
     * - decoding: unused.
     */
    float rc_min_vbv_overflow_use;

    /**
     * Number of bits which should be loaded into the rc buffer before decoding starts.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int rc_initial_buffer_occupancy;

#define FF_CODER_TYPE_VLC       0
#define FF_CODER_TYPE_AC        1
#define FF_CODER_TYPE_RAW       2
#define FF_CODER_TYPE_RLE       3
#define FF_CODER_TYPE_DEFLATE   4
    /**
     * coder type
     * - encoding: Set by user.
     * - decoding: unused
     */
    int coder_type;

    /**
     * context model
     * - encoding: Set by user.
     * - decoding: unused
     */
    int context_model;

    /**
     * minimum Lagrange multipler
     * - encoding: Set by user.
     * - decoding: unused
     */
    int lmin;

    /**
     * maximum Lagrange multipler
     * - encoding: Set by user.
     * - decoding: unused
     */
    int lmax;

    /**
     * frame skip threshold
     * - encoding: Set by user.
     * - decoding: unused
     */
    int frame_skip_threshold;

    /**
     * frame skip factor
     * - encoding: Set by user.
     * - decoding: unused
     */
    int frame_skip_factor;

    /**
     * frame skip exponent
     * - encoding: Set by user.
     * - decoding: unused
     */
    int frame_skip_exp;

    /**
     * frame skip comparison function
     * - encoding: Set by user.
     * - decoding: unused
     */
    int frame_skip_cmp;

    /**
     * trellis RD quantization
     * - encoding: Set by user.
     * - decoding: unused
     */
    int trellis;

    /**
     * - encoding: Set by user.
     * - decoding: unused
     */
    int min_prediction_order;

    /**
     * - encoding: Set by user.
     * - decoding: unused
     */
    int max_prediction_order;

    /**
     * GOP timecode frame start number, in non drop frame format
     * - encoding: Set by user.
     * - decoding: unused
     */
    int64_t timecode_frame_start;

    /* The RTP callback: This function is called    */
    /* every time the encoder has a packet to send. */
    /* It depends on the encoder if the data starts */
    /* with a Start Code (it should). H.263 does.   */
    /* mb_nb contains the number of macroblocks     */
    /* encoded in the RTP payload.                  */
    void (*rtp_callback)(struct AVCodecContext *avctx, void *data, int size, int mb_nb);

    int rtp_payload_size;   /* The size of the RTP payload: the coder will  */
                            /* do its best to deliver a chunk with size     */
                            /* below rtp_payload_size, the chunk will start */
                            /* with a start code on some codecs like H.263. */
                            /* This doesn't take account of any particular  */
                            /* headers inside the transmitted RTP payload.  */

    /* statistics, used for 2-pass encoding */
    int mv_bits;
    int header_bits;
    int i_tex_bits;
    int p_tex_bits;
    int i_count;
    int p_count;
    int skip_count;
    int misc_bits;

    /**
     * number of bits used for the previously encoded frame
     * - encoding: Set by libavcodec.
     * - decoding: unused
     */
    int frame_bits;

    /**
     * pass1 encoding statistics output buffer
     * - encoding: Set by libavcodec.
     * - decoding: unused
     */
    char *stats_out;

    /**
     * pass2 encoding statistics input buffer
     * Concatenated stuff from stats_out of pass1 should be placed here.
     * - encoding: Allocated/set/freed by user.
     * - decoding: unused
     */
    char *stats_in;

    /**
     * Work around bugs in encoders which sometimes cannot be detected automatically.
     * - encoding: Set by user
     * - decoding: Set by user
     */
    int workaround_bugs;
#define FF_BUG_AUTODETECT       1  ///< autodetection
#define FF_BUG_OLD_MSMPEG4      2
#define FF_BUG_XVID_ILACE       4
#define FF_BUG_UMP4             8
#define FF_BUG_NO_PADDING       16
#define FF_BUG_AMV              32
#define FF_BUG_AC_VLC           0  ///< Will be removed, libavcodec can now handle these non-compliant files by default.
#define FF_BUG_QPEL_CHROMA      64
#define FF_BUG_STD_QPEL         128
#define FF_BUG_QPEL_CHROMA2     256
#define FF_BUG_DIRECT_BLOCKSIZE 512
#define FF_BUG_EDGE             1024
#define FF_BUG_HPEL_CHROMA      2048
#define FF_BUG_DC_CLIP          4096
#define FF_BUG_MS               8192 ///< Work around various bugs in Microsoft's broken decoders.
#define FF_BUG_TRUNCATED       16384

    /**
     * strictly follow the standard (MPEG4, ...).
     * - encoding: Set by user.
     * - decoding: Set by user.
     * Setting this to STRICT or higher means the encoder and decoder will
     * generally do stupid things, whereas setting it to unofficial or lower
     * will mean the encoder might produce output that is not supported by all
     * spec-compliant decoders. Decoders don't differentiate between normal,
     * unofficial and experimental (that is, they always try to decode things
     * when they can) unless they are explicitly asked to behave stupidly
     * (=strictly conform to the specs)
     */
    int strict_std_compliance;
#define FF_COMPLIANCE_VERY_STRICT   2 ///< Strictly conform to an older more strict version of the spec or reference software.
#define FF_COMPLIANCE_STRICT        1 ///< Strictly conform to all the things in the spec no matter what consequences.
#define FF_COMPLIANCE_NORMAL        0
#define FF_COMPLIANCE_UNOFFICIAL   -1 ///< Allow unofficial extensions
#define FF_COMPLIANCE_EXPERIMENTAL -2 ///< Allow nonstandardized experimental things.

    /**
     * error concealment flags
     * - encoding: unused
     * - decoding: Set by user.
     */
    int error_concealment;
#define FF_EC_GUESS_MVS   1
#define FF_EC_DEBLOCK     2

    /**
     * debug
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int debug;
#define FF_DEBUG_PICT_INFO   1
#define FF_DEBUG_RC          2
#define FF_DEBUG_BITSTREAM   4
#define FF_DEBUG_MB_TYPE     8
#define FF_DEBUG_QP          16
#define FF_DEBUG_MV          32
#define FF_DEBUG_DCT_COEFF   0x00000040
#define FF_DEBUG_SKIP        0x00000080
#define FF_DEBUG_STARTCODE   0x00000100
#define FF_DEBUG_PTS         0x00000200
#define FF_DEBUG_ER          0x00000400
#define FF_DEBUG_MMCO        0x00000800
#define FF_DEBUG_BUGS        0x00001000
#define FF_DEBUG_VIS_QP      0x00002000
#define FF_DEBUG_VIS_MB_TYPE 0x00004000
#define FF_DEBUG_BUFFERS     0x00008000
#define FF_DEBUG_THREADS     0x00010000

    /**
     * debug
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int debug_mv;
#define FF_DEBUG_VIS_MV_P_FOR  0x00000001 //visualize forward predicted MVs of P frames
#define FF_DEBUG_VIS_MV_B_FOR  0x00000002 //visualize forward predicted MVs of B frames
#define FF_DEBUG_VIS_MV_B_BACK 0x00000004 //visualize backward predicted MVs of B frames

    /**
     * Error recognition; may misdetect some more or less valid parts as errors.
     * - encoding: unused
     * - decoding: Set by user.
     */
    int err_recognition;
#define AV_EF_CRCCHECK  (1<<0)
#define AV_EF_BITSTREAM (1<<1)
#define AV_EF_BUFFER    (1<<2)
#define AV_EF_EXPLODE   (1<<3)

#define AV_EF_CAREFUL    (1<<16)
#define AV_EF_COMPLIANT  (1<<17)
#define AV_EF_AGGRESSIVE (1<<18)

    /**
     * opaque 64bit number (generally a PTS) that will be reordered and
     * output in AVFrame.reordered_opaque
     * @deprecated in favor of pkt_pts
     * - encoding: unused
     * - decoding: Set by user.
     */
    int64_t reordered_opaque;
    int64_t reordered_opaque2; /* ffdshow custom code */
    int64_t reordered_opaque3; /* ffdshow custom code */

    /**
     * Hardware accelerator in use
     * - encoding: unused.
     * - decoding: Set by libavcodec
     */
    struct AVHWAccel *hwaccel;

    /**
     * Hardware accelerator context.
     * For some hardware accelerators, a global context needs to be
     * provided by the user. In that case, this holds display-dependent
     * data Libav cannot instantiate itself. Please refer to the
     * Libav HW accelerator documentation to know how to fill this
     * is. e.g. for VA API, this is a struct vaapi_context.
     * - encoding: unused
     * - decoding: Set by user
     */
    void *hwaccel_context;

    /**
     * error
     * - encoding: Set by libavcodec if flags&CODEC_FLAG_PSNR.
     * - decoding: unused
     */
    uint64_t error[AV_NUM_DATA_POINTERS];

    /**
     * DCT algorithm, see FF_DCT_* below
     * - encoding: Set by user.
     * - decoding: unused
     */
    int dct_algo;
#define FF_DCT_AUTO    0
#define FF_DCT_FASTINT 1
#define FF_DCT_INT     2
#define FF_DCT_MMX     3
#define FF_DCT_ALTIVEC 5
#define FF_DCT_FAAN    6

    /**
     * IDCT algorithm, see FF_IDCT_* below.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int idct_algo;
/*
 * idct_algo is set by ffdshow.ax.
 * It uses the list "idctNames[]" found in Tlibavcodec.cpp.
 * The indexes of the items in that list should match with the values below.
 */
#define FF_IDCT_AUTO         0
#define FF_IDCT_LIBMPEG2MMX  1
#define FF_IDCT_SIMPLEMMX    2
#define FF_IDCT_XVIDMMX      3
#define FF_IDCT_SIMPLE       4
#define FF_IDCT_INT          5
#define FF_IDCT_FAAN         6
#define FF_IDCT_H264         7
#define FF_IDCT_VP3          8
#define FF_IDCT_CAVS         9
#define FF_IDCT_WMV2         10

    /**
     * dsp_mask could be add used to disable unwanted CPU features
     * CPU features (i.e. MMX, SSE. ...)
     *
     * With the FORCE flag you may instead enable given CPU features.
     * (Dangerous: Usable in case of misdetection, improper usage however will
     * result into program crash.)
     */
    unsigned dsp_mask;

    /**
     * bits per sample/pixel from the demuxer (needed for huffyuv).
     * - encoding: Set by libavcodec.
     * - decoding: Set by user.
     */
     int bits_per_coded_sample;

    /**
     * Bits per sample/pixel of internal libavcodec pixel/sample format.
     * - encoding: set by user.
     * - decoding: set by libavcodec.
     */
    int bits_per_raw_sample;

    /**
     * low resolution decoding, 1-> 1/2 size, 2->1/4 size
     * - encoding: unused
     * - decoding: Set by user.
     */
     int lowres;

    /**
     * the picture in the bitstream
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    AVFrame *coded_frame;

    /**
     * thread count
     * is used to decide how many independent tasks should be passed to execute()
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int thread_count;

    /**
     * Which multithreading methods to use.
     * Use of FF_THREAD_FRAME will increase decoding delay by one frame per thread,
     * so clients which cannot provide future frames should not use it.
     *
     * - encoding: Set by user, otherwise the default is used.
     * - decoding: Set by user, otherwise the default is used.
     */
    int thread_type;
#define FF_THREAD_FRAME   1 ///< Decode more than one frame at once
#define FF_THREAD_SLICE   2 ///< Decode more than one part of a single frame at once

    /**
     * Which multithreading methods are in use by the codec.
     * - encoding: Set by libavcodec.
     * - decoding: Set by libavcodec.
     */
    int active_thread_type;

    /**
     * Set by the client if its custom get_buffer() callback can be called
     * from another thread, which allows faster multithreaded decoding.
     * draw_horiz_band() will be called from other threads regardless of this setting.
     * Ignored if the default get_buffer() is used.
     * - encoding: Set by user.
     * - decoding: Set by user.
     */
    int thread_safe_callbacks;

    /**
     * The codec may call this to execute several independent things.
     * It will return only after finishing all tasks.
     * The user may replace this with some multithreaded implementation,
     * the default implementation will execute the parts serially.
     * @param count the number of things to execute
     * - encoding: Set by libavcodec, user can override.
     * - decoding: Set by libavcodec, user can override.
     */
    int (*execute)(struct AVCodecContext *c, int (*func)(struct AVCodecContext *c2, void *arg), void *arg2, int *ret, int count, int size);

    /**
     * The codec may call this to execute several independent things.
     * It will return only after finishing all tasks.
     * The user may replace this with some multithreaded implementation,
     * the default implementation will execute the parts serially.
     * Also see avcodec_thread_init and e.g. the --enable-pthread configure option.
     * @param c context passed also to func
     * @param count the number of things to execute
     * @param arg2 argument passed unchanged to func
     * @param ret return values of executed functions, must have space for "count" values. May be NULL.
     * @param func function that will be called count times, with jobnr from 0 to count-1.
     *             threadnr will be in the range 0 to c->thread_count-1 < MAX_THREADS and so that no
     *             two instances of func executing at the same time will have the same threadnr.
     * @return always 0 currently, but code should handle a future improvement where when any call to func
     *         returns < 0 no further calls to func may be done and < 0 is returned.
     * - encoding: Set by libavcodec, user can override.
     * - decoding: Set by libavcodec, user can override.
     */
    int (*execute2)(struct AVCodecContext *c, int (*func)(struct AVCodecContext *c2, void *arg, int jobnr, int threadnr), void *arg2, int *ret, int count);

    /**
     * thread opaque
     * Can be used by execute() to store some per AVCodecContext stuff.
     * - encoding: set by execute()
     * - decoding: set by execute()
     */
    void *thread_opaque;

    /**
     * noise vs. sse weight for the nsse comparsion function
     * - encoding: Set by user.
     * - decoding: unused
     */
     int nsse_weight;

    /**
     * profile
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
     int profile;
#define FF_PROFILE_UNKNOWN -99
#define FF_PROFILE_RESERVED -100

#define FF_PROFILE_AAC_MAIN 0
#define FF_PROFILE_AAC_LOW  1
#define FF_PROFILE_AAC_SSR  2
#define FF_PROFILE_AAC_LTP  3

#define FF_PROFILE_DTS         20
#define FF_PROFILE_DTS_ES      30
#define FF_PROFILE_DTS_96_24   40
#define FF_PROFILE_DTS_HD_HRA  50
#define FF_PROFILE_DTS_HD_MA   60

#define FF_PROFILE_MPEG2_422    0
#define FF_PROFILE_MPEG2_HIGH   1
#define FF_PROFILE_MPEG2_SS     2
#define FF_PROFILE_MPEG2_SNR_SCALABLE  3
#define FF_PROFILE_MPEG2_MAIN   4
#define FF_PROFILE_MPEG2_SIMPLE 5

#define FF_PROFILE_H264_CONSTRAINED  (1<<9)  // 8+1; constraint_set1_flag
#define FF_PROFILE_H264_INTRA        (1<<11) // 8+3; constraint_set3_flag

#define FF_PROFILE_H264_BASELINE             66
#define FF_PROFILE_H264_CONSTRAINED_BASELINE (66|FF_PROFILE_H264_CONSTRAINED)
#define FF_PROFILE_H264_MAIN                 77
#define FF_PROFILE_H264_EXTENDED             88
#define FF_PROFILE_H264_HIGH                 100
#define FF_PROFILE_H264_HIGH_10              110
#define FF_PROFILE_H264_HIGH_10_INTRA        (110|FF_PROFILE_H264_INTRA)
#define FF_PROFILE_H264_HIGH_422             122
#define FF_PROFILE_H264_HIGH_422_INTRA       (122|FF_PROFILE_H264_INTRA)
#define FF_PROFILE_H264_HIGH_444             144
#define FF_PROFILE_H264_HIGH_444_PREDICTIVE  244
#define FF_PROFILE_H264_HIGH_444_INTRA       (244|FF_PROFILE_H264_INTRA)
#define FF_PROFILE_H264_CAVLC_444            44

#define FF_PROFILE_VC1_SIMPLE   0
#define FF_PROFILE_VC1_MAIN     1
#define FF_PROFILE_VC1_COMPLEX  2
#define FF_PROFILE_VC1_ADVANCED 3

#define FF_PROFILE_MPEG4_SIMPLE                     0
#define FF_PROFILE_MPEG4_SIMPLE_SCALABLE            1
#define FF_PROFILE_MPEG4_CORE                       2
#define FF_PROFILE_MPEG4_MAIN                       3
#define FF_PROFILE_MPEG4_N_BIT                      4
#define FF_PROFILE_MPEG4_SCALABLE_TEXTURE           5
#define FF_PROFILE_MPEG4_SIMPLE_FACE_ANIMATION      6
#define FF_PROFILE_MPEG4_BASIC_ANIMATED_TEXTURE     7
#define FF_PROFILE_MPEG4_HYBRID                     8
#define FF_PROFILE_MPEG4_ADVANCED_REAL_TIME         9
#define FF_PROFILE_MPEG4_CORE_SCALABLE             10
#define FF_PROFILE_MPEG4_ADVANCED_CODING           11
#define FF_PROFILE_MPEG4_ADVANCED_CORE             12
#define FF_PROFILE_MPEG4_ADVANCED_SCALABLE_TEXTURE 13
#define FF_PROFILE_MPEG4_SIMPLE_STUDIO             14
#define FF_PROFILE_MPEG4_ADVANCED_SIMPLE           15

    /**
     * level
     * - encoding: Set by user.
     * - decoding: Set by libavcodec.
     */
     int level;
#define FF_LEVEL_UNKNOWN -99

    /**
     *
     * - encoding: unused
     * - decoding: Set by user.
     */
    enum AVDiscard skip_loop_filter;

    /**
     *
     * - encoding: unused
     * - decoding: Set by user.
     */
    enum AVDiscard skip_idct;

    /**
     *
     * - encoding: unused
     * - decoding: Set by user.
     */
    enum AVDiscard skip_frame;

    /**
     * Header containing style information for text subtitles.
     * For SUBTITLE_ASS subtitle type, it should contain the whole ASS
     * [Script Info] and [V4+ Styles] section, plus the [Events] line and
     * the Format line following. It shouldn't include any Dialogue line.
     * - encoding: Set/allocated/freed by user (before avcodec_open2())
     * - decoding: Set/allocated/freed by libavcodec (by avcodec_open2())
     */
    uint8_t *subtitle_header;
    int subtitle_header_size;

    /**
     * Simulates errors in the bitstream to test error concealment.
     * - encoding: Set by user.
     * - decoding: unused
     */
    int error_rate;

    /**
     * Current packet as passed into the decoder, to avoid having
     * to pass the packet into every function. Currently only valid
     * inside lavc and get/release_buffer callbacks.
     * - decoding: set by avcodec_decode_*, read by get_buffer() for setting pkt_pts
     * - encoding: unused
     */
    AVPacket *pkt;

    /**
     * VBV delay coded in the last frame (in periods of a 27 MHz clock).
     * Used for compliant TS muxing.
     * - encoding: Set by libavcodec.
     * - decoding: unused.
     */
    uint64_t vbv_delay;

/* ffdshow custom stuff (begin) */  
    /**
     * Percentage of dynamic range compression to be applied by the decoder.
     * The default value is 1.0, corresponding to full compression.
     * - encoding: unused
     * - decoding: Set by user.
     * @deprecated use AC3 decoder private option instead.
     */
    attribute_deprecated float drc_scale;

    /**
     * minimum and maxminum quantizer for I frames. If 0, derived from qmin, i_quant_factor, i_quant_offset
     * - encoding: set by user.
     * - decoding: unused
     */
    int qmin_i,qmax_i;

    /**
     * minimum and maximum quantizer for B frames. If 0, derived from qmin, b_quant_factor, b_quant_offset
     * - encoding: set by user.
     * - decoding: unused
     */
    int qmin_b,qmax_b;
    
    uint8_t ac3channels[6];
    int nal_length_size;
    int vorbis_header_size[3];
    int64_t granulepos;
    void (*handle_user_data)(struct AVCodecContext *c,const uint8_t *buf,int buf_size);
    int h264_has_to_drop_first_non_ref;    // Workaround Haali's media splitter (http://forum.doom9.org/showthread.php?p=1226434#post1226434)
    int h264_using_dxva;

     /**
     * Force 4:3 or 16:9 as DAR (MPEG-2 only)
     * - encoding: unused.
     * - decoding: Set by user.
     */
    int isDVD;
/* ffdshow custom stuff (end) */
} AVCodecContext;

/**
 * AVProfile.
 */
typedef struct AVProfile {
    int profile;
    const char *name; ///< short name for the profile
} AVProfile;

typedef struct AVCodecDefault AVCodecDefault;

/**
 * AVCodec.
 */
typedef struct AVCodec {
    /**
     * Name of the codec implementation.
     * The name is globally unique among encoders and among decoders (but an
     * encoder and a decoder can share the same name).
     * This is the primary way to find a codec from the user perspective.
     */
    const char *name;
    /**
     * Descriptive name for the codec, meant to be more human readable than name.
     * You should use the NULL_IF_CONFIG_SMALL() macro to define it.
     */
    const char *long_name;
    enum AVMediaType type;
    enum CodecID id;
    /**
     * Codec capabilities.
     * see CODEC_CAP_*
     */
    int capabilities;
    const AVRational *supported_framerates; ///< array of supported framerates, or NULL if any, array is terminated by {0,0}
    const enum PixelFormat *pix_fmts;       ///< array of supported pixel formats, or NULL if unknown, array is terminated by -1
    const int *supported_samplerates;       ///< array of supported audio samplerates, or NULL if unknown, array is terminated by 0
    const enum AVSampleFormat *sample_fmts; ///< array of supported sample formats, or NULL if unknown, array is terminated by -1
    const uint64_t *channel_layouts;         ///< array of support channel layouts, or NULL if unknown. array is terminated by 0
    uint8_t max_lowres;                     ///< maximum value for lowres supported by the decoder
    const AVClass *priv_class;              ///< AVClass for the private context
    const AVProfile *profiles;              ///< array of recognized profiles, or NULL if unknown, array is terminated by {FF_PROFILE_UNKNOWN}

    /*****************************************************************
     * No fields below this line are part of the public API. They
     * may not be used outside of libavcodec and can be changed and
     * removed at will.
     * New public fields should be added right above.
     *****************************************************************
     */
    int priv_data_size;
    struct AVCodec *next;
    /**
     * @name Frame-level threading support functions
     * @{
     */
    /**
     * If defined, called on thread contexts when they are created.
     * If the codec allocates writable tables in init(), re-allocate them here.
     * priv_data will be set to a copy of the original.
     */
    int (*init_thread_copy)(AVCodecContext *);
    /**
     * Copy necessary context variables from a previous thread context to the current one.
     * If not defined, the next thread will start automatically; otherwise, the codec
     * must call ff_thread_finish_setup().
     *
     * dst and src will (rarely) point to the same context, in which case memcpy should be skipped.
     */
    int (*update_thread_context)(AVCodecContext *dst, const AVCodecContext *src);
    /** @} */

    /**
     * Private codec-specific defaults.
     */
    const AVCodecDefault *defaults;

    /**
     * Initialize codec static data, called from avcodec_register().
     */
    void (*init_static_data)(struct AVCodec *codec);

    int (*init)(AVCodecContext *);
    int (*encode)(AVCodecContext *, uint8_t *buf, int buf_size, void *data);
    /**
     * Encode data to an AVPacket.
     *
     * @param      avctx          codec context
     * @param      avpkt          output AVPacket (may contain a user-provided buffer)
     * @param[in]  frame          AVFrame containing the raw data to be encoded
     * @param[out] got_packet_ptr encoder sets to 0 or 1 to indicate that a
     *                            non-empty packet was returned in avpkt.
     * @return 0 on success, negative error code on failure
     */
    int (*encode2)(AVCodecContext *avctx, AVPacket *avpkt, const AVFrame *frame,
                   int *got_packet_ptr);
    int (*decode)(AVCodecContext *, void *outdata, int *outdata_size, AVPacket *avpkt);
    int (*close)(AVCodecContext *);
    /**
     * Flush buffers.
     * Will be called when seeking
     */
    void (*flush)(AVCodecContext *);
} AVCodec;

/**
 * AVHWAccel.
 */
typedef struct AVHWAccel {
    /**
     * Name of the hardware accelerated codec.
     * The name is globally unique among encoders and among decoders (but an
     * encoder and a decoder can share the same name).
     */
    const char *name;

    /**
     * Type of codec implemented by the hardware accelerator.
     *
     * See AVMEDIA_TYPE_xxx
     */
    enum AVMediaType type;

    /**
     * Codec implemented by the hardware accelerator.
     *
     * See CODEC_ID_xxx
     */
    enum CodecID id;

    /**
     * Supported pixel format.
     *
     * Only hardware accelerated formats are supported here.
     */
    enum PixelFormat pix_fmt;

    /**
     * Hardware accelerated codec capabilities.
     * see FF_HWACCEL_CODEC_CAP_*
     */
    int capabilities;

    struct AVHWAccel *next;

    /**
     * Called at the beginning of each frame or field picture.
     *
     * Meaningful frame information (codec specific) is guaranteed to
     * be parsed at this point. This function is mandatory.
     *
     * Note that buf can be NULL along with buf_size set to 0.
     * Otherwise, this means the whole frame is available at this point.
     *
     * @param avctx the codec context
     * @param buf the frame data buffer base
     * @param buf_size the size of the frame in bytes
     * @return zero if successful, a negative value otherwise
     */
    int (*start_frame)(AVCodecContext *avctx, const uint8_t *buf, uint32_t buf_size);

    /**
     * Callback for each slice.
     *
     * Meaningful slice information (codec specific) is guaranteed to
     * be parsed at this point. This function is mandatory.
     *
     * @param avctx the codec context
     * @param buf the slice data buffer base
     * @param buf_size the size of the slice in bytes
     * @return zero if successful, a negative value otherwise
     */
    int (*decode_slice)(AVCodecContext *avctx, const uint8_t *buf, uint32_t buf_size);

    /**
     * Called at the end of each frame or field picture.
     *
     * The whole picture is parsed at this point and can now be sent
     * to the hardware accelerator. This function is mandatory.
     *
     * @param avctx the codec context
     * @return zero if successful, a negative value otherwise
     */
    int (*end_frame)(AVCodecContext *avctx);

    /**
     * Size of HW accelerator private data.
     *
     * Private data is allocated with av_mallocz() before
     * AVCodecContext.get_buffer() and deallocated after
     * AVCodecContext.release_buffer().
     */
    int priv_data_size;
} AVHWAccel;

/**
 * four components are given, that's all.
 * the last component is alpha
 */
typedef struct AVPicture {
    uint8_t *data[AV_NUM_DATA_POINTERS];
    int linesize[AV_NUM_DATA_POINTERS];     ///< number of bytes per line
} AVPicture;

#define AVPALETTE_SIZE 1024
#define AVPALETTE_COUNT 256

/* packet functions */

/**
 * @deprecated use NULL instead
 */
attribute_deprecated void av_destruct_packet_nofree(AVPacket *pkt);

/**
 * Default packet destructor.
 */
void av_destruct_packet(AVPacket *pkt);

/**
 * Initialize optional fields of a packet with default values.
 *
 * @param pkt packet
 */
FF_EXPORT void av_init_packet(AVPacket *pkt);

/**
 * Allocate the payload of a packet and initialize its fields with
 * default values.
 *
 * @param pkt packet
 * @param size wanted payload size
 * @return 0 if OK, AVERROR_xxx otherwise
 */
int av_new_packet(AVPacket *pkt, int size);

/**
 * Reduce packet size, correctly zeroing padding
 *
 * @param pkt packet
 * @param size new size
 */
void av_shrink_packet(AVPacket *pkt, int size);

/**
 * Increase packet size, correctly zeroing padding
 *
 * @param pkt packet
 * @param grow_by number of bytes by which to increase the size of the packet
 */
int av_grow_packet(AVPacket *pkt, int grow_by);

/**
 * @warning This is a hack - the packet memory allocation stuff is broken. The
 * packet is allocated if it was not really allocated.
 */
int av_dup_packet(AVPacket *pkt);

/**
 * Free a packet.
 *
 * @param pkt packet to free
 */
void av_free_packet(AVPacket *pkt);

/**
 * Allocate new information of a packet.
 *
 * @param pkt packet
 * @param type side information type
 * @param size side information size
 * @return pointer to fresh allocated data or NULL otherwise
 */
uint8_t* av_packet_new_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                                 int size);

/**
 * Shrink the already allocated side data buffer
 *
 * @param pkt packet
 * @param type side information type
 * @param size new side information size
 * @return 0 on success, < 0 on failure
 */
int av_packet_shrink_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                               int size);

/**
 * Get side information from packet.
 *
 * @param pkt packet
 * @param type desired side information type
 * @param size pointer for side information size to store (optional)
 * @return pointer to data if present or NULL otherwise
 */
uint8_t* av_packet_get_side_data(AVPacket *pkt, enum AVPacketSideDataType type,
                                 int *size);

void avcodec_get_chroma_sub_sample(enum PixelFormat pix_fmt, int *h_shift, int *v_shift);

void avcodec_set_dimensions(AVCodecContext *s, int width, int height);

/* external high level API */

/**
 * If c is NULL, returns the first registered codec,
 * if c is non-NULL, returns the next registered codec after c,
 * or NULL if c is the last one.
 */
AVCodec *av_codec_next(AVCodec *c);

/**
 * Return the LIBAVCODEC_VERSION_INT constant.
 */
unsigned avcodec_version(void);

/**
 * Return the libavcodec build-time configuration.
 */
const char *avcodec_configuration(void);

/**
 * Return the libavcodec license.
 */
const char *avcodec_license(void);

/**
 * Register the codec codec and initialize libavcodec.
 *
 * @warning either this function or avcodec_register_all() must be called
 * before any other libavcodec functions.
 *
 * @see avcodec_register_all()
 */
void avcodec_register(AVCodec *codec);

/**
 * Find a registered encoder with a matching codec ID.
 *
 * @param id CodecID of the requested encoder
 * @return An encoder if one was found, NULL otherwise.
 */
AVCodec *avcodec_find_encoder(enum CodecID id);

/**
 * Find a registered encoder with the specified name.
 *
 * @param name name of the requested encoder
 * @return An encoder if one was found, NULL otherwise.
 */
AVCodec *avcodec_find_encoder_by_name(const char *name);

/**
 * Find a registered decoder with a matching codec ID.
 *
 * @param id CodecID of the requested decoder
 * @return A decoder if one was found, NULL otherwise.
 */
FF_EXPORT AVCodec *avcodec_find_decoder(enum CodecID id);

/**
 * Find a registered decoder with the specified name.
 *
 * @param name name of the requested decoder
 * @return A decoder if one was found, NULL otherwise.
 */
FF_EXPORT AVCodec *avcodec_find_decoder_by_name(const char *name);
void avcodec_string(char *buf, int buf_size, AVCodecContext *enc, int encode);

/**
 * Return a name for the specified profile, if available.
 *
 * @param codec the codec that is searched for the given profile
 * @param profile the profile value for which a name is requested
 * @return A name for the profile if found, NULL otherwise.
 */
const char *av_get_profile_name(const AVCodec *codec, int profile);

/**
 * Set the fields of the given AVCodecContext to default values corresponding
 * to the given codec (defaults may be codec-dependent).
 *
 * Do not call this function if a non-NULL codec has been passed
 * to avcodec_alloc_context3() that allocated this AVCodecContext.
 * If codec is non-NULL, it is illegal to call avcodec_open2() with a
 * different codec on this AVCodecContext.
 */
int avcodec_get_context_defaults3(AVCodecContext *s, AVCodec *codec);

/**
 * Allocate an AVCodecContext and set its fields to default values.  The
 * resulting struct can be deallocated by calling avcodec_close() on it followed
 * by av_free().
 *
 * @param codec if non-NULL, allocate private data and initialize defaults
 *              for the given codec. It is illegal to then call avcodec_open2()
 *              with a different codec.
 *              If NULL, then the codec-specific defaults won't be initialized,
 *              which may result in suboptimal default settings (this is
 *              important mainly for encoders, e.g. libx264).
 *
 * @return An AVCodecContext filled with default values or NULL on failure.
 * @see avcodec_get_context_defaults
 */
FF_EXPORT AVCodecContext *avcodec_alloc_context3(AVCodec *codec);

/**
 * Set the fields of the given AVFrame to default values.
 *
 * @param pic The AVFrame of which the fields should be set to default values.
 */
void avcodec_get_frame_defaults(AVFrame *pic);

/**
 * Allocate an AVFrame and set its fields to default values.  The resulting
 * struct can be deallocated by simply calling av_free().
 *
 * @return An AVFrame filled with default values or NULL on failure.
 * @see avcodec_get_frame_defaults
 */
FF_EXPORT AVFrame *avcodec_alloc_frame(void);

FF_EXPORT int avcodec_default_get_buffer(AVCodecContext *s, AVFrame *pic);
FF_EXPORT void avcodec_default_release_buffer(AVCodecContext *s, AVFrame *pic);
FF_EXPORT int avcodec_default_reget_buffer(AVCodecContext *s, AVFrame *pic);

/**
 * Return the amount of padding in pixels which the get_buffer callback must
 * provide around the edge of the image for codecs which do not have the
 * CODEC_FLAG_EMU_EDGE flag.
 *
 * @return Required padding in pixels.
 */
unsigned avcodec_get_edge_width(void);
/**
 * Modify width and height values so that they will result in a memory
 * buffer that is acceptable for the codec if you do not use any horizontal
 * padding.
 *
 * May only be used if a codec with CODEC_CAP_DR1 has been opened.
 * If CODEC_FLAG_EMU_EDGE is not set, the dimensions must have been increased
 * according to avcodec_get_edge_width() before.
 */
void avcodec_align_dimensions(AVCodecContext *s, int *width, int *height);
/**
 * Modify width and height values so that they will result in a memory
 * buffer that is acceptable for the codec if you also ensure that all
 * line sizes are a multiple of the respective linesize_align[i].
 *
 * May only be used if a codec with CODEC_CAP_DR1 has been opened.
 * If CODEC_FLAG_EMU_EDGE is not set, the dimensions must have been increased
 * according to avcodec_get_edge_width() before.
 */
void avcodec_align_dimensions2(AVCodecContext *s, int *width, int *height,
                               int linesize_align[AV_NUM_DATA_POINTERS]);

enum PixelFormat avcodec_default_get_format(struct AVCodecContext *s, const enum PixelFormat * fmt);

int avcodec_default_execute(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2),void *arg, int *ret, int count, int size);
int avcodec_default_execute2(AVCodecContext *c, int (*func)(AVCodecContext *c2, void *arg2, int, int),void *arg, int *ret, int count);
const char* avcodec_get_current_idct(AVCodecContext *avctx);
void avcodec_get_encoder_info(AVCodecContext *avctx,int *xvid_build,int *divx_version,int *divx_build,int *lavc_build);
//FIXME func typedef

/**
 * Initialize the AVCodecContext to use the given AVCodec. Prior to using this
 * function the context has to be allocated with avcodec_alloc_context3().
 *
 * The functions avcodec_find_decoder_by_name(), avcodec_find_encoder_by_name(),
 * avcodec_find_decoder() and avcodec_find_encoder() provide an easy way for
 * retrieving a codec.
 *
 * @warning This function is not thread safe!
 *
 * @code
 * avcodec_register_all();
 * av_dict_set(&opts, "b", "2.5M", 0);
 * codec = avcodec_find_decoder(CODEC_ID_H264);
 * if (!codec)
 *     exit(1);
 *
 * context = avcodec_alloc_context3(codec);
 *
 * if (avcodec_open2(context, codec, opts) < 0)
 *     exit(1);
 * @endcode
 *
 * @param avctx The context to initialize.
 * @param codec The codec to open this context for. If a non-NULL codec has been
 *              previously passed to avcodec_alloc_context3() or
 *              avcodec_get_context_defaults3() for this context, then this
 *              parameter MUST be either NULL or equal to the previously passed
 *              codec.
 * @param options A dictionary filled with AVCodecContext and codec-private options.
 *                On return this object will be filled with options that were not found.
 *
 * @return zero on success, a negative value on error
 * @see avcodec_alloc_context3(), avcodec_find_decoder(), avcodec_find_encoder(),
 *      av_dict_set(), av_opt_find().
 */
FF_EXPORT int avcodec_open2(AVCodecContext *avctx, AVCodec *codec, AVDictionary **options);

#if FF_API_OLD_DECODE_AUDIO
/**
 * Wrapper function which calls avcodec_decode_audio4.
 *
 * @deprecated Use avcodec_decode_audio4 instead.
 *
 * Decode the audio frame of size avpkt->size from avpkt->data into samples.
 * Some decoders may support multiple frames in a single AVPacket, such
 * decoders would then just decode the first frame. In this case,
 * avcodec_decode_audio3 has to be called again with an AVPacket that contains
 * the remaining data in order to decode the second frame etc.
 * If no frame
 * could be outputted, frame_size_ptr is zero. Otherwise, it is the
 * decompressed frame size in bytes.
 *
 * @warning You must set frame_size_ptr to the allocated size of the
 * output buffer before calling avcodec_decode_audio3().
 *
 * @warning The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
 * the actual read bytes because some optimized bitstream readers read 32 or 64
 * bits at once and could read over the end.
 *
 * @warning The end of the input buffer avpkt->data should be set to 0 to ensure that
 * no overreading happens for damaged MPEG streams.
 *
 * @warning You must not provide a custom get_buffer() when using
 * avcodec_decode_audio3().  Doing so will override it with
 * avcodec_default_get_buffer.  Use avcodec_decode_audio4() instead,
 * which does allow the application to provide a custom get_buffer().
 *
 * @note You might have to align the input buffer avpkt->data and output buffer
 * samples. The alignment requirements depend on the CPU: On some CPUs it isn't
 * necessary at all, on others it won't work at all if not aligned and on others
 * it will work but it will have an impact on performance.
 *
 * In practice, avpkt->data should have 4 byte alignment at minimum and
 * samples should be 16 byte aligned unless the CPU doesn't need it
 * (AltiVec and SSE do).
 *
 * @note Codecs which have the CODEC_CAP_DELAY capability set have a delay
 * between input and output, these need to be fed with avpkt->data=NULL,
 * avpkt->size=0 at the end to return the remaining frames.
 *
 * @param avctx the codec context
 * @param[out] samples the output buffer, sample type in avctx->sample_fmt
 *                     If the sample format is planar, each channel plane will
 *                     be the same size, with no padding between channels.
 * @param[in,out] frame_size_ptr the output buffer size in bytes
 * @param[in] avpkt The input AVPacket containing the input buffer.
 *            You can create such packet with av_init_packet() and by then setting
 *            data and size, some decoders might in addition need other fields.
 *            All decoders are designed to use the least fields possible though.
 * @return On error a negative value is returned, otherwise the number of bytes
 * used or zero if no frame data was decompressed (used) from the input AVPacket.
 */
attribute_deprecated int avcodec_decode_audio3(AVCodecContext *avctx, int16_t *samples,
                         int *frame_size_ptr,
                         AVPacket *avpkt);
#endif

/**
 * Decode the audio frame of size avpkt->size from avpkt->data into frame.
 *
 * Some decoders may support multiple frames in a single AVPacket. Such
 * decoders would then just decode the first frame. In this case,
 * avcodec_decode_audio4 has to be called again with an AVPacket containing
 * the remaining data in order to decode the second frame, etc...
 * Even if no frames are returned, the packet needs to be fed to the decoder
 * with remaining data until it is completely consumed or an error occurs.
 *
 * @warning The input buffer, avpkt->data must be FF_INPUT_BUFFER_PADDING_SIZE
 *          larger than the actual read bytes because some optimized bitstream
 *          readers read 32 or 64 bits at once and could read over the end.
 *
 * @note You might have to align the input buffer. The alignment requirements
 *       depend on the CPU and the decoder.
 *
 * @param      avctx the codec context
 * @param[out] frame The AVFrame in which to store decoded audio samples.
 *                   Decoders request a buffer of a particular size by setting
 *                   AVFrame.nb_samples prior to calling get_buffer(). The
 *                   decoder may, however, only utilize part of the buffer by
 *                   setting AVFrame.nb_samples to a smaller value in the
 *                   output frame.
 * @param[out] got_frame_ptr Zero if no frame could be decoded, otherwise it is
 *                           non-zero.
 * @param[in]  avpkt The input AVPacket containing the input buffer.
 *                   At least avpkt->data and avpkt->size should be set. Some
 *                   decoders might also require additional fields to be set.
 * @return A negative error code is returned if an error occurred during
 *         decoding, otherwise the number of bytes consumed from the input
 *         AVPacket is returned.
 */
FF_EXPORT int avcodec_decode_audio4(AVCodecContext *avctx, AVFrame *frame,
                          int *got_frame_ptr, AVPacket *avpkt);

/**
 * Decode the video frame of size avpkt->size from avpkt->data into picture.
 * Some decoders may support multiple frames in a single AVPacket, such
 * decoders would then just decode the first frame.
 *
 * @warning The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
 * the actual read bytes because some optimized bitstream readers read 32 or 64
 * bits at once and could read over the end.
 *
 * @warning The end of the input buffer buf should be set to 0 to ensure that
 * no overreading happens for damaged MPEG streams.
 *
 * @note You might have to align the input buffer avpkt->data.
 * The alignment requirements depend on the CPU: on some CPUs it isn't
 * necessary at all, on others it won't work at all if not aligned and on others
 * it will work but it will have an impact on performance.
 *
 * In practice, avpkt->data should have 4 byte alignment at minimum.
 *
 * @note Codecs which have the CODEC_CAP_DELAY capability set have a delay
 * between input and output, these need to be fed with avpkt->data=NULL,
 * avpkt->size=0 at the end to return the remaining frames.
 *
 * @param avctx the codec context
 * @param[out] picture The AVFrame in which the decoded video frame will be stored.
 *             Use avcodec_alloc_frame to get an AVFrame, the codec will
 *             allocate memory for the actual bitmap.
 *             with default get/release_buffer(), the decoder frees/reuses the bitmap as it sees fit.
 *             with overridden get/release_buffer() (needs CODEC_CAP_DR1) the user decides into what buffer the decoder
 *                   decodes and the decoder tells the user once it does not need the data anymore,
 *                   the user app can at this point free/reuse/keep the memory as it sees fit.
 *
 * @param[in] avpkt The input AVpacket containing the input buffer.
 *            You can create such packet with av_init_packet() and by then setting
 *            data and size, some decoders might in addition need other fields like
 *            flags&AV_PKT_FLAG_KEY. All decoders are designed to use the least
 *            fields possible.
 * @param[in,out] got_picture_ptr Zero if no frame could be decompressed, otherwise, it is nonzero.
 * @return On error a negative value is returned, otherwise the number of bytes
 * used or zero if no frame could be decompressed.
 */
FF_EXPORT int avcodec_decode_video2(AVCodecContext *avctx, AVFrame *picture,
                         int *got_picture_ptr,
                         AVPacket *avpkt);

#if FF_API_OLD_ENCODE_AUDIO
/**
 * Encode an audio frame from samples into buf.
 *
 * @deprecated Use avcodec_encode_audio2 instead.
 *
 * @note The output buffer should be at least FF_MIN_BUFFER_SIZE bytes large.
 * However, for codecs with avctx->frame_size equal to 0 (e.g. PCM) the user
 * will know how much space is needed because it depends on the value passed
 * in buf_size as described below. In that case a lower value can be used.
 *
 * @param avctx the codec context
 * @param[out] buf the output buffer
 * @param[in] buf_size the output buffer size
 * @param[in] samples the input buffer containing the samples
 * The number of samples read from this buffer is frame_size*channels,
 * both of which are defined in avctx.
 * For codecs which have avctx->frame_size equal to 0 (e.g. PCM) the number of
 * samples read from samples is equal to:
 * buf_size * 8 / (avctx->channels * av_get_bits_per_sample(avctx->codec_id))
 * This also implies that av_get_bits_per_sample() must not return 0 for these
 * codecs.
 * @return On error a negative value is returned, on success zero or the number
 * of bytes used to encode the data read from the input buffer.
 */
int attribute_deprecated avcodec_encode_audio(AVCodecContext *avctx,
                                              uint8_t *buf, int buf_size,
                                              const short *samples);
#endif

/**
 * Encode a frame of audio.
 *
 * Takes input samples from frame and writes the next output packet, if
 * available, to avpkt. The output packet does not necessarily contain data for
 * the most recent frame, as encoders can delay, split, and combine input frames
 * internally as needed.
 *
 * @param avctx     codec context
 * @param avpkt     output AVPacket.
 *                  The user can supply an output buffer by setting
 *                  avpkt->data and avpkt->size prior to calling the
 *                  function, but if the size of the user-provided data is not
 *                  large enough, encoding will fail. All other AVPacket fields
 *                  will be reset by the encoder using av_init_packet(). If
 *                  avpkt->data is NULL, the encoder will allocate it.
 *                  The encoder will set avpkt->size to the size of the
 *                  output packet.
 * @param[in] frame AVFrame containing the raw audio data to be encoded.
 *                  May be NULL when flushing an encoder that has the
 *                  CODEC_CAP_DELAY capability set.
 *                  There are 2 codec capabilities that affect the allowed
 *                  values of frame->nb_samples.
 *                  If CODEC_CAP_SMALL_LAST_FRAME is set, then only the final
 *                  frame may be smaller than avctx->frame_size, and all other
 *                  frames must be equal to avctx->frame_size.
 *                  If CODEC_CAP_VARIABLE_FRAME_SIZE is set, then each frame
 *                  can have any number of samples.
 *                  If neither is set, frame->nb_samples must be equal to
 *                  avctx->frame_size for all frames.
 * @param[out] got_packet_ptr This field is set to 1 by libavcodec if the
 *                            output packet is non-empty, and to 0 if it is
 *                            empty. If the function returns an error, the
 *                            packet can be assumed to be invalid, and the
 *                            value of got_packet_ptr is undefined and should
 *                            not be used.
 * @return          0 on success, negative error code on failure
 */
int avcodec_encode_audio2(AVCodecContext *avctx, AVPacket *avpkt,
                          const AVFrame *frame, int *got_packet_ptr);

/**
 * Fill audio frame data and linesize.
 * AVFrame extended_data channel pointers are allocated if necessary for
 * planar audio.
 *
 * @param frame       the AVFrame
 *                    frame->nb_samples must be set prior to calling the
 *                    function. This function fills in frame->data,
 *                    frame->extended_data, frame->linesize[0].
 * @param nb_channels channel count
 * @param sample_fmt  sample format
 * @param buf         buffer to use for frame data
 * @param buf_size    size of buffer
 * @param align       plane size sample alignment
 * @return            0 on success, negative error code on failure
 */
int avcodec_fill_audio_frame(AVFrame *frame, int nb_channels,
                             enum AVSampleFormat sample_fmt, const uint8_t *buf,
                             int buf_size, int align);

#if FF_API_OLD_ENCODE_VIDEO
/**
 * @deprecated use avcodec_encode_video2() instead.
 *
 * Encode a video frame from pict into buf.
 * The input picture should be
 * stored using a specific format, namely avctx.pix_fmt.
 *
 * @param avctx the codec context
 * @param[out] buf the output buffer for the bitstream of encoded frame
 * @param[in] buf_size the size of the output buffer in bytes
 * @param[in] pict the input picture to encode
 * @return On error a negative value is returned, on success zero or the number
 * of bytes used from the output buffer.
 */
attribute_deprecated
int avcodec_encode_video(AVCodecContext *avctx, uint8_t *buf, int buf_size,
                         const AVFrame *pict);
#endif

/**
 * Encode a frame of video.
 *
 * Takes input raw video data from frame and writes the next output packet, if
 * available, to avpkt. The output packet does not necessarily contain data for
 * the most recent frame, as encoders can delay and reorder input frames
 * internally as needed.
 *
 * @param avctx     codec context
 * @param avpkt     output AVPacket.
 *                  The user can supply an output buffer by setting
 *                  avpkt->data and avpkt->size prior to calling the
 *                  function, but if the size of the user-provided data is not
 *                  large enough, encoding will fail. All other AVPacket fields
 *                  will be reset by the encoder using av_init_packet(). If
 *                  avpkt->data is NULL, the encoder will allocate it.
 *                  The encoder will set avpkt->size to the size of the
 *                  output packet. The returned data (if any) belongs to the
 *                  caller, he is responsible for freeing it.
 * @param[in] frame AVFrame containing the raw video data to be encoded.
 *                  May be NULL when flushing an encoder that has the
 *                  CODEC_CAP_DELAY capability set.
 * @param[out] got_packet_ptr This field is set to 1 by libavcodec if the
 *                            output packet is non-empty, and to 0 if it is
 *                            empty. If the function returns an error, the
 *                            packet can be assumed to be invalid, and the
 *                            value of got_packet_ptr is undefined and should
 *                            not be used.
 * @return          0 on success, negative error code on failure
 */
FF_EXPORT int avcodec_encode_video2(AVCodecContext *avctx, AVPacket *avpkt,
                          const AVFrame *frame, int *got_packet_ptr);

/**
 * Close a given AVCodecContext and free all the data associated with it
 * (but not the AVCodecContext itself).
 *
 * Calling this function on an AVCodecContext that hasn't been opened will free
 * the codec-specific data allocated in avcodec_alloc_context3() /
 * avcodec_get_context_defaults3() with a non-NULL codec. Subsequent calls will
 * do nothing.
 */
FF_EXPORT int avcodec_close(AVCodecContext *avctx);

/**
 * Register all the codecs, parsers and bitstream filters which were enabled at
 * configuration time. If you do not call this function you can select exactly
 * which formats you want to support, by using the individual registration
 * functions.
 *
 * @see avcodec_register
 * @see av_register_codec_parser
 * @see av_register_bitstream_filter
 */
FF_EXPORT void avcodec_register_all(void);

/**
 * Flush buffers, should be called when seeking or when switching to a different stream.
 */
FF_EXPORT void avcodec_flush_buffers(AVCodecContext *avctx);

void avcodec_default_free_buffers(AVCodecContext *s);

/* misc useful functions */

/**
 * Return codec bits per sample.
 *
 * @param[in] codec_id the codec
 * @return Number of bits per sample or zero if unknown for the given codec.
 */
int av_get_bits_per_sample(enum CodecID codec_id);

/* frame parsing */
typedef struct AVCodecParserContext {
    void *priv_data;
    struct AVCodecParser *parser;
    int64_t frame_offset; /* offset of the current frame */
    int64_t cur_offset; /* current offset
                           (incremented by each av_parser_parse()) */
    int64_t next_frame_offset; /* offset of the next frame */
    /* video info */
    int pict_type; /* XXX: Put it back in AVCodecContext. */
    /**
     * This field is used for proper frame duration computation in lavf.
     * It signals, how much longer the frame duration of the current frame
     * is compared to normal frame duration.
     *
     * frame_duration = (1 + repeat_pict) * time_base
     *
     * It is used by codecs like H.264 to display telecined material.
     */
    int repeat_pict; /* XXX: Put it back in AVCodecContext. */
    int64_t pts;     /* pts of the current frame */
    int64_t dts;     /* dts of the current frame */

    /* private data */
    int64_t last_pts;
    int64_t last_dts;
    int fetch_timestamp;

#define AV_PARSER_PTS_NB 4
    int cur_frame_start_index;
    int64_t cur_frame_offset[AV_PARSER_PTS_NB];
    int64_t cur_frame_pts[AV_PARSER_PTS_NB];
    int64_t cur_frame_dts[AV_PARSER_PTS_NB];

    int flags;
#define PARSER_FLAG_COMPLETE_FRAMES           0x0001
#define PARSER_FLAG_ONCE                      0x0002
/// Set if the parser has a valid file offset
#define PARSER_FLAG_FETCHED_OFFSET            0x0004

    int64_t offset;      ///< byte offset from starting packet start
    int64_t cur_frame_end[AV_PARSER_PTS_NB];

    /**
     * Set by parser to 1 for key frames and 0 for non-key frames.
     * It is initialized to -1, so if the parser doesn't set this flag,
     * old-style fallback using AV_PICTURE_TYPE_I picture type as key frames
     * will be used.
     */
    int key_frame;

    /**
     * Time difference in stream time base units from the pts of this
     * packet to the point at which the output from the decoder has converged
     * independent from the availability of previous frames. That is, the
     * frames are virtually identical no matter if decoding started from
     * the very first frame or from this keyframe.
     * Is AV_NOPTS_VALUE if unknown.
     * This field is not the display duration of the current frame.
     * This field has no meaning if the packet does not have AV_PKT_FLAG_KEY
     * set.
     *
     * The purpose of this field is to allow seeking in streams that have no
     * keyframes in the conventional sense. It corresponds to the
     * recovery point SEI in H.264 and match_time_delta in NUT. It is also
     * essential for some types of subtitle streams to ensure that all
     * subtitles are correctly displayed after seeking.
     */
    int64_t convergence_duration;

    // Timestamp generation support:
    /**
     * Synchronization point for start of timestamp generation.
     *
     * Set to >0 for sync point, 0 for no sync point and <0 for undefined
     * (default).
     *
     * For example, this corresponds to presence of H.264 buffering period
     * SEI message.
     */
    int dts_sync_point;

    /**
     * Offset of the current timestamp against last timestamp sync point in
     * units of AVCodecContext.time_base.
     *
     * Set to INT_MIN when dts_sync_point unused. Otherwise, it must
     * contain a valid timestamp offset.
     *
     * Note that the timestamp of sync point has usually a nonzero
     * dts_ref_dts_delta, which refers to the previous sync point. Offset of
     * the next frame after timestamp sync point will be usually 1.
     *
     * For example, this corresponds to H.264 cpb_removal_delay.
     */
    int dts_ref_dts_delta;

    /**
     * Presentation delay of current frame in units of AVCodecContext.time_base.
     *
     * Set to INT_MIN when dts_sync_point unused. Otherwise, it must
     * contain valid non-negative timestamp delta (presentation time of a frame
     * must not lie in the past).
     *
     * This delay represents the difference between decoding and presentation
     * time of the frame.
     *
     * For example, this corresponds to H.264 dpb_output_delay.
     */
    int pts_dts_delta;

    /**
     * Position of the packet in file.
     *
     * Analogous to cur_frame_pts/dts
     */
    int64_t cur_frame_pos[AV_PARSER_PTS_NB];

    /**
     * Byte position of currently parsed frame in stream.
     */
    int64_t pos;

    /**
     * Previous frame byte position.
     */
    int64_t last_pos;

    /**
     * Duration of the current frame.
     * For audio, this is in units of 1 / AVCodecContext.sample_rate.
     * For all other types, this is in units of AVCodecContext.time_base.
     */
    int duration;
} AVCodecParserContext;

typedef struct AVCodecParser {
    int codec_ids[5]; /* several codec IDs are permitted */
    int priv_data_size;
    int (*parser_init)(AVCodecParserContext *s);
    int (*parser_parse)(AVCodecParserContext *s,
                        AVCodecContext *avctx,
                        const uint8_t **poutbuf, int *poutbuf_size,
                        const uint8_t *buf, int buf_size);
    void (*parser_close)(AVCodecParserContext *s);
    int (*split)(AVCodecContext *avctx, const uint8_t *buf, int buf_size);
    struct AVCodecParser *next;
} AVCodecParser;

AVCodecParser *av_parser_next(AVCodecParser *c);

void av_register_codec_parser(AVCodecParser *parser);
FF_EXPORT AVCodecParserContext *av_parser_init(int codec_id);

/**
 * Parse a packet.
 *
 * @param s             parser context.
 * @param avctx         codec context.
 * @param poutbuf       set to pointer to parsed buffer or NULL if not yet finished.
 * @param poutbuf_size  set to size of parsed buffer or zero if not yet finished.
 * @param buf           input buffer.
 * @param buf_size      input length, to signal EOF, this should be 0 (so that the last frame can be output).
 * @param pts           input presentation timestamp.
 * @param dts           input decoding timestamp.
 * @param pos           input byte position in stream.
 * @return the number of bytes of the input bitstream used.
 *
 * Example:
 * @code
 *   while(in_len){
 *       len = av_parser_parse2(myparser, AVCodecContext, &data, &size,
 *                                        in_data, in_len,
 *                                        pts, dts, pos);
 *       in_data += len;
 *       in_len  -= len;
 *
 *       if(size)
 *          decode_frame(data, size);
 *   }
 * @endcode
 */
FF_EXPORT int av_parser_parse2(AVCodecParserContext *s,
                     AVCodecContext *avctx,
                     uint8_t **poutbuf, int *poutbuf_size,
                     const uint8_t *buf, int buf_size,
                     int64_t pts, int64_t dts,
                     int64_t pos);

int av_parser_change(AVCodecParserContext *s,
                     AVCodecContext *avctx,
                     uint8_t **poutbuf, int *poutbuf_size,
                     const uint8_t *buf, int buf_size, int keyframe);
FF_EXPORT void av_parser_close(AVCodecParserContext *s);

/* memory */
FF_EXPORT void av_freep(void *arg);
FF_EXPORT void av_free(void *ptr);
FF_EXPORT void *av_realloc(void *ptr, size_t size);
FF_EXPORT void *av_malloc(size_t size);
FF_EXPORT void *av_mallocz(size_t size);

/**
 * Reallocate the given block if it is not large enough, otherwise do nothing.
 *
 * @see av_realloc
 */
void *av_fast_realloc(void *ptr, unsigned int *size, size_t min_size);

/**
 * Allocate a buffer, reusing the given one if large enough.
 *
 * Contrary to av_fast_realloc the current buffer contents might not be
 * preserved and on error the old buffer is freed, thus no special
 * handling to avoid memleaks is necessary.
 *
 * @param ptr pointer to pointer to already allocated buffer, overwritten with pointer to new buffer
 * @param size size of the buffer *ptr points to
 * @param min_size minimum size of *ptr buffer after returning, *ptr will be NULL and
 *                 *size 0 if an error occurred.
 */
void av_fast_malloc(void *ptr, unsigned int *size, size_t min_size);

/**
 * Allocate a buffer with padding, reusing the given one if large enough.
 *
 * Same behaviour av_fast_malloc but the buffer has additional
 * FF_INPUT_PADDING_SIZE at the end which will always memset to 0.
 *
 */
void av_fast_padded_malloc(void *ptr, unsigned int *size, size_t min_size);

/**
 * Copy image src to dst. Wraps av_picture_data_copy() above.
 */
void av_picture_copy(AVPicture *dst, const AVPicture *src,
                     enum PixelFormat pix_fmt, int width, int height);

/**
 * Crop image top and left side.
 */
int av_picture_crop(AVPicture *dst, const AVPicture *src,
                    enum PixelFormat pix_fmt, int top_band, int left_band);

/**
 * Pad image.
 */
int av_picture_pad(AVPicture *dst, const AVPicture *src, int height, int width, enum PixelFormat pix_fmt,
            int padtop, int padbottom, int padleft, int padright, int *color);

/**
 * Encode extradata length to a buffer. Used by xiph codecs.
 *
 * @param s buffer to write to; must be at least (v/255+1) bytes long
 * @param v size of extradata in bytes
 * @return number of bytes written to the buffer.
 */
unsigned int av_xiphlacing(unsigned char *s, unsigned int v);

/**
 * Log a generic warning message about a missing feature. This function is
 * intended to be used internally by Libav (libavcodec, libavformat, etc.)
 * only, and would normally not be used by applications.
 * @param[in] avc a pointer to an arbitrary struct of which the first field is
 * a pointer to an AVClass struct
 * @param[in] feature string containing the name of the missing feature
 * @param[in] want_sample indicates if samples are wanted which exhibit this feature.
 * If want_sample is non-zero, additional verbage will be added to the log
 * message which tells the user how to report samples to the development
 * mailing list.
 */
void av_log_missing_feature(void *avc, const char *feature, int want_sample);

/**
 * Log a generic warning message asking for a sample. This function is
 * intended to be used internally by Libav (libavcodec, libavformat, etc.)
 * only, and would normally not be used by applications.
 * @param[in] avc a pointer to an arbitrary struct of which the first field is
 * a pointer to an AVClass struct
 * @param[in] msg string containing an optional message, or NULL if no message
 */
void av_log_ask_for_sample(void *avc, const char *msg, ...) av_printf_format(2, 3);

/**
 * Lock operation used by lockmgr
 */
enum AVLockOp {
  AV_LOCK_CREATE,  ///< Create a mutex
  AV_LOCK_OBTAIN,  ///< Lock the mutex
  AV_LOCK_RELEASE, ///< Unlock the mutex
  AV_LOCK_DESTROY, ///< Free mutex resources
};

/**
 * Register a user provided lock manager supporting the operations
 * specified by AVLockOp. mutex points to a (void *) where the
 * lockmgr should store/get a pointer to a user allocated mutex. It's
 * NULL upon AV_LOCK_CREATE and != NULL for all other ops.
 *
 * @param cb User defined callback. Note: Libav may invoke calls to this
 *           callback during the call to av_lockmgr_register().
 *           Thus, the application must be prepared to handle that.
 *           If cb is set to NULL the lockmgr will be unregistered.
 *           Also note that during unregistration the previously registered
 *           lockmgr callback may also be invoked.
 */
int av_lockmgr_register(int (*cb)(void **mutex, enum AVLockOp op));

/**
 * Get the type of the given codec.
 */
enum AVMediaType avcodec_get_type(enum CodecID codec_id);

/**
 * Get the AVClass for AVCodecContext. It can be used in combination with
 * AV_OPT_SEARCH_FAKE_OBJ for examining options.
 *
 * @see av_opt_find().
 */
const AVClass *avcodec_get_class(void);

/**
 * @return a positive value if s is open (i.e. avcodec_open2() was called on it
 * with no corresponding avcodec_close()), 0 otherwise.
 */
int avcodec_is_open(AVCodecContext *s);

/**
 * ffdshow custom stuff
 *
 * @param[out] recovery_frame_cnt. Valid only if GDR.
 * @return   0: no recovery point, 1:I-frame 2:Recovery Point SEI (GDR), 3:IDR, -1:error
 */
FF_EXPORT int avcodec_h264_search_recovery_point(AVCodecContext *avctx,
                         const uint8_t *buf, int buf_size, int *recovery_frame_cnt);

/* Media Player Classic - Homecinema specific functions */
FF_EXPORT int    FFGetChannelMap(struct AVCodecContext* avctx);
FF_EXPORT void*  FF_aligned_malloc(size_t size, size_t alignment);
FF_EXPORT void   FF_aligned_free(void* mem_ptr);
#endif /* AVCODEC_AVCODEC_H */

/*
 * (C) 2012-2013 see Authors.txt
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

namespace DSObjects
{
    // The list for resizers is offset by two; shaders 0 and 1 are never used
    extern char const gk_szHorizontalBlurShader[],
           gk_szVerticalBlurShader[],
           *const gk_aszResizerShader[13],
           gk_szFinalpassShader[],
           *const gk_aszBasicFrameInterpolationShader[4],
           *const gk_aszPreAdaptiveFrameInterpolationShader[3],
           *const gk_aszAdaptiveFrameInterpolationShader[4],
           gk_szSubtitlePassShader[],
           gk_szOSDPassShader[],
           gk_szInitialGammaShader[],
           gk_szRGBconvYCCShader[],
           *const gk_aszInitialPassShader[28];

    extern unsigned __int32 const gk_u32LenHorizontalBlurShader,
           gk_u32LenVerticalBlurShader,
           gk_au32LenResizerShader[13],
           gk_u32LenFinalpassShader,
           gk_au32LenBasicFrameInterpolationShader[4],
           gk_au32LenPreAdaptiveFrameInterpolationShader[3],
           gk_au32LenAdaptiveFrameInterpolationShader[4],
           gk_u32LenSubtitlePassShader,
           gk_u32LenOSDPassShader,
           gk_u32LenInitialGammaShader,
           gk_u32LenRGBconvYCCShader,
           gk_au32LenInitialPassShader[28];
}

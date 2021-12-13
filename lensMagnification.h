
/*............................................................................
 For vapoursynth
Lens Magnification creates a disc with underlying image magnified uniformly or 
varying as seen through a water drop. Requires interpolationMethods.h

 Author V.C.Mohan
 Date 13 Mar 2021
 copyright  2020

This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 3 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	A copy of the GNU General Public License is at
	see <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------------*/

#pragma once
#ifndef LENS_MAGNIFICATION_V_C_MOHAN_2021
#define LENS_MAGNIFICATION_V_C_MOHAN_2021

#include "interpolationMethods.h"

// circular area magnification by a lens. cx, cy center coordinates
template <typename finc>
void circularLensMagnification(finc** dp, const finc** sp, int np, int* pitch,
     int ht, int wd, int* subW , int* subH , finc min, finc max,
     int radius, int cx, int cy, float mag,  int span, 
     float * coeff, int quantiles, bool drop);

//.....................................................................................
template <typename finc>
void circularLensMagnification(finc** dp, const finc** sp, int np, int* pitch,
    int ht, int wd, int* subW , int* subH , finc min, finc max,
    int radius, int cx, int cy, float mag, int span,
    float* coeff, int quantiles, bool drop)
{
    int sx = VSMIN(VSMAX(cx - radius, span / 2), wd - span / 2);
    int ex = VSMIN(VSMAX(cx + radius, span / 2), wd - span / 2 );

    int sy = VSMIN(VSMAX(cy - radius, span / 2), ht - span / 2);
    int ey = VSMIN(VSMAX(cy + radius, span / 2), ht - span / 2);
    int rsq = radius * radius;
    float rmag = mag;

    int andH = (1 << subH[1]) - 1;
    int andW = (1 << subW[1]) - 1;    

    for (int h = sy; h < ey; h++)
    {
        int hsq = (cy - h) * (cy - h);

        for (int w = sx; w < ex; w++)
        {
            if (hsq + (cx - w) * (cx - w) <= rsq)
            {
                if (drop)
                {
                    // magnification decreases from center towards periphery
                    rmag = mag * (1.0f + ((hsq + (cx - w) * (cx - w)) / (float)rsq) ) / 2.0f;
                }
                float xsource = (cx - w) / rmag;
                
                int xs = (int)xsource;
                if (cx > w)
                    xs++;
                int qx = (int)(fabs(xsource - xs) * quantiles);
                int framex = cx - xs;

                float ysource = (cy - h) / rmag;
                int ys = (int)ysource;
                if (cy > h)
                    ys++;
                int qy = (int)(fabs(ysource - ys) * quantiles);
                int framey = cy - ys;

                for (int p = 0; p < np; p++)
                {
                    
                    finc* dpp = (finc*)(dp[p]); //+ sy * pitch[p];
                    const finc* spp = (finc*)(sp[p]); // +sy * pitch[p];

                    if (p == 0 || (subH[p] == 0 && subW[p] == 0))
                    {                        
                        dpp[h * pitch[p] + w]
                            = clamp(LaQuantile(spp + framey * pitch[p] + framex, pitch[p],
                                span, qx, qy, coeff), min, max);
                    }

                    else if ( p > 0 && (h & andH) == 0 && (w & andW) == 0)
                    {
                        
                        dpp[(h >> subH[p]) * pitch[p] + (w >> subW[p])] 
                            = spp[(framey >> subH[p]) * pitch[p] + (framex >> subW[p])];
                    }

                }
            }

        }
    }

    
}


#endif
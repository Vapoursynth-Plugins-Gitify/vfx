/******************************************************************************
The DiscoLights Filter for vfx plugin of vapoursynth
random colored spot lights, moves randomly or in random preset directions
as in a disco
Created on 9 Mar 2021
Author V.C.Mohan

  Copyright (C) <2021>  <V.C.Mohan>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    A copy of the GNU General Public License is at
    http://www.gnu.org/licenses/.
    For details of how to contact author see <http://www.avisynth.org/vcmohan/vcmohan.html>
*************************************************************************************************/

#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int StartFrame;
    int EndFrame;
	int life;	// no of frames spot glows
    int type;	// 1 random 2 random line 3 along axii and corner to corners
	int minrad;	// spot radius * 1, * 2, * 3 sizes    
	float dim;	// scaling down frame amplitudes by this factor  
	int nspots;	// number of spots seen at any time 
	// pointers
    uint8_t* yuvCol;		//  preset yuv color  of spot 1 to 6
	// all buffers have at random values
    int* ix;				//  x coord	
    int* iy;				//  y coord 
	int* fx;				// final x
	int* fy;				// finaly
    int* rad;				// radius of spot minrad  x 1, x 2, x 3
    int* colFlag;			// 1 to 6
	
	int nx;					//number of random values
			// odd numbers 11 to 25 will be life of directional spots
    
} DiscoLightsData;


template <typename finc>
void spotLightRGB(finc** dp, const finc** sp, int pitch, int np,
				int x, int y, int r, int wd, int ht, int *colFlags);

template <typename finc>
void spotLightYUV(finc** dp, const finc** sp, int* pitch, int np,
	int x, int y, int r, int wd, int ht, int subW, int subH,
	finc gray, int colFlag, finc* yuvCol);
//-------------------------------------------------------------------------------
static void VS_CC discolightsInit(VSMap *in, VSMap *out, void **instanceData,
	VSNode *node, VSCore *core, const VSAPI *vsapi) 
{
    DiscoLightsData *d = (DiscoLightsData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
	const VSFormat* fi = d->vi->format;
	int wd = d->vi->width;;
	int ht = d->vi->height;
	// constants
	int ncol = 6;
	int nrad = 3;
	 d->nx = 25;
	int minRad = d->minrad;
	int nxy = d->nx * d->nspots;	// each var have this many random values
	
	d->ix = (int*)vs_aligned_malloc(sizeof(int) * nxy  * 6, 32);  	
	d->iy = d->ix + nxy;		// 2
	d->fx = d->ix + 2 * nxy;		// 3
	d->fy = d->ix + 3 * nxy;		// 4
	d->rad = d->ix + 4 * nxy;		// 5
	d->colFlag = d->ix + 5 * nxy;  // 6

	srand(((d->EndFrame - d->StartFrame) * (d->EndFrame - d->StartFrame)
		* (d->EndFrame - d->StartFrame)) | 1);
	// random x, y, radius, preset color of spot	
	for (int i = 0; i < nxy; i++)
	{
		d->rad[i] = (1 + (rand() % nrad)) * minRad; // get (1,2 or 3)* minRad
		d->colFlag[i] = 1 + (rand() % ncol);	// we have 6 colors get 1 to 6

		if (d->type == 1 || d->type == 2)
		{
			d->ix[i] = rand() % wd;
			d->iy[i] = rand() % ht;
			d->fx[i] = rand() % wd;
			d->fy[i] = rand() % ht;
		}
		else if ( d->type == 3)
		{
			int temp = rand() % 8;
			switch (temp)
			{
				case 0:
					d->ix[i] = 0;
					d->iy[i] = 0;
					d->fx[i] = wd ;
					d->fy[i] = ht;					
					break;

				case 1:
					d->ix[i] = wd / 2;
					d->iy[i] = 0;
					d->fx[i] = wd / 2;
					d->fy[i] = ht ;
					break;
				case 2:
					d->ix[i] = wd;
					d->iy[i] = 0;
					d->fx[i] = 0;
					d->fy[i] = ht ;
					break;
				case 3:
					d->ix[i] = wd;
					d->iy[i] = ht / 2;
					d->fx[i] = 0;
					d->fy[i] = ht / 2;
					break;
				case 4:
					d->ix[i] = wd;
					d->iy[i] = ht;
					d->fx[i] = 0;
					d->fy[i] = 0;
					break;
				case 5:
					d->ix[i] = wd / 2;
					d->iy[i] = ht;
					d->fx[i] = wd / 2;
					d->fy[i] = 0;
					break;
				case 6:
					d->ix[i] = 0;
					d->iy[i] = ht;
					d->fx[i] = wd;
					d->fy[i] = 0;
					break;
				case 7:
					d->ix[i] = 0;
					d->iy[i] = ht/2;
					d->fx[i] = wd;
					d->fy[i] = ht/2;
					break;
			}
		}
		
		//
	}
	
	d->yuvCol = NULL;

	if (fi->colorFamily == cmYUV)
	{
		// input bgr have one or two components full saturations. Equivalent yuv are computed
		d->yuvCol = (uint8_t*)vs_aligned_malloc(sizeof(float ) * 7 * 3, 32);

		for (int i = 1; i < 7; i++)
		{
			if (fi->sampleType == stInteger && fi->bytesPerSample == 1)
			{
				uint8_t bgr[] = { 0,0,0 };
				
				bgr[0] = (i & 1) * ((1 << fi->bitsPerSample) - 1);
				bgr[1] = (i & 2) * ((1 << fi->bitsPerSample) - 1);
				bgr[2] = (i & 4) * ((1 << fi->bitsPerSample) - 1);

				BGR8YUV(d->yuvCol + 3 * i, bgr);
			}
			else if (fi->sampleType == stInteger && fi->bytesPerSample == 2)
			{
				uint16_t bgr[] = { 0,0,0 };

				bgr[0] = (uint16_t)((i & 1) * ((1 << fi->bitsPerSample) - 1));
				bgr[1] = (uint16_t)((i & 2) * ((1 << fi->bitsPerSample) - 1));
				bgr[2] = (uint16_t)((i & 4) * ((1 << fi->bitsPerSample) - 1));

				BGR16YUV((uint16_t*)d->yuvCol + 3 * i, bgr, fi->bitsPerSample);
			}

			else if (fi->sampleType == stFloat && fi->bytesPerSample == 4)
			{
				float bgr[] = { 0,0,0 };

				bgr[0] = (float)((i & 1));
				bgr[1] = (float)((i & 2));
				bgr[2] = (float)((i & 4));

				BGR32YUV((float*)d->yuvCol + 3 * i, bgr);
			}
		}
	}
	
}

//------------------------------------------------------------
template <typename finc>
void spotLightRGB(finc** dp, const finc** sp, int pitch, int np, 
			int x, int y, int r, int wd, int ht, int  colFlag)
{
	int sx = VSMIN(VSMAX(x - r, 0), wd - 1); 	
	int ex = VSMIN(VSMAX(x + r, 0), wd - 1); 	

	int sy = VSMIN(VSMAX(y - r, 0), ht - 1);	
	int ey = VSMIN(VSMAX(y + r, 0), ht - 1); 	

	
	int rsq = r * r;

	for (int h = sy; h < ey; h++)
	{
		int hsq = (h - y) * (h - y);
		
		for (int w = sx; w < ex; w++)
		{
			if (hsq + (w - x) * (w - x) <= rsq)
			{
				for (int p = 0; p < np; p++)
				{
					if (((colFlag >> p) & 1) == 1)
					{

						*(dp[p] + h * pitch + w) = *(sp[p] + h * pitch + w);
					}
								
				}
			}

		}
	}

}
//--------------------------------------------------------------------------------------------------
template <typename finc>
void spotLightYUV(finc** dp,  const finc** sp, int * pitch, int np,
	int x, int y, int r, int wd, int ht,  int subW, int subH,
	finc gray,  int colFlag, finc * yuvCol)
{
	int andH = (1 << subH) - 1;
	int andW = (1 << subW) - 1;

	int sx = VSMIN(VSMAX(x - r, 0), wd - 1); 	
	int ex = VSMIN(VSMAX(x + r, 0), wd - 1); 	

	int sy = VSMIN(VSMAX(y - r, 0), ht - 1);	
	int ey = VSMIN(VSMAX(y + r, 0), ht - 1); 	
	// initialize pointers
	
	int rsq = r * r;

	finc yuv[] = { yuvCol[3 * colFlag], yuvCol[3 * colFlag + 1], yuvCol[3 * colFlag + 2] };

	for (int h = sy; h < ey; h++)
	{
		int hsq = (h - y) * (h - y);
		
		for (int w = sx; w < ex; w++)
		{
			if (hsq + (w - x) * (w - x) <= rsq)
			{
				for (int p = 0; p < np; p++)
				{
					if (p == 0)
						*(dp[p] + h * pitch[p] +  w) = VSMIN(*(sp[p] + h * pitch[p] + w), yuv[p]);
					// u, v planes
					else if ( (h & andH) == 0 && ( w & andW) == 0)
					{
						if (*(sp[p] + (h >> subH) * pitch[p] + (w >> subW)) > gray && yuv[p] > gray)
							*(dp[p] + (h >> subH) * pitch[p] + (w >> subW)) = VSMIN(*(sp[p] + (h >> subH) * pitch[p] + (w >> subW)), yuv[p]);

						else if (*(sp[p] + (h >> subH) * pitch[p] + (w >> subW)) < gray && yuv[p] < gray)
							*(dp[p] + (h >> subH) * pitch[p] + (w >> subW)) = VSMAX(*(sp[p] + (h >> subH) * pitch[p] + (w >> subW)), yuv[p]);
						
					}
				}
			}
		}
	}
	
}


//........................................................................

static const VSFrameRef *VS_CC discolightsGetFrame(int in, int activationReason, 
	void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) 
{
    DiscoLightsData *d = (DiscoLightsData *) * instanceData;

    if (activationReason == arInitial)
	{
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    } 
	else if (activationReason == arAllFramesReady)
	{
		const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

		if (in < d->StartFrame || in > d->EndFrame)
		{
			return src;
		}

		int n = in - d->StartFrame;
		const VSFormat* fi = d->vi->format;
		int ht = d->vi->height;
		int wd = d->vi->width;

		VSFrameRef* dst = vsapi->newVideoFrame(fi, wd, ht, src, core);
		
		
		const unsigned char* sp[] = { NULL, NULL, NULL };
		unsigned char* dp[] = { NULL, NULL, NULL };
		int subH = fi->subSamplingH;
		int subW = fi->subSamplingW;
		int andH = (1 << subH) - 1;
		int andW = (1 << subW) - 1;

		int pitch[] = { 0,0,0 };
		int nbytes = fi->bytesPerSample;
		int nbits = fi->bitsPerSample;
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;		

		for (int p = 0; p < np; p++)
		{
			sp[p] = vsapi->getReadPtr(src, p);
			dp[p] = vsapi->getWritePtr(dst, p);
			pitch[p] = vsapi->getStride(dst, p) / nbytes;
		}
		// process

		if (fi->colorFamily == cmRGB)
		{
			// all 3 planes of RGB or Y of YUV
			for (int p = 0; p < np; p++)
			{
				if (nbytes == 1)
					dimplaneRGB(dp[p], sp[p], pitch[p], wd, ht, d->dim);

				else if (nbytes == 2)

					dimplaneRGB((uint16_t*)(dp[p]), (const uint16_t*)(sp[p]), pitch[p], wd, ht, d->dim);

				else if (nbytes == 4)

					dimplaneRGB((float*)(dp[p]), (const float*)(sp[p]), pitch[p], wd, ht, d->dim);
			}
		}

		else if (fi->colorFamily == cmYUV)
		{
			for (int p = 0; p < 3; p++)
			{
				int lim = p == 0 ? 16 : 128;

				if (nbytes == 1)
					dimplaneYUV(dp[p], sp[p], pitch[p], vsapi->getFrameWidth(src,p), vsapi->getFrameHeight(src, p), d->dim, (uint8_t)lim);

				else if (nbytes == 2)

					dimplaneYUV((uint16_t*)(dp[p]), (const uint16_t*)(sp[p]), pitch[p],
						vsapi->getFrameWidth(src, p), vsapi->getFrameHeight(src, p), d->dim, (uint16_t)(lim << (nbits - 8)));

				else if (nbytes == 4)

					dimplaneYUV((float*)(dp[p]), (const float*)(sp[p]), pitch[p], vsapi->getFrameWidth(src, p), vsapi->getFrameHeight(src, p), d->dim, (float)0);
			}

		}
				 
		int curFrame = n % d->life;
		int mainIndex = ( (n / d->life) % d->nx) * d->nspots;

		for (int nsp = 0; nsp < d->nspots; nsp++)
		{			
			int newIndex = mainIndex + nsp;
			int x = 0, y = 0;							
			int radius = d->rad[newIndex];
			int colorFlag = d->colFlag[newIndex];			
				
			if (d->type == 1)
			{
				x = d->ix[newIndex];
				y = d->iy[newIndex];
			}
			else if (d->type > 1)
			{
				x = d->ix[newIndex] + ((d->fx[newIndex] - d->ix[newIndex]) * curFrame ) / d->life;
				y = d->iy[newIndex] + ((d->fy[newIndex] - d->iy[newIndex]) * curFrame ) / d->life;
			}			
			
			// create spot light
			if (fi->colorFamily == cmRGB)
			{
				if (nbytes == 1)
				{
					spotLightRGB(dp, sp, pitch[0], np, x, y, radius,
						wd, ht, colorFlag);
					
				}

				else if (nbytes == 2)
				{
					spotLightRGB((uint16_t**)(dp), (const uint16_t**)(sp), pitch[0], np, x, y, radius,
						wd, ht, colorFlag);

					if (nsp == 1)
					{
						vsapi->freeFrame(src);
						return (dst);
					}
				}

				else if (nbytes == 4)
				{
					spotLightRGB((float**)(dp), (const float**)(sp), pitch[0], np, x, y, radius,
						wd, ht, colorFlag);

				}
			}

			else // yuv 
			{
				if (nbytes == 1)
				{
					uint8_t gray = (uint8_t)127;
					spotLightYUV(dp, sp, pitch, np, x, y, radius,
						wd, ht, subW, subH, gray, colorFlag, d->yuvCol);
				}

				else if (nbytes == 2)
				{
					uint16_t gray = (uint16_t)(1 << (nbits - 1));
					spotLightYUV((uint16_t**)(dp), (const uint16_t**)(sp), pitch, np, x, y, radius,
						wd, ht, subW, subH, gray, colorFlag, (uint16_t*)d->yuvCol);
				}

				else if (nbytes == 4)
				{
					float gray = 0.0;
					spotLightYUV((float**)(dp), (const float**)(sp), pitch, np, x, y, radius,
						wd, ht, subW, subH, gray, colorFlag, (float*)d->yuvCol);

				}
			}

		}					
		vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC discolightsFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    DiscoLightsData *d = (DiscoLightsData *)instanceData;
    vsapi->freeNode(d->node);
	
	vs_aligned_free(d->ix);
	if (d->yuvCol != NULL)
		vs_aligned_free(d->yuvCol);
    free(d);
}

static void VS_CC discolightsCreate(const VSMap *in, VSMap *out, 
	void *userData, VSCore *core, const VSAPI *vsapi) 
{
    DiscoLightsData d;
    DiscoLightsData *data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	
	const VSFormat* fi = d.vi->format;

	if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
		&& fi->sampleType != stInteger && fi->sampleType != stFloat && !isConstantFormat(d.vi))
	{
		vsapi->setError(out, "DiscoLights: only RGB, YUV, Gray constant formats, with integer or float samples as Input allowed ");
		vsapi->freeNode(d.node);
		return;
	}
	

	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "DiscoLights: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
	{
		vsapi->setError(out, "DiscoLights: ef must be within video and not less than sf");
		vsapi->freeNode(d.node);
		return;
	}
	d.type = int64ToIntS(vsapi->propGetInt(in, "type", 0, &err));
	if (err)
		d.type = 2;
	else if (d.type < 1 || d.type > 3)
	{
		vsapi->setError(out, "DiscoLights: type must be 1. random 2. random move 3 move in eight directions only");
		vsapi->freeNode(d.node);
		return;
	}
	d.nspots = int64ToIntS(vsapi->propGetInt(in, "nspots", 0, &err));
	if (err)
		d.nspots = 3;
	else if (d.nspots < 1 || d.nspots > 10 )
	{
		vsapi->setError(out, "DiscoLights: nspots must be 1 to 10");
		vsapi->freeNode(d.node);
		return;
	}
	d.life = int64ToIntS(vsapi->propGetInt(in, "life", 0, &err));
	if (err)
		d.life = 15;
	else if (d.life < 1 || d.life > 60)
	{
		vsapi->setError(out, "DiscoLights: life must be 1 to 60");
		vsapi->freeNode(d.node);
		return;
	}
	d.minrad = int64ToIntS(vsapi->propGetInt(in, "minrad", 0, &err));
	if (err)
		d.minrad = 32;
	else if (d.minrad < 4 || d.minrad > 128)
	{
		vsapi->setError(out, "DiscoLights: minrad must be 4 to 128");
		vsapi->freeNode(d.node);
		return;
	}
	d.dim = (float)vsapi->propGetFloat(in, "dim", 0, &err);
	if (err)
		d.dim = 0.2f;
	else if (d.dim < 0.01f || d.dim > 0.5f)
	{
		vsapi->setError(out, "DiscoLights: dim factor must be 0.01 to 0.5");
		vsapi->freeNode(d.node);
		return;
	}
	

    data = (DiscoLightsData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Filter", discolightsInit, discolightsGetFrame,
											discolightsFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.example.discolights", "discolights", "VapourSynth Filter Skeleton", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("DiscoLights", "clip:clip;sf:int:opt;ef:int:opt;life:int:opt;type:int:opt;nspots:int:opt;minrad:int:opt;dim:float:opt;", discolightsCreate, 0, plugin);
}*/

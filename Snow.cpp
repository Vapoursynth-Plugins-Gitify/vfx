/*
Snow is a function for vfx, a vapoursynth plugin
Creates a Snow on frame 


Author V.C.Mohan.
Date 31 Mar 2021
copyright  2021

This program is free software : you can redistribute it and /or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

A copy of the GNU General Public License is at
see < http://www.gnu.org/licenses/>.

---------------------------------------------------------------------------- - */
//#include "VapourSynth.h"
//#include "VSHelper.h"

typedef struct {
    VSNodeRef* node;
    const VSVideoInfo* vi;

	int StartFrame;
	int EndFrame;
	int density;	// strength of snow fall		
	int drift;		// drift pertubrance
	int fall;		// speed of vertical fall
	bool big;		// size of flake

	int* degree, * sy;
	float deltay;
	int cell;
	int cellsize;
	int nflakes;
	uint8_t col[3];

} SnowData;

template <typename finc>
void buildFlakeArm(finc* dp, int dpitch,  int d, int m1, int m2,  finc col);
template <typename finc>
void createSnowFlakes(finc* dp, int pitch, int x, bool big, finc col, int subW, int subH);
template <typename finc>
void buildFlakeArms(finc* dp, int dpitch, int d, finc col, int subW, int subH);

static void VS_CC snowInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    SnowData* d = (SnowData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

	d->cellsize = 128;
	d->nflakes = 300;


	d->deltay = (d->fall * 8.0f) / 100.0f;

	d->cell = d->cellsize / 8 + ((100 - d->density) * d->cellsize) / 100;

	d->degree = (int *)vs_aligned_malloc(sizeof(int) * 2 * d->nflakes, 32);

	d->sy = d->degree + d->nflakes;

	for (int i = 0; i < d->nflakes; i++)
	{

		d->degree[i] = rand() % 360;

		d->sy[i] = rand() % d->cell;

	}
	if (d->vi->format->colorFamily == cmRGB)
	{
		d->col[0] = (uint8_t)255;
		d->col[1] = (uint8_t)255;
		d->col[2] = (uint8_t)255;
	}
	else
	{
		d->col[0] = (uint8_t)235;
		d->col[1] = (uint8_t)127;
		d->col[2] = (uint8_t)127;
	}

}

template <typename finc>
void buildFlakeArm(finc* dp, int dpitch,  int d, int m1, int m2,  finc col)
{
	for (int m = m1; m < m2; m++)
	{
		
		*(dp + d * dpitch -  (d + m) ) = col;
		*(dp - d * dpitch -  (d + m) ) = col;

		*(dp - (d + m) * dpitch - d ) = col;
		*(dp + (d + m) * dpitch - d ) = col;

		*(dp + d * dpitch + (d + m) ) = col;
		*(dp - d * dpitch + (d + m) ) = col;

		*(dp - (d + m) * dpitch + d ) = col;
		*(dp + (d + m) * dpitch + d ) = col;
	}
}

template <typename finc>
void buildFlakeArms(finc* dp, int dpitch, int d, finc col, int subW, int subH)
{
	int dh = d >> subH, dw = d >> subW;

	for (int i = -d; i < d; i++)
	{
		int ih = i >> subH, iw = i >> subW;
		// horizontal arm
		*(dp  + iw) = col;
		// vertical arm
		*(dp + ( ih) * dpitch ) = col;
		// slant right down arms
		*(dp + ( dh + ih) * dpitch + iw) = col;
		*(dp + ih * dpitch + (dw + iw)) = col;
		// slant right up arms
		*(dp - ih * dpitch + (dw + iw)) = col;
		*(dp - (dh + ih) * dpitch +  iw) = col;
		// slant left up arms
		*(dp - (dh + ih) * dpitch - iw) = col;
		*(dp - ih * dpitch - (dw + iw)) = col;
		// slant left down arms
		*(dp + ih * dpitch - (dw + iw)) = col;
		*(dp + (dh + ih) * dpitch - iw) = col;

	}
}
template <typename finc>
void createSnowFlakes(finc* dp, int pitch, int x, bool big, finc col, int subW, int subH)
{
	if (!big || ( x % 10 ) > 6)
	{
		buildFlakeArm(dp, pitch, 0, -2, 3, col);

	}
	else
	{
		
		buildFlakeArms(dp, pitch, 4, col, subW, subH);

		
	}
}

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC snowGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SnowData* d = (SnowData*)*instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    }
	else if (activationReason == arAllFramesReady) {
		const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

		if (in < d->StartFrame || in > d->EndFrame)
			return src;
		int n = in - d->StartFrame;
		int nframes = d->EndFrame - d->StartFrame + 1;

		const VSFormat* fi = d->vi->format;
		int ht = d->vi->height;
		int wd = d->vi->width;		

		VSFrameRef* dst = vsapi->copyFrame(src, core);		
		int nbytes = fi->bytesPerSample;
		int nbits = fi->bitsPerSample;
		int nb = fi->bitsPerSample;
		int subH[] = { 0,fi->subSamplingH, fi->subSamplingH };
		int subW[] = { 0,fi->subSamplingW, fi->subSamplingW };

		int andH = ( 1 << subH[1]) - 1;
		int andW = (1 << subW[1]) - 1;
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;		

		uint8_t* dp[] = { NULL, NULL, NULL, NULL };
		//const uint8_t* sp[] = { NULL, NULL, NULL, NULL };
		int pitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			//sp[p] = vsapi->getReadPtr(src, p);
			dp[p] = vsapi->getWritePtr(dst, p);
			pitch[p] = vsapi->getStride(dst, p) / nbytes;			
		}
						// now create snow
		
		int nw = wd / d->cell > d->nflakes ? d->nflakes : wd / d->cell;
		int nh = ht / d->cell > d->nflakes ? d->nflakes : ht / d->cell;
		float fps = (float)(d->vi->fpsNum / d->vi->fpsDen);
		float yspeed = ((100 - d->fall) * fps) / 100;	// numb of frames  to fall by arbitrary choice
		int maxdx = (d->cell * d->drift) / 100;

		for (int i = 0; i < nh; i++)
		{
			for (int j = 0; j < nw + 1; j++)
			{

				float radian = (float)((d->degree[(i * nw + j) % d->nflakes])
					/ M_PI + (2.0 * n / fps));	// 2*fps is arbitrary choice

				int yy = 8 + (int)(d->cell * i + d->sy[(i * nw + j) % d->nflakes] + n * d->deltay + (rand() % 4)) % (ht - 16);	// avoid access violation. enables fold back
				int xx = 8 + (int)((d->cell * (j)) + maxdx * sin(radian) + (rand() % 4)) % (wd - 16);	// avoid access violation and enable foldback
				if (yy > 8 && xx > 8 && yy - 8 < ht && xx - 8 < wd)
				{
					for (int p = 0; p < np; p++)
					{
						if (fi->sampleType == stInteger && nbytes == 1)
						{
							unsigned char* dpp = dp[p] + (yy >> subH[p]) * pitch[p] * nbytes
								+ (xx >> subW[p]) * nbytes;
							uint8_t col = d->col[p];
							createSnowFlakes(dpp, pitch[p], xx, d->big, col, subW[p], subH[p]);
						}

						else if (fi->sampleType == stInteger && nbytes == 2)
						{
							uint16_t* dpp = (uint16_t*)(dp[p] + (yy >> subH[p]) * pitch[p] * nbytes
								+ (xx >> subW[p]) * nbytes);
							uint16_t col = d->col[p] << (nbits - 8);
							createSnowFlakes(dpp, pitch[p], xx, d->big, col, subW[p], subH[p]);
						}

						if (fi->sampleType == stFloat && nbytes == 4)
						{
							float* dpp = (float*)(dp[p] + (yy >> subH[p]) * pitch[p] * nbytes
								+ (xx >> subW[p]) * nbytes);
							float col = (d->col[p]) / 255.0f;

							if (fi->colorFamily == cmYUV)
								if (p == 0)
									col = ((d->col[p]) - 16) / 235.0f;
								else
									col = ((d->col[p]) - 128) / 235.0f;
							createSnowFlakes(dpp, pitch[p], xx, d->big, col, subW[p], subH[p]);
						}
					}
				}
			}					
		}
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC snowFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SnowData* d = (SnowData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->degree);
    free(d);
}

static void VS_CC snowCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	SnowData d;
	SnowData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Snow: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Snow: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Snow: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Snow: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}

	int temp = !!int64ToIntS(vsapi->propGetInt(in, "big", 0, &err));
	if (err)
		d.big = true;
	else if (temp == 0)
		d.big = false;
	else
		d.big = true;

	
	d.density = int64ToIntS(vsapi->propGetInt(in, "density", 0, &err));
	if (err)
		d.density = 50;
	if (d.density < 1 || d.density > 100)
	{
		vsapi->setError(out, "Snow: density %age can range from 1 to 100");
		vsapi->freeNode(d.node);
		return;
	}
	d.drift = int64ToIntS(vsapi->propGetInt(in, "drift", 0, &err));
	if (err)
		d.drift = 50;
	if (d.drift < 1 || d.drift > 100)
	{
		vsapi->setError(out, "Snow: drift %age can range from 1 to 100");
		vsapi->freeNode(d.node);
		return;
	}
	d.fall = int64ToIntS(vsapi->propGetInt(in, "fall", 0, &err));
	if (err)
		d.fall = 20;
	if (d.fall < 1 || d.fall > 100)
	{
		vsapi->setError(out, "Snow:  fall % age can range  1 to 100");
		vsapi->freeNode(d.node);
		return;
	}
	
    data = (SnowData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Snow", snowInit, snowGetFrame, snowFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Snow", "Effect snow ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Snow", "clip:clip;sf:int:opt;ef:int:opt;density:int:opt;"
				"big:int:opt;fall:int:opt;drift:int:opt;", snowCreate, 0, plugin);
	
}
*/

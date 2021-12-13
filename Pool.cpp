/*
Pool is a function for vfx, a vapoursynth plugin
Creates a Pool of water over frame or part of it
Water swish swashes


Author V.C.Mohan.
Date 18 Mar 2021
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
	int waveLength;			// 4 to 250 varies wave length 
	int samp;			// amplitude of wave at start	
	int eamp;			// amplitude of wave at end
	float sspeed;		// .0 to 100 speed of wave motion. 
	int sxcoord;		// xcoord of  center of pool	
	int sycoord;		// ycoord of  center of pool
	int swidth;			// width of pool
	int sheight;		// height of pool
	int excoord;		// end xcoord of base center of pool	
	int eycoord;		// end ycoord of base center of pool
	int ewidth;			// end width of pool
	int eheight;		// end height of pool
	float espeed;			// end speed 0 to 100	
	bool paint;			// paint effected borders?
	//int color;			// RGB color valueRRGGBB

	unsigned char bgr[3], yuv[3];

	int* sintbl; // *sinx, * siny;


} PoolData;

void poolPaintCode(PoolData* d, VSFrameRef* dst, const VSAPI* vsapi, int w, int h);

void poolPaintCode(PoolData* d, VSFrameRef* dst, const VSAPI* vsapi, int w, int h)
{
	const VSFormat* fi = d->vi->format;
	int nbytes = fi->bytesPerSample;
	int nbits = fi->bitsPerSample;
	int subH = fi->subSamplingH;
	int subW = fi->subSamplingW;

	int andH = (1 << subH) - 1;
	int andW = (1 << subW) - 1;
	int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;
	for (int p = 0; p < np; p++)
	{
		unsigned char* dp = vsapi->getWritePtr(dst, p);
		int pitch = vsapi->getStride(dst, p) / nbytes;

		if (fi->sampleType == stInteger && nbytes == 1)
		{
			if (p == 0 || (subH == 0 && subW == 0))
			{
				*(dp + h * pitch + w)
					= fi->colorFamily == cmRGB ? d->bgr[p] : d->yuv[p];
			}

			else if ((w & andW) == 0 && (h & andH) == 0)
			{
				*(dp + (h >> subH) * pitch + (w >> subW))
					= fi->colorFamily == cmRGB ? d->bgr[p] : d->yuv[p];
			}

		}
		else if (fi->sampleType == stInteger && nbytes == 2)
		{
			if (p == 0 || (subH == 0 && subW == 0))
			{
				*((uint16_t*)dp + h * pitch + w)
					= (uint16_t)(fi->colorFamily == cmRGB ? ((int)d->bgr[p]) << (nbits - 8)
						: ((int)d->yuv[p]) << (nbits - 8));
			}

			else if ((w & andW) == 0 && (h & andH) == 0)
			{
				*((uint16_t*)dp + (h >> subH) * pitch + (w >> subW))
					= (uint16_t)(fi->colorFamily == cmRGB ? ((int)d->bgr[p]) << (nbits - 8)
						: ((int)d->yuv[p]) << (nbits - 8));
			}

		}
		else if (fi->sampleType == stFloat && nbytes == 4)
		{
			if (p == 0 || (subH == 0 && subW == 0))
			{
				*((float*)dp + h * pitch + w)
					= (float)(fi->colorFamily == cmRGB ? ((float)d->bgr[p]) / 255.0f
						: ((float)d->yuv[p]) / 255.0f);
			}

			else if ((w & andW) == 0 && (h & andH) == 0)
			{
				*((float*)dp + (h >> subH) * pitch + (w >> subW))
					= (float)(fi->colorFamily == cmRGB ? ((float)d->bgr[p]) / 255.0f
						: ((float)d->yuv[p]) / 255.0f);
			}

		}
	}
}

static void VS_CC poolInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    PoolData* d = (PoolData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

	BGR8YUV(d->yuv, d->bgr);

	d->sintbl = (int *)vs_aligned_malloc(sizeof(int) * d->waveLength, 32);
	// create sine table
	for (int i = 0; i < d->waveLength; i++)

		d->sintbl[i] = (int)(256 * sin(i * 2 * M_PI / d->waveLength));
	
}
//------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC poolGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    PoolData* d = (PoolData*)*instanceData;

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

		// calculate current coordinates, width and height
		int xcoord = d->sxcoord + (n * (d->excoord - d->sxcoord)) / nframes;
		int ycoord = d->sycoord + (n * (d->eycoord - d->sycoord)) / nframes;
		int poolWidth = d->swidth + (n * (d->ewidth - d->swidth)) / nframes;
		int poolHeight = d->sheight + (n * (d->eheight - d->sheight)) / nframes;
		int ampl = d->samp + (n * (d->eamp - d->samp)) / nframes;

		float speed = d->sspeed + n * (d->espeed - d->sspeed) / nframes;
		float nn = speed * n;

		// calculate wave movement indexes

		// max value of sin =1. Therefore max possible value of following siny+sinx=waveLength should
		 // n component is to get movement effect
		int maxh = (poolHeight > poolWidth ?
			poolHeight : poolWidth);

		int * siny = (int*)vs_aligned_malloc(sizeof(int) * maxh, 32);
	

		for (int h = 0; h < maxh; h++)
		{
			siny[h] = (ampl * (20 * d->sintbl[(int)(3 * h + nn / 3) % d->waveLength]
				+ 25 * d->sintbl[(int)(2 * h + nn / 2) % d->waveLength]
				+ 38 * d->sintbl[(int)(h + nn) % d->waveLength]
				+ 25 * d->sintbl[(int)(h / 2 + 2 * nn) % d->waveLength]
				+ 20 * d->sintbl[(int)(h / 3 + 3 * nn) % d->waveLength]
				)) >> 16;	// 256 was what we multiplied in sintbl, 
					//128 is total of sin coefficients so div by (2*128*256)
		}

		int hmin = ycoord;
		int hmax = ycoord + poolHeight;
		int wmin = xcoord;
		int wmax = xcoord + poolWidth;

		VSFrameRef* dst = vsapi->copyFrame(src, core);		
		int height = vsapi->getFrameHeight(src, 0);
		int width = vsapi->getFrameWidth(src, 0);
		int nbytes = fi->bytesPerSample;
		int nbits = fi->bitsPerSample;
		int nb = fi->bitsPerSample;
		int subH[] = { 0,fi->subSamplingH, fi->subSamplingH };
		int subW[] = { 0,fi->subSamplingW, fi->subSamplingW };

		int andH = ( 1 << subH[1]) - 1;
		int andW = (1 << subW[1]) - 1;
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;		

		uint8_t* dp[] = { NULL, NULL, NULL, NULL };
		const uint8_t* sp[] = { NULL, NULL, NULL, NULL };
		int pitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			sp[p] = vsapi->getReadPtr(src, p);
			dp[p] = vsapi->getWritePtr(dst, p);
			pitch[p] = vsapi->getStride(dst, p) / nbytes;			
		}


		for (int h = hmin; h < hmax; h++)
		{
			if (h + siny[h - hmin] < hmax && h + siny[h - hmin] > hmin)
			{
				for (int w = wmin; w < wmax; w++)
				{
					if (w + siny[w - wmin] < wmax && w + siny[w - wmin] > wmin)
					{
						for ( int p = 0; p < np; p ++)
						{
							if (fi->sampleType == stInteger && nbytes == 1)
							{
								if (p == 0 || (subH[p] == 0 && subW[p] == 0))
								{
									*(dp[p] + (h)*pitch[p] + w)
										= *(sp[p] + (h + siny[h - hmin]) * pitch[p] 
											+ (w + siny[w - wmin]));
								}

								else if ((w & andW) == 0 && (h & andH) == 0)
								{
									*(dp[p] + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= *(sp[p] + ((h + siny[h - hmin]) >> subH[p]) * pitch[p]
											+ ((w + siny[w - wmin]) >> subW[p]));
								}
							}

							else if (fi->sampleType == stInteger && nbytes == 2)
							{
								if (p == 0 || (subH[p] == 0 && subW[p] == 0))
								{
									*((uint16_t*)(dp[p]) + (h)*pitch[p] + w)
										= *((const uint16_t*)(sp[p]) + (h + siny[h - hmin]) * pitch[p]
											+ (w + siny[w - wmin]));
								}

								else if ((w & andW) == 0 && (h & andH) == 0)
								{
									*((uint16_t*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= *((const uint16_t*)(sp[p]) + ((h + siny[h - hmin]) >> subH[p]) * pitch[p]
											+ ((w + siny[w - wmin]) >> subW[p]));
								}
							}

							else if (fi->sampleType == stFloat && nbytes == 4)
							{
								if (p == 0 || (subH[p] == 0 && subW[p] == 0))
								{
									*((float*)(dp[p]) + (h)*pitch[p] + w)
										= *((const float*)(sp[p]) + (h + siny[h - hmin]) * pitch[p]
											+ (w + siny[w - wmin]));
								}

								else if ((w & andW) == 0 && (h & andH) == 0)
								{
									*((float*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= *((const float*)(sp[p]) + ((h + siny[h - hmin]) >> subH[p]) * pitch[p]
											+ ((w + siny[w - wmin]) >> subW[p]));
								}
							}
						}
					}
				}
			}
		}

		if (d->paint)
		{

			xcoord = xcoord & 0xfffffffe;
			ampl = ampl & 0xfffffffc;
			width = width & 0xfffffffc;
			ycoord = ycoord & 0xfffffffe;
			height = height & 0xfffffffc;

			// paint likely affected image with color. plain drudgery.
			for (int h = hmin; h < hmin + ampl / 2; h++)
			{
				for (int w = wmin; w < wmax; w++)
				{
					poolPaintCode(d, dst, vsapi, w, h);
					
				}
			}
			for (int h = hmin; h < hmax; h++)
			{
				for (int w = wmax - ampl / 2; w < wmax; w++)
				{
					poolPaintCode(d, dst, vsapi, w, h);
					
				}
				for (int w = wmin; w < wmin + ampl / 2; w++)
				{
					poolPaintCode(d, dst, vsapi, w, h);
					
				}
			}
			for (int h = hmax - ampl / 2; h < hmax; h++)
			{
				for (int w = wmin; w < wmax; w++)
				{
					poolPaintCode(d, dst, vsapi, w, h);
					
				}
			}
		}
		vs_aligned_free (siny);
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC poolFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    PoolData* d = (PoolData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->sintbl);	
    free(d);
}

static void VS_CC poolCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	PoolData d;
	PoolData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Pool: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Pool: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Pool: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Pool: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.samp = int64ToIntS(vsapi->propGetInt(in, "amp", 0, &err));
	if (err)
		d.samp = 8;
	else if (d.samp < 4 || d.samp > 100)
	{
		vsapi->setError(out, "Pool: amp must be in range 4 to 100");
		vsapi->freeNode(d.node);
		return;
	}

	d.eamp = int64ToIntS(vsapi->propGetInt(in, "eamp", 0, &err));
	if (err)
		d.eamp = d.samp;
	else if (d.eamp < 4 || d.eamp > 100)
	{
		vsapi->setError(out, "Pool: eamp must be in range 4 to 100");
		vsapi->freeNode(d.node);
		return;
	}

	d.waveLength = int64ToIntS(vsapi->propGetInt(in, "wavelen", 0, &err));
	if (err)
		d.waveLength = 128;
	else if (d.waveLength < 4 || d.waveLength > 250)
	{
		vsapi->setError(out, "Pool: wavelen must be in range 4 to 250");
		vsapi->freeNode(d.node);
		return;
	}

	d.sxcoord = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.sxcoord = 0;
	else if (d.sxcoord < 0 || d.sxcoord > d.vi->width - 1)
	{
		vsapi->setError(out, "Pool: x must be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.excoord = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.excoord = d.sxcoord;
	else if (d.excoord < 0 || d.excoord > d.vi->width - 1)
	{
		vsapi->setError(out, "Pool: ex must be in frame");
		vsapi->freeNode(d.node);
		return;
	}

	d.sycoord = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.sycoord = 0;
	else if (d.sycoord < 0 || d.sycoord > d.vi->height - 1)
	{
		vsapi->setError(out, "Pool: y must be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.eycoord = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.eycoord = d.sycoord;
	else if (d.eycoord < 0 || d.eycoord > d.vi->height - 1)
	{
		vsapi->setError(out, "Pool: ey must be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.swidth = int64ToIntS(vsapi->propGetInt(in, "wd", 0, &err));
	if (err)
		d.swidth = d.vi->width -1 - d.sxcoord;
	else if (d.swidth < 4 || d.swidth + d.sxcoord  > d.vi->width - 1)
	{
		vsapi->setError(out, "Pool: wd must be 4 or more and x + wd be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.ewidth = int64ToIntS(vsapi->propGetInt(in, "ewd", 0, &err));
	if (err)
		d.ewidth = d.vi->width - 1 - d.excoord;
	else if (d.ewidth < 4 || d.ewidth + d.excoord > d.vi->width - 1)
	{
		vsapi->setError(out, "Pool: ew must be minimum 4 and ex + ewd must be in frame");
		vsapi->freeNode(d.node);
		return;
	}

	d.sheight = int64ToIntS(vsapi->propGetInt(in, "ht", 0, &err));
	if (err)
		d.sheight = d.vi->height - 1 - d.sycoord;
	else if (d.sheight < 4 || d.sheight + d.sycoord  > d.vi->height - 1)
	{
		vsapi->setError(out, "Pool: ht must be 4 or more and y + ht be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.eheight = int64ToIntS(vsapi->propGetInt(in, "eht", 0, &err));
	if (err)
		d.eheight = d.vi->height - 1 - d.eycoord;
	else if (d.eheight < 4 || d.eheight + d.eycoord > d.vi->width - 1)
	{
		vsapi->setError(out, "Pool: ew must be minimum 4 and ey + eht must be in frame");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.sspeed = (float)vsapi->propGetFloat(in, "speed", 0, &err);
	if (err)
		d.sspeed = 10.0f;
	else if (d.sspeed < 0.1f || d.sspeed > 100.0f)
	{
		vsapi->setError(out, "Pool: speed can be 0.1 to 100.0 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.espeed = (float)vsapi->propGetFloat(in, "espeed", 0, &err);
	if (err)
		d.espeed = d.sspeed;
	else if (d.espeed < 0.1f || d.espeed > 100.0f)
	{
		vsapi->setError(out, "Pool: espeed can be 0.1 to 100.0 only");
		vsapi->freeNode(d.node);
		return;
	}
	
	
	int temp = !!int64ToIntS(vsapi->propGetInt(in, "paint", 0, &err));
	if (err)
		d.paint = true;
	else if (temp == 0)
		d.paint = false;
	else
		d.paint = true;

	if (d.paint)
	{
		temp = vsapi->propNumElements(in, "color");
		if (temp == 0)
		{
			d.bgr[0] = 150;
			d.bgr[1] = 55;
			d.bgr[2] = 0;
		}

		else if (temp < 4)
		{
			for (int i = 0; i < temp; i++)
			{
				d.bgr[2 - i] = int64ToIntS(vsapi->propGetInt(in, "color", i, &err));
				if (d.bgr[2 - i] < 0 || d.bgr[2 - i] > 255)
				{
					vsapi->setError(out, "Pool: rise and x values must ensure vflower pot is visible");
					vsapi->freeNode(d.node);
					return;
				}
			}

			for (int i = temp; i < 3; i++)
			{
				d.bgr[2 - i] = d.bgr[2 - i - 1];
			}
		}

		else
		{			
			vsapi->setError(out, "Pool: color array in R G B order can have not more than 3 values  ");
			vsapi->freeNode(d.node);
			return;
			
		}

	}

	
    data = (PoolData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Pool", poolInit, poolGetFrame, poolFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Pool", "Effect pool ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Pool", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;ex:int:opt;"
	"ey:int:opt;wd:int:opt;ewd:int:opt;ht:int:opt;eht:int:opt;wavelen:int:opt;"
	"amp:int:opt;eamp:int:opt;speed:float:opt;espeed:float:opt;"
	"paint:int:opt;color:int[]:opt;", poolCreate, 0, plugin);
	
}
*/

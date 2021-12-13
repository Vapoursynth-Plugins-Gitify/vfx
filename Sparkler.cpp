/*
Sparkler is a function for vfx, a vapoursynth plugin
Creates a Sparkler Firework on frame 


Author V.C.Mohan.
Date 2 April 2021
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
	int startx, starty, endx, endy;
	int radius, span;
	bool color;	
	uint8_t* allCol;
	
	int* xy;	// beam ends and flower locations
	int nbeams;
	

} SparklerData;



static void VS_CC sparklerInit(VSMap* in, VSMap* out, void** instanceData,
	VSNode* node, VSCore* core, const VSAPI* vsapi) {
	SparklerData* d = (SparklerData*)*instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);
	const VSFormat* fi = d->vi->format;

	d->nbeams = 64;
	d->xy = (int*)vs_aligned_malloc(sizeof(int) * d->nbeams, 32);

	for (int i = 0; i < d->nbeams; i++)
	{
		d->xy[i] = (rand() % (3 * d->radius) / 4) + d->radius / 4;
		if ((i % 4) > 1) d->xy[i] = -d->xy[i];
	}
	d->span = d->radius / 4;
	

	d->allCol = (uint8_t*)vs_aligned_malloc(sizeof(float) * 9 * 3, 32);
	uint8_t* yuvCol = d->allCol + sizeof(float) * 8 * 3;

	// 0 th will be black. 7th will be white. in between all colors
	for (int i = 0; i < 8; i++)
	{
		if (fi->sampleType == stInteger && fi->bytesPerSample == 1)
		{
			uint8_t* bgr = (uint8_t*)d->allCol;
			uint8_t* yuv = (uint8_t*)yuvCol;
			bgr[3 * i] = (i & 1) * ((1 << fi->bitsPerSample) - 1);
			bgr[3 * i + 1] = (i & 2) * ((1 << fi->bitsPerSample) - 1);
			bgr[3 * i + 2] = (i & 4) * ((1 << fi->bitsPerSample) - 1);
			if (fi->colorFamily == cmYUV)
			{
				BGR8YUV(yuv, bgr + 3 * i);

				bgr[3 * i] = yuv[0];
				bgr[3 * i + 1] = yuv[1];
				bgr[3 * i + 2] = yuv[2];
			}
		}
		else if (fi->sampleType == stInteger && fi->bytesPerSample == 2)
		{
			uint16_t* bgr = (uint16_t*)d->allCol;
			uint16_t* yuv = (uint16_t*)yuvCol;

			bgr[3 * i + 0] = (uint16_t)((i & 1) * ((1 << fi->bitsPerSample) - 1));
			bgr[3 * i + 1] = (uint16_t)((i & 2) * ((1 << fi->bitsPerSample) - 1));
			bgr[3 * i + 2] = (uint16_t)((i & 4) * ((1 << fi->bitsPerSample) - 1));

			if (fi->colorFamily == cmYUV)
			{
				BGR16YUV(yuv, bgr + 3 * i, fi->bitsPerSample);

				bgr[3 * i] = yuv[0];
				bgr[3 * i + 1] = yuv[1];
				bgr[3 * i + 2] = yuv[2];
			}
		}

		else if (fi->sampleType == stFloat && fi->bytesPerSample == 4)
		{
			float* bgr = (float*)d->allCol;;
			float* yuv = (float*)yuvCol;

			bgr[3 * i + 0] = (float)((i & 1));
			bgr[3 * i + 1] = (float)((i & 2));
			bgr[3 * i + 2] = (float)((i & 4));

			if (fi->colorFamily == cmYUV)
			{
				BGR32YUV(yuv, bgr + 3 * i);

				bgr[3 * i] = yuv[0];
				bgr[3 * i + 1] = yuv[1];
				bgr[3 * i + 2] = yuv[2];
			}
		}
	}
}

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC sparklerGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SparklerData* d = (SparklerData*)*instanceData;

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

						// now create sparkler
		
		int xx = d->startx + (n * (d->endx - d->startx)) / nframes;
		int yy = d->starty + (n * (d->endy - d->starty)) / nframes;
		int rayxy[8];	// 3 beams, 1 flower

		for (int i = 0; i < 8; i++)
		{
			rayxy[i] = d->xy[( (n / 3) + i) % d->nbeams] * (4 - i / 2) / 4;
		}
		//srand((2 << 16) - 1);
		// flower color
		int colIndex = 1 + (n / 3) % 6;
		
		if (!d->color)
		{
			colIndex = 7;
		}
		
		if (n % 3 != 0)
		{
			for (int p = 0; p < np; p++)
			{
				if (fi->sampleType == stInteger && nbytes == 1)
				{
					unsigned char* dpp = dp[p];
					uint8_t colf = d->allCol[3 * colIndex + p];
					
					for (int i = 0; i < 4; i++)
					{
						drawRay((dpp), pitch[p], colf, xx >> subW[p], yy >> subH[p],
							rayxy[2 * i] >> subW[p], rayxy[2 * i + 1] >> subH[p],
							wd >> subW[p], ht >> subH[p]);

						drawFlower((dpp), pitch[p], colf,
							(xx + rayxy[i + 1]) >> subW[p], (yy + rayxy[2 * i ]) >> subH[p],
							d->span >> subW[p], d->span >> subH[p], wd >> subW[p], ht >> subH[p]);
					}
				}

				else if (fi->sampleType == stInteger && nbytes == 2)
				{
					uint16_t* dpp = (uint16_t*)(dp[p]);
					uint16_t* color = (uint16_t*)d->allCol;
					uint16_t colf = color[3 * colIndex + p];

					for (int i = 0; i < 4; i++)
					{
						drawRay((dpp), pitch[p], colf, xx >> subW[p], yy >> subH[p],
							rayxy[2 * i] >> subW[p], rayxy[2 * i + 1] >> subH[p],
							wd >> subW[p], ht >> subH[p]);

						drawFlower((dpp), pitch[p], colf,
							(xx + rayxy[i + 1]) >> subW[p], (yy + rayxy[2 * i]) >> subH[p],
							d->span >> subW[p], d->span >> subH[p], wd >> subW[p], ht >> subH[p]);
					}


				}

				if (fi->sampleType == stFloat && nbytes == 4)
				{
					float* dpp = (float*)(dp[p]);
					float* color = (float*)d->allCol;
					float colf = (color[3 * colIndex + p]);

					for (int i = 0; i < 4; i++)
					{
						drawRay((dpp), pitch[p], colf, xx >> subW[p], yy >> subH[p],
							rayxy[2 * i] >> subW[p], rayxy[2 * i + 1] >> subH[p],
							wd >> subW[p], ht >> subH[p]);

						drawFlower((dpp), pitch[p], colf,
							(xx + rayxy[i + 1]) >> subW[p], (yy + rayxy[2 * i]) >> subH[p],
							d->span >> subW[p], d->span >> subH[p], wd >> subW[p], ht >> subH[p]);
					}
				}
			}
		}
		
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC sparklerFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SparklerData* d = (SparklerData*)instanceData;
    vsapi->freeNode(d->node);	
	
    free(d);
}

static void VS_CC sparklerCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	SparklerData d;
	SparklerData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Sparkler: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Sparkler: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Sparkler: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Sparkler: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
		
	d.radius = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.radius = 64 > d.vi->width / 8 ? d.vi->width / 8: 64;
	else if (d.radius < 4 || d.radius > d.vi->width / 4)
	{
		vsapi->setError(out, "Sparkler:  rad can be 4 to frame width / 4");
		vsapi->freeNode(d.node);
		return;
	}
	d.startx = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.startx = d.vi->width / 2; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.startx < 0 || d.startx > d.vi->width )
	{
		vsapi->setError(out, "Sparkler:  x can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.starty = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.starty = d.vi->height / 2;
	else if (d.starty < 0 || d.starty > d.vi->height)
	{
		vsapi->setError(out, "Sparkler:  y can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}

	d.endx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.endx = d.startx; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.endx < 0 || d.endx > d.vi->width)
	{
		vsapi->setError(out, "Sparkler:  ex can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.endy = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.endy = d.starty; //
	else if (d.endy < 0 || d.endy > d.vi->height)
	{
		vsapi->setError(out, "Sparkler:  ey can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}
	int temp = !!int64ToIntS(vsapi->propGetInt(in, "color", 0, &err));
	if (err)
		d.color = true;
	else if (temp == 0)
		d.color = false;
	else
		d.color = true;

    data = (SparklerData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Sparkler", sparklerInit, sparklerGetFrame, sparklerFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Sparkler", "Effect sparkler ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Sparkler", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;"
	"x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;color:int:opt;", sparklerCreate, 0, plugin);
	
}
*/

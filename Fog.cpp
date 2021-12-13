/*
Fog is a function for vfx, a vapoursynth plugin
Creates a Fog firework 


Author V.C.Mohan.
Date 16 Mar 2021
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
	float fog;	// start  greyness
	float efog;		// end  value
	float vary;	// variation
} FogData;


static void VS_CC fogInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
    FogData* d = (FogData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
	
}
//------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC fogGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    FogData* d = (FogData*)*instanceData;

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
		// fog values
		int maxval = 180; // fi->colorFamily == cmRGB ? 255 : 235;
		int shade = (int)(((d->fog + ((d->efog - d->fog) * n) / nframes) * maxval));
		int variation = (int)(d->vary * shade);

		VSFrameRef* dst = vsapi->copyFrame(src, core);		
		
		int nbytes = fi->bytesPerSample;
		int nbits = fi->bitsPerSample;
		int nb = fi->bitsPerSample;
		int subH[] = { 0,fi->subSamplingH, fi->subSamplingH };
		int subW[] = { 0,fi->subSamplingW, fi->subSamplingW };

		int andH = ( 1 << subH[1]) - 1;
		int andW = (1 << subW[1]) - 1;
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;		

		//uint8_t* dpt[] = { NULL, NULL, NULL, NULL };
		//int dpitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			uint8_t* dp = vsapi->getWritePtr(dst, p);
			int dpitch = vsapi->getStride(dst, p);
			int ht = vsapi->getFrameHeight(dst, p);
			int wd = vsapi->getFrameWidth(dst, p);

			for (int h = 0; h < ht; h++)
			{
				for (int w = 0; w < wd; w++)
				{
					if (fi->sampleType == stInteger && nbytes == 1)
					{
						if (fi->colorFamily == cmRGB || p == 0)
						{
							uint8_t fog = (uint8_t)(shade - variation / 2 +  (rand() % variation));

							if (*(dp + w) <= fog)
							{
								*(dp + w) = fog;
							}
							else
							{
								*(dp + w) = (*(dp + w) + fog) / 2;
							}
						}
						else // yuv
						{
							uint8_t gray = 1 << (nbits - 1);

							*(dp + w) = (*(dp + w) + gray) / 2;
						}

					}

					else if (fi->sampleType == stInteger && nbytes == 2)
					{
						if (fi->colorFamily == cmRGB || p == 0)
						{
							uint16_t fog = (uint16_t) ((shade - variation / 2 + (rand() % variation)) << (nbits - 8));

							if (*((uint16_t *)dp + w) <= fog)
							{
								*((uint16_t*)dp + w) = fog;
							}
							else
							{
								*((uint16_t*)dp + w) = (*((uint16_t*)dp + w) + fog) / 2;
							}
						}
						else // yuv
						{
							uint16_t gray = (uint16_t)( 1 << (nbits - 1));

							*((uint16_t*)dp + w) = (*((uint16_t*)dp + w) + gray) / 2;
						}

					}

					else if (fi->sampleType == stFloat && nbytes == 4)
					{
						if (fi->colorFamily == cmRGB || p == 0)
						{

							float fog = (float)(shade - variation / 2 + (rand() % variation))/ 255.0f;

							if (*( (float*)dp + w) <= fog)
							{
								*((float*)dp + w) = fog;
							}
							else
							{
								*((float*)dp + w) = (*((float*)dp + w) + fog) / 2;
							}
						}
						else // yuv
						{
							//(float)gray = 0.0f;

							*((float*)dp + w) = (*((float*)dp + w) ) / 2;
						}

					}
				}

				dp += dpitch;
			}
		}
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC fogFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    FogData* d = (FogData*)instanceData;
    vsapi->freeNode(d->node);	
	
    free(d);
}

static void VS_CC fogCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	FogData d;
	FogData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Fog: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Fog: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Fog: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Fog: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.fog = (float)vsapi->propGetFloat(in, "fog", 0, &err);
	if (err)
		d.fog = 0.5f;
	else if (d.fog < 0 || d.fog > 0.9f)
	{
		vsapi->setError(out, "Fog: fog can be 0 to 0.9 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.efog = (float)vsapi->propGetFloat(in, "efog", 0, &err);
	if (err)
		d.efog = d.fog;
	else if (d.efog < 0 || d.efog > 0.9f)
	{
		vsapi->setError(out, "Fog: efog can be 0 to 0.9 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.vary = (float)vsapi->propGetFloat(in, "vary", 0, &err);
	if (err)
		d.vary =  d.fog * 0.1f;
	if (d.vary < 0 || d.fog  + d.fog * d.vary > 1.0f  || d.efog  + d.efog * d.vary > 1.0f)
	{
		vsapi->setError(out, "Fog: total of fog and its variation should not exceed 1.0");
		vsapi->freeNode(d.node);
		return;
	}

	
    data = (FogData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Fog", fogInit, fogGetFrame, fogFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Fog", "Effect fog ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Fog", "clip:clip;sf:int:opt;ef:int:opt;fog:float:opt;efog:float:opt;"
	"vary:float:opt;", fogCreate, 0, plugin);
	
}
*/

/*
SnowStorm is a function for vfx, a vapoursynth plugin
Creates a SnowStorm on frame 


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
	int type[2];
	uint8_t col[3];

} SnowStormData;
static void VS_CC snowstormInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    SnowStormData* d = (SnowStormData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

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

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC snowstormGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SnowStormData* d = (SnowStormData*)*instanceData;

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
						// now create snowstorm
		
		int maxnum = (ht * wd) >> 3;
		int minnum = maxnum >> 4;
		int numbers = 0; 
		//shade = heavy;
		if (d->type[0] == 1)		// light
			numbers = minnum;
		else if (d->type[0] == 2)		// medium
			numbers = maxnum >> 2;
		else //was heavy
			numbers = maxnum;

		if (d->type[1] == 1)
			// incresing
			numbers = numbers + (n * (maxnum - numbers)) / nframes;
			// decreasing
		else if (d->type[1] == 3)
			numbers = numbers + (n * (minnum - numbers)) / nframes;
		// else if d->type[1] == 2 constant

		srand((2 << 16) - 1);

		for (int i = 0; i < numbers; i++)
		{
			// n is to give continuous motion illusion.
			// as each frame we are initializing rand, random numbers would be identical
			int yoffset = (n + rand()) % (ht - 1);
			int xoffset = (n + rand()) % (wd - 1);

			for (int p = 0; p < np; p++)
			{
				if (fi->sampleType == stInteger && nbytes == 1)
				{
					unsigned char* dpp = dp[p];
					uint8_t col = d->col[p];

					*(dpp + (yoffset >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + ((yoffset + 1) >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + (yoffset >> subH[p]) * pitch[p] + ((xoffset + 1) >> subW[p])) = col;

				}

				else if (fi->sampleType == stInteger && nbytes == 2)
				{
					uint16_t* dpp = (uint16_t*)(dp[p]);
					uint16_t col = d->col[p] << (nbits - 8);

					*(dpp + (yoffset >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + ((yoffset + 1) >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + (yoffset >> subH[p]) * pitch[p] + ((xoffset + 1) >> subW[p])) = col;

				}

				if (fi->sampleType == stFloat && nbytes == 4)
				{
					float* dpp = (float*)(dp[p]);
					float col = (d->col[p]) / 255.0f;

					if (fi->colorFamily == cmYUV)
						if (p == 0)
							col = ((d->col[p]) - 16) / 235.0f;
						else
							col = ((d->col[p]) - 128) / 235.0f;

					*(dpp + (yoffset >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + ((yoffset + 1) >> subH[p]) * pitch[p] + (xoffset >> subW[p])) = col;
					*(dpp + (yoffset >> subH[p]) * pitch[p] + ((xoffset + 1) >> subW[p])) = col;

				}

			}
		}		
		
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC snowstormFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SnowStormData* d = (SnowStormData*)instanceData;
    vsapi->freeNode(d->node);	
	
    free(d);
}

static void VS_CC snowstormCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	SnowStormData d;
	SnowStormData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "SnowStorm: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "SnowStorm: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "SnowStorm: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "SnowStorm: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}

	int temp = vsapi->propNumElements(in, "type");
	if (temp == 0)
	{
		d.type[0] = 1;
		d.type[1] = 1;
	}
	else if (temp <= 2)
	{
		for (int i = 0; i < temp; i++)
		{
			d.type[i] = int64ToIntS(vsapi->propGetInt(in, "type", i, &err));
			if (d.type[i] < 1 || d.type[i] > 3)
			{
				vsapi->setError(out, "SnowStorm:  type array can have values of 1, 2 or 3 only ");
				vsapi->freeNode(d.node);
				return;
			}
		}
		if (temp == 1)
			d.type[1] = 2;
	}
	else
	{
		vsapi->setError(out, "SnowStorm: type array consists of two values only");
		vsapi->freeNode(d.node);
		return;
	}

	if ( (d.type[0] == 1 && d.type[1] == 3 ) || (d.type[0] == 3 && d.type[1] == 1))
	{
		vsapi->setError(out, "SnowStorm: type array values are inconsistant. light snow can not decrease nor heavy snow can further increase");
		vsapi->freeNode(d.node);
		return;
	}

	
    data = (SnowStormData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "SnowStorm", snowstormInit, snowstormGetFrame, snowstormFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "SnowStorm", "Effect snowstorm ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("SnowStorm", "clip:clip;sf:int:opt;ef:int:opt;type:int[]:opt;", snowstormCreate, 0, plugin);
	
}
*/

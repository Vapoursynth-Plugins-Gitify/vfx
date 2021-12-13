/*
Rockets is a function for vfx, a vapoursynth plugin
Creates a Rockets on frame 


Author V.C.Mohan.
Date 30 Mar 2021
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
	int leftx;
	int inity;
	int rise;
	int life;
	int interval;
	int rightx;
	bool target;
	int targetx, targety;

	int nrockets;
	int* px, * py, * xcoord;
	uint8_t red[3];
	uint8_t white[3];

} RocketsData;



static void VS_CC rocketsInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    RocketsData* d = (RocketsData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

	d->nrockets = d->life / d->interval + 1;
	if (d->nrockets < 100)
		d->nrockets = 100;
	d->red[0] = 0;
	d->red[1] = 0;
	d->red[2] = 255;
	d->white[0] = 255;
	d->white[1] = 255;
	d->white[2] = 255;

	if (d->vi->format->colorFamily == cmYUV )
	{
		//convert and transfer to array red 
		uint8_t yuv[] = { 0,0,0 };
		BGR8YUV(yuv, d->red);
		for (int i = 0; i < 3; i++)
		{
			d->red[i] = yuv[i];
		}
		BGR8YUV(yuv, d->white);
		for (int i = 0; i < 3; i++)
		{
			d->white[i] = yuv[i];
		}
	}
	else // again nonsense looks internally its RGB not BGR as in documentation
	{
		d->red[0] = 255;
		d->red[1] = 0;
		d->red[2] = 0;
	}
	
	d->px = (int*)vs_aligned_malloc(sizeof(int) * 3 * d->nrockets, 32);
	d->py = d->px + d->nrockets;
	d->xcoord = d->py + d->nrockets;

	if (d->target)
	{
		d->rise = d->inity - d->targety;		
	}
	//	auto start = (std::chrono::system_clock::now()) % RAND_MAX;
	srand(((d->EndFrame - d->StartFrame) * (d->EndFrame - d->StartFrame)
		* (d->EndFrame - d->StartFrame)) | 1);
	// table of parabola constants for nrockets
	for (int i = 0; i < d->nrockets; i++)
	{
		d->xcoord[i] = d->leftx + (rand() % (d->rightx - d->leftx));	// rocket firing x coordinate 

		if (!d->target)
		{
			d->py[i] = d->rise / 2 + (rand() % (d->rise / 2));	// parabola y
			d->px[i] = d->rise / 4 + (rand() % (d->rise / 4));	// parabola x
		}
		else // if (d->target)
		{			
			d->py[i] = (9 * d->rise / 10) + (rand() % ( (d->rise / 10) ));	// parabola y
			d->px[i] = (9 * (d->targetx - d->xcoord[i])) / 10 
				+ (rand() % ((d->targetx - d->xcoord[i])/ 10)); 	// parabola x
		}
	}

}

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC rocketsGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    RocketsData* d = (RocketsData*)*instanceData;

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
						// now create rockets
		
		int rockets = 1 + (n / d->interval) % d->life;

		for (int r = 0; r < rockets; r++)
		{
			int m = (n / d->interval - r) % d->nrockets;

			float pp = (d->px[m] * d->px[m]) / (4.0f * (d->py[m]));	// parameter p of parabola
			int factor = (d->life - r * d->interval - n % d->interval);	// to make appearance smaller at height
			int yy = (d->py[m] * factor) / d->life;	// location of rocket relative to parabola 0,0
			int dy = yy / 4;
			unsigned char* col = d->red;			

			for (int hh = yy - dy; hh < yy; hh++)
			{
				if (hh < d->inity - 2 && hh >  2)		// ensure within frame and above pot
				{
					int plume = 2 + (dy - yy + hh) / 16;	// width of plume on this frame
					int w = (int)sqrt(4.0 * pp * (hh)) + d->xcoord[m];
					// this is to ensure rockets fire in all directions
					if (d->xcoord[m] - d->leftx > (d->rightx - d->leftx) / 2
						&& !d->target)
						w = -w;  // relative x coord is translated to image coord
					else if (d->target)
					{						
						if (w < 50 && d->inity - d->py[m] + hh < d->targety + 50)
						{
							col = d->white;
							plume = 64;
						}
						w = d->targetx - w;
					}
					

					if ( true) //(rand() & 1) == 0)	// <(RAND_MAX/2))
					{
						if (w > plume / 2 && w < wd - plume / 2)
						{
							for (int i = 0; i < plume; i++)
							{
								if ((rand() & 1) == 0)		// some randomness to give wavy appearence to plume
								{
									for (int p = 0; p < np; p++)
									{
										if (fi->sampleType == stInteger && nbytes == 1)
										{
											*(dp[p] + ((d->inity - d->py[m] + hh) >> subH[p]) * pitch[p]
												+ ((-plume / 2 + i + w) >> subW[p])) = col[p];
										}
										else if (fi->sampleType == stInteger && nbytes == 2)
										{
											*((uint16_t*)(dp[p]) + ((d->inity - d->py[m] + hh) >> subH[p]) * pitch[p]
												+ ((-plume / 2 + i + w) >> subW[p])) = col[p] << (nbits - 8);
										}

										else if (fi->sampleType == stFloat && nbytes == 4)
										{
											if (p == 0 || fi->colorFamily == cmRGB)
												*((float*)(dp[p]) + ((d->inity - d->py[m] + hh)) * pitch[p]
													+ -plume / 2 + i + w) = col[p] / 256.0f;
											
											else
												*((float*)(dp[p]) + ((d->inity - d->py[m] + hh) >> subH[p]) * pitch[p]
													+ ((-plume / 2 + i + w) >> subW[p])) = (col[p] - 128) / 256.0f;
										}
									}
								}	
							}	

						}	
					}							
				}	
			}		
		}
						
		
		
		//vs_aligned_free (wspan);
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC rocketsFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    RocketsData* d = (RocketsData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->px);
    free(d);
}

static void VS_CC rocketsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	RocketsData d;
	RocketsData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Rockets: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Rockets: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Rockets: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Rockets: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}

	int temp = !!int64ToIntS(vsapi->propGetInt(in, "target", 0, &err));
	if (err)
		d.target = true;
	else if (temp == 0)
		d.target = false;
	else
		d.target = true;

	if (!d.target)
	{
		d.leftx = int64ToIntS(vsapi->propGetInt(in, "lx", 0, &err));
		if (err)
			d.leftx = 0;
		if (d.leftx < 0 || d.leftx > d.vi->width - 1)
		{
			vsapi->setError(out, "Rockets: lx Firing range left side x coord must be in frame");
			vsapi->freeNode(d.node);
			return;
		}
		d.rightx = int64ToIntS(vsapi->propGetInt(in, "rx", 0, &err));
		if (err)
			d.rightx = d.vi->width - 1;
		if (d.rightx <= d.leftx || d.rightx > d.vi->width - 1)
		{
			vsapi->setError(out, "Rockets:rx Firing range right side x coord must not be less than lx and must be in frame");
			vsapi->freeNode(d.node);
			return;
		}
		d.inity = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
		if (err)
			d.inity = d.vi->height - 1;
		if (d.inity < 0 || d.inity > d.vi->height - 1)
		{
			vsapi->setError(out, "Rockets:  Firing range  y coord must be in frame");
			vsapi->freeNode(d.node);
			return;
		}

		d.rise = int64ToIntS(vsapi->propGetInt(in, "rise", 0, &err));
		if (err)
			d.rise = d.inity;
		if (d.inity - d.rise < 0 || d.rise < 1)
		{
			vsapi->setError(out, "Rockets:  Rockets rise should not take them out of frame or become duds");
			vsapi->freeNode(d.node);
			return;
		}
	}	

	else if (d.target)
	{

		d.targetx = int64ToIntS(vsapi->propGetInt(in, "tx", 0, &err));
		if (err)
			d.targetx = d.vi->width - 1;
		else if (d.targetx < 0.75 * d.vi->width || d.targetx > d.vi->width - 1)
		{
			vsapi->setError(out, "Rockets: tx  x coord of target must be 3/4 or more of width of frame and in frame");
			vsapi->freeNode(d.node);
			return;
		}
		d.targety = int64ToIntS(vsapi->propGetInt(in, "ty", 0, &err));
		if (err)
			d.targety = 1;
		if (d.targety > 0.25 * d.vi->height || d.targety < 0)
		{
			vsapi->setError(out, "Rockets: ty y coord of target can be 0 to quarter of frame height.");
			vsapi->freeNode(d.node);
			return;
		}

		d.leftx = 0;
		d.rightx = d.vi->width / 2;
		d.inity = d.vi->height - 1;
		d.rise = d.vi->height - 1;
		
	}
	
	float life = (float)vsapi->propGetFloat(in, "life", 0, &err);
	if (err)
		d.life = (d.EndFrame - d.StartFrame) / 20;
	else
		d.life = (int)((life * d.vi->fpsNum) / (d.vi->fpsDen));
	if (d.life < 2 || d.life > d.EndFrame - d.StartFrame)
	{
		vsapi->setError(out, "Rockets: life of each rocket  can have value resulting in from 2 to ef - sf");
		vsapi->freeNode(d.node);
		return;
	}
	float interval = (float)vsapi->propGetFloat(in, "interval", 0, &err);
	if (err)
		d.interval = (int) ((  d.vi->fpsNum) / d.vi->fpsDen)/4;
	else
		d.interval = (int)(( interval * d.vi->fpsNum) / d.vi->fpsDen);

	if (d.interval  < 1 || d.interval > d.EndFrame - d.StartFrame)
	{
		vsapi->setError(out, "Rockets: firing interval must result in at least one frame and not more than ef - sf frames");
		vsapi->freeNode(d.node);
		return;
	}

	
    data = (RocketsData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Rockets", rocketsInit, rocketsGetFrame, rocketsFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Rockets", "Effect rockets ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Rockets", "clip:clip;sf:int:opt;ef:int:opt;life:float:opt;interval:float:opt;"
				"lx:int:opt;rx:int:opt;y:int:opt;rise:int:opt;"
				"target:int:opt;tx:int:opt;ty:int:opt;", rocketsCreate, 0, plugin);
	
}
*/

/*
Ripple is a function for vfx, a vapoursynth plugin
Creates a Ripple on frame 


Author V.C.Mohan.
Date 26 Mar 2021
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
	int waveLength;			// 0 to 250 varies wave height 
	float speed;			// .0 to 100 speed of wave motion. 
	int x;		// xcoord of top left of pool	
	int y;		// ycoord of  top left of pool
	int swd;			// width of pool
	int sht;		// height of pool
	int xo;		// xcoord of ripple origin	
	int yo;		// ycoordof ripple origin
	int rippleOrigin; 	//preset coord of ripple origin	

	float espeed;			// end speed 0 to 100	
	int samp;			// initial amplitide
	int eamp;			// final amplitude
	int ifr;				// ripples grow from start to  this %age of frames
	int dfr;				// ripples subside from this %age of frames to end frame

	float* sintbl;

} RippleData;



static void VS_CC rippleInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    RippleData* d = (RippleData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
	d->sintbl = (float*)vs_aligned_malloc( sizeof(float) * d->waveLength, 32);
	
	for (int i = 0; i < d->waveLength; i++)
		d->sintbl[i] = (float)(sin(i * M_PI / (d->waveLength / 2)));

	switch (d->rippleOrigin)
	{
	case 1:
		d->xo = d->x;
		d->yo = d->y + d->sht;
		break;
	case 2:
		d->xo = d->x + d->swd / 2;
		d->yo = d->y + d->sht;
		break;
	case 3:
		d->xo = d->x + d->swd;
		d->yo = d->y + d->sht;
		break;
	case 4:
		d->xo = d->x;
		d->yo = d->y + d->sht / 2;
		break;
	case 5:
		d->xo = d->x + d->swd / 2;
		d->yo = d->y + d->sht / 2;
		break;
	case 6:
		d->xo = d->x + d->swd;
		d->yo = d->y + d->sht / 2;
		break;
	case 7:
		d->xo = d->x;
		d->yo = d->y;
		break;
	case 8:
		d->xo = d->x + d->swd / 2;
		d->yo = d->y;
		break;
	case 9:
		d->xo = d->x + d->swd;
		d->yo = d->y;
		break;
	}
}

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC rippleGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    RippleData* d = (RippleData*)*instanceData;

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
		
		// calculate current coordinates, width and height

		int iframe = (nframes * d->ifr) / 100;		// ripples grow upto this frame
		int dframe = (nframes * d->dfr) / 100;		// ripples subside from this frame
		int poolx = d->x;
		int pooly = d->y;
		int poolWidth = d->swd;
		int poolHeight = d->sht;
		int ampl = d->samp;
		
		float speed = d->speed + n * (d->espeed - d->speed) / nframes;
		//  add this displacement for each frame to get wave movement
		int nn = (int)(speed * (nframes - n));	
		int ht = vsapi->getFrameHeight(src, 0);
		int wd = vsapi->getFrameWidth(src, 0);
		int radmax = (int)sqrt((float)(ht * ht + wd * wd));// max radius ripple can grow
			 // max radius to grow in this frame
		int rad = radmax;

		if (n < iframe)
		{
			ampl = d->samp + ((d->eamp - d->samp) * (n - iframe)) / iframe;
			rad = (radmax * n) / iframe;
		}

		else if (n < dframe)
		{
			ampl = d->samp;
			rad = radmax;
		}

		else
		{
			// dframe to endframe		
			ampl = d->samp + ((d->eamp - d->samp) * (n - dframe)) / (d->EndFrame - dframe);
			rad = radmax; 
		}

		

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
		const uint8_t* sp[] = { NULL, NULL, NULL, NULL };
		int pitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			sp[p] = vsapi->getReadPtr(src, p);
			dp[p] = vsapi->getWritePtr(dst, p);
			pitch[p] = vsapi->getStride(dst, p) / nbytes;			
		}
						// now create ripple
		for (int h = pooly; h < pooly + poolHeight; h++)				//
		{
			int hh = (h - d->yo);			

			int hsq = hh * hh;

			for (int w = poolx; w < poolx + poolWidth; w++)
			{
				int ww = (w - d->xo);
				//int wx = xcoord + w - width / 2;

				int radix = (int)sqrt((float)(hsq + (ww * ww))); // radius of circle

				if (radix < rad && radix >= 1)
				{
					// prevent div by zero
					int rdisp = (radix + nn) % d->waveLength;	// position of wave at this point on this frame
					int ydisp = (int)(d->sintbl[rdisp] * ampl);	// how much we move in y direction 
					int xdisp = (int)(d->sintbl[(radix + nn + d->waveLength / 2) % d->waveLength] * ampl);	// how much we move in x direction

					if ((h + ydisp) >= 0 && (h + ydisp) < poolHeight 
						&& w + xdisp >= 0 && w + xdisp < poolWidth)
					{
						for (int p = 0; p < np; p++)
						{

							if (fi->sampleType == stInteger && nbytes == 1)

								*(dp[p] + ((h) >> subH[p]) * pitch[p] + ((w) >> subW[p]))
								= *(sp[p] + ((h + ydisp) >> subH[p]) * pitch[p]
									+ ((w + xdisp) >> subW[p]));

							else if (fi->sampleType == stInteger && nbytes == 2)

								*((uint16_t*)(dp[p]) + ((h) >> subH[p]) * pitch[p] + ((w) >> subW[p]))
								= *((uint16_t*)(sp[p]) + ((h + ydisp) >> subH[p]) * pitch[p]
									+ ((w + xdisp) >> subW[p]));

							else if (fi->sampleType == stFloat && nbytes == 4)

								*((float*)(dp[p]) + +((h) >> subH[p]) * pitch[p] + ((w) >> subW[p]))
								= *((float*)(sp[p]) + ((h + ydisp) >> subH[p]) * pitch[p]
									+ ((w + xdisp) >> subW[p]));
						}

					}	// if h + ydisp

				}	// if radix
			}	// for w
		}
						
		
		
		//vs_aligned_free (wspan);
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC rippleFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    RippleData* d = (RippleData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->sintbl);
    free(d);
}

static void VS_CC rippleCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	RippleData d;
	RippleData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Ripple: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Ripple: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Ripple: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Ripple: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}

	d.waveLength = int64ToIntS(vsapi->propGetInt(in, "wavelen", 0, &err));
	if (err)
		d.waveLength = 12;
	if (d.waveLength < 4 || d.waveLength > 256)
	{
		vsapi->setError(out, "Ripple: wavelen can have values 4 to 256");
		vsapi->freeNode(d.node);
		return;
	}
	d.speed = (float)vsapi->propGetFloat(in, "speed", 0, &err);
	if (err)
		d.speed = 5.0f;
	else if (d.speed < 0.1 || d.speed > 100)
	{
		vsapi->setError(out, "Ripple: speed  can have values 0.1 to 100");
		vsapi->freeNode(d.node);
		return;
	}
	d.espeed = (float)vsapi->propGetFloat(in, "espeed", 0, &err);
	if (err)
		d.espeed = d.speed;
	else if (d.espeed < 0.1 || d.espeed > 100)
	{
		vsapi->setError(out, "Ripple: espeed  can have values 0.1 to 100");
		vsapi->freeNode(d.node);
		return;
	}

	d.samp = int64ToIntS(vsapi->propGetInt(in, "amp", 0, &err));
	if (err)
		d.samp = 4;
	if (d.samp < 1 || d.samp > 16)
	{
		vsapi->setError(out, "Ripple: amp  can have values 1 to 16");
		vsapi->freeNode(d.node);
		return;
	}

	d.eamp = int64ToIntS(vsapi->propGetInt(in, "eamp", 0, &err));
	if (err)
		d.eamp = d.samp;
	if (d.eamp < 1 || d.eamp > d.samp)
	{
		vsapi->setError(out, "Ripple: eamp  can have values 1 to amp");
		vsapi->freeNode(d.node);
		return;
	}

	d.x = int64ToIntS(vsapi->propGetInt(in, "poolx", 0, &err));
	if (err)
		d.x = 0;
	if (d.x < 0 || d.x > d.vi->width - 1)
	{
		vsapi->setError(out, "Ripple: poolx  can have values 0 to frame width - 1 only");
		vsapi->freeNode(d.node);
		return;
	}
	

	d.y = int64ToIntS(vsapi->propGetInt(in, "pooly", 0, &err));
	if (err)
		d.y = 0;
	if (d.y < 0 || d.y > d.vi->height - 1)
	{
		vsapi->setError(out, "Ripple: pooly  can have values 0 to frame height - 1 only");
		vsapi->freeNode(d.node);
		return;
	}

	
	
	d.swd = int64ToIntS(vsapi->propGetInt(in, "wd", 0, &err));
	if (err)
		d.swd = d.vi->width - d.x ;
	if (d.swd < 8 || d.swd + d.x > d.vi->width)
	{
		vsapi->setError(out, "Ripple: wd pool width can have values 8 to frame width - x only");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.sht = int64ToIntS(vsapi->propGetInt(in, "ht", 0, &err));
	if (err)
		d.sht = d.vi->height - d.y;
	if (d.sht < 8 || d.sht + d.y > d.vi->height)
	{
		vsapi->setError(out, "Ripple: ht pool height can have values 8 to frame height - y only");
		vsapi->freeNode(d.node);
		return;
	}
	d.rippleOrigin = int64ToIntS(vsapi->propGetInt(in, "origin", 0, &err));
	if (err)
		d.rippleOrigin = 4;
	if ( d.rippleOrigin < 0 || d.rippleOrigin > 9)
	{
		vsapi->setError(out, "Ripple: (Ripple) origin can have presets 1 to 9 as per number pad or zero for custom position");
		vsapi->freeNode(d.node);
		return;
	}

	if (d.rippleOrigin == 0)
	{
		d.xo = int64ToIntS(vsapi->propGetInt(in, "xo", 0, &err));

		if (err || d.xo < 0 || d.xo > d.vi->width - 1)
		{
			vsapi->setError(out, "Ripple: xo must be specified and should be within frame");
			vsapi->freeNode(d.node);
			return;
		}
		d.yo = int64ToIntS(vsapi->propGetInt(in, "yo", 0, &err));

		if (err || d.yo < 0 || d.yo > d.vi->height - 1)
		{
			vsapi->setError(out, "Ripple: yo  must be specified and be within frame");
			vsapi->freeNode(d.node);
			return;
		}
	}
	

	d.ifr = int64ToIntS(vsapi->propGetInt(in, "ifr", 0, &err));
	if (err)
		d.ifr = 33;
	if (d.ifr < 0 || d.ifr > 100)
	{
		vsapi->setError(out, "Ripple: ifr during this %age frames ripples expand  can have values 0 to 100 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.dfr = int64ToIntS(vsapi->propGetInt(in, "dfr", 0, &err));
	if (err)
		d.dfr = 67;
	if (d.dfr < d.ifr || d.dfr > 100)
	{
		vsapi->setError(out, "Ripple: dfr from which ripple subsides  can have values ifr to 100 only");
		vsapi->freeNode(d.node);
		return;
	}
	

	
    data = (RippleData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Ripple", rippleInit, rippleGetFrame, rippleFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Ripple", "Effect ripple ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Ripple", "clip:clip;sf:int:opt;ef:int:opt;wavelen:int:opt;speed:float:opt;"
				"espeed:float:opt;poolx:int:opt;pooly:int:opt;"
				"wd:int:opt;ht:int:opt;origin:int:opt;xo:int:opt;yo:int:opt;"
				"amp:int:opt;eamp:int:opt;ifr:int:opt;dfr:int:opt;", rippleCreate, 0, plugin);
	
}
*/

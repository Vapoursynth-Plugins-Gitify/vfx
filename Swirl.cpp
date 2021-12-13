/*
Swirl is a function for vfx, a vapoursynth plugin
 . Selected part of Image  swirls in the direction specified.
Thickness controls coarseness of image. Smaller it is it will be finer but slower. The swirl
radius increases from zero at first frame to maximum (radius) specified for grow% length and
swirls till steady% length and decreases to zero at end. Thread safe

Author V.C.Mohan.
Date 9 April 2021
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
	bool dir;	// direction of rotation. True clock wise
	int thick;	//
	int radmax;	// max radius swirl reaches
	int sx;	// center x of swirl
	int sy;	// center y of swirl
	int grow;		// swirl radius grows to radmax upto this frame
	int steady;		// swirl radius remains constant till this frame.
					// After this it decreases to zero
} SwirlData;

template <typename finc>
void rotateRing(finc** dp, const finc** sp, int* pitch,
	int wd, int ht, int* subW, int* subH, int np,
	int radius, int thickness, int cx, int cy, float alfa);

template <typename finc>
void rotateRing(finc** dp, const finc** sp, int* pitch,
	int wd, int ht, int* subW, int* subH, int np,
	int radius, int thickness, int cx, int cy, float alfa)
{
	int sx = VSMAX(cx - radius, 0);
	int ex = VSMIN(cx + radius, wd - 2);
	int sy = VSMAX(cy - radius,0 );
	int ey = VSMIN(cy + radius, ht - 2);
	int rsq = radius * radius;
	int ring = (radius - thickness);
	if (ring <= thickness) return;
	int ringsq = ring * ring;

	int andH = (1 << subH[1]) - 1;
	int andW = (1 << subW[1]) - 1;
	float sinalfa = sin(alfa);
	float cosalfa = cos(alfa);

	for (int h = sy; h < ey; h++)
	{
		int hsq = (cy - h) * (cy - h);

		for (int w = sx; w < ex; w++)
		{
			if (hsq + (cx - w) * (cx - w) <= rsq
				&& hsq + (cx - w) * (cx - w) > ringsq)
			{
				float newx = (cx - w) * cosalfa - (cy - h) * sinalfa;
				int x = (int)newx + cx;
				float newy = (cx - w) * sinalfa + (cy - h) * cosalfa;
				int y = (int)newy + cy;

				if (y >= 0 && y < ht && x >= 0 && x < wd)
				{

					*(dp[0] + h * pitch[0] + w) = *(sp[0] + y * pitch[0] + x);
					if ((h & andH) == 0 && (w & andW) == 0)
					{
						for (int p = 1; p < np; p++)
						{
							*(dp[p] + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
								= *(sp[p] + (y >> subH[p]) * pitch[p] + (x >> subW[p]));
						}
					}
				}
			}
		}
	}
}


static void VS_CC swirlInit(VSMap* in, VSMap* out, void** instanceData,
	VSNode* node, VSCore* core, const VSAPI* vsapi)
{
	SwirlData* d = (SwirlData*)*instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);
	const VSFormat* fi = d->vi->format;
}
//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC swirlGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SwirlData* d = (SwirlData*)*instanceData;

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
		const uint8_t* sp[] = { NULL, NULL, NULL, NULL };
		int pitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			sp[p] = vsapi->getReadPtr(src, p);
			dp[p] = vsapi->getWritePtr(dst, p);
			pitch[p] = vsapi->getStride(dst, p) / nbytes;			
		}

		int growth = (d->grow * nframes) / 100;
		int steady = (d->steady * nframes) / 100;
						// now create swirl		
		int radius =  n < growth ? ( n * d->radmax) / growth
			: n < steady ? d->radmax : d->radmax * ( nframes - n) / (nframes - d->steady);

		for (int i = radius ; i > d->thick; i -= d->thick / 2)
		//for (int i = 2 * d->thick; i < radius; i += d->thick / 2)
		{			
			float alfa = (float)(M_PI * i * n) / 180.0f;
			if (d->dir)
				alfa = -alfa;						

			if (fi->sampleType == stInteger && nbytes == 1)
			{
				rotateRing(dp, sp, pitch,
					wd, ht, subW, subH, np,
					i, d->thick, d->sx, d->sy, alfa);
			}
			else if (fi->sampleType == stInteger && nbytes == 2)
			{
				rotateRing((uint16_t**)dp, (const uint16_t**)sp, pitch,
					wd, ht, subW, subH, np,
					i, d->thick, d->sx, d->sy, alfa);
			}
			else if (fi->sampleType == stFloat && nbytes == 4)
			{

				rotateRing((float**)dp, (const float**)sp, pitch,
					wd, ht, subW, subH, np,
					i, d->thick, d->sx, d->sy, alfa);
			}

		}			
		
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC swirlFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SwirlData* d = (SwirlData*)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}

static void VS_CC swirlCreate(const VSMap* in, VSMap* out, void* userData,
									VSCore* core, const VSAPI* vsapi)
{
	SwirlData d;
	SwirlData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Swirl: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Swirl: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Swirl: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Swirl: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
		
	int temp = int64ToIntS(vsapi->propGetInt(in, "q", 0, &err));
	if (err)
		temp = 2 ;
	if (temp < 1 || temp > 10 )
	{
		vsapi->setError(out, "Swirl:  q can be  1 to 10 for fine to coarse quality");
		vsapi->freeNode(d.node);
		return;
	}

	d.thick = temp * 2;

	d.radmax = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.radmax = d.vi->height  / 2;
	if (d.radmax < 32 || (d.radmax > d.vi->width && d.radmax > d.vi->height))
	{
		vsapi->setError(out, "Swirl:  rad can be 32 to frame width or height");
		vsapi->freeNode(d.node);
		return;
	}
	d.sx = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.sx = d.vi->width / 2; 
	else if (d.sx < 0 || d.sx > d.vi->width )
	{
		vsapi->setError(out, "Swirl:  x can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.sy = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.sy = d.vi->height / 2;
	else if (d.sy < 0 || d.sy > d.vi->height)
	{
		vsapi->setError(out, "Swirl:  y can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}	
	
	d.grow = int64ToIntS(vsapi->propGetInt(in, "grow", 0, &err));
	if (err)
		d.grow = 50;

	if (d.grow < 1 || d.grow > 100)
	{
		vsapi->setError(out, "Swirl: grow %age of effect duration must be between 1 and 100");
		vsapi->freeNode(d.node);
		return;
	}	

	d.steady = int64ToIntS(vsapi->propGetInt(in, "steady", 0, &err));
	if (err)
		d.steady = 80;

	if (d.steady < d.grow || d.steady > 100)
	{
		vsapi->setError(out, "Swirl: steady up to  %age of effect duration must be between grow and 100");
		vsapi->freeNode(d.node);
		return;
	}	
	
	temp = !!int64ToIntS(vsapi->propGetInt(in, "dir", 0, &err));
	if (err)
		d.dir = true;
	else if (temp == 0)
		d.dir = false;
	else
		d.dir = true;

		
    data = (SwirlData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Swirl", swirlInit, swirlGetFrame, swirlFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init lambda appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Swirl", "Effect swirl ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Swirl", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;x:int:opt;y:int:opt;"
	"q:int:opt;dir:int:opt;grow:int:opt;steady:int:opt;", swirlCreate, 0, plugin);
	
}
*/

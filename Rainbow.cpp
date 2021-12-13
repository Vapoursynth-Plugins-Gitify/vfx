/*
Rainbow is a function for vfx, a vapoursynth plugin
Creates a Rainbow on frame 


Author V.C.Mohan.
Date 22 Mar 2021
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
	int rad;	// radius of rainbow
	int x;		// initial x coord of center	
	int y;		// initial y coord of center
	int erad;		// final radius of rainbow
	int ex;		// final x coord of center
	int ey;		// final y coord of rainbow center
	int xleft;	// rainbow left end start x coord
	int xright;// rainbow ending x coord
	int exleft;	// final left x cutoff	
	int exright;// final right x cutoff

	uint8_t * col;

} RainbowData;



static void VS_CC rainbowInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    RainbowData* d = (RainbowData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
	d->col = (uint8_t*)vs_aligned_malloc( 3 * 80, 8);
	

	// rainbow colors gradients
	// red - yellow
	for (int i = 0; i < 24; i++)
	{
		d->col[3 * i + 2] = 250;
		d->col[3 * i + 1] =  10 * i;
		d->col[3 * i ] = 7;

	}
	// yellow to green
	for (int i = 24; i < 40; i++)
	{
		d->col[3 * i + 2] = 255 - (i - 24) * 10;
		d->col[3 * i + 1] = 250;
		d->col[3 * i] = i * 2;
	}
	// green to blue
	for (int i = 40; i < 56; i++)
	{

		d->col[3 * i + 2] = 7;
		d->col[3 * i + 1] = 250 - (i - 40) * 14;
		d->col[3 * i] =  16 * (i - 24);

	}
	// blue to voilet
	for (int i = 56; i < 64; i++)
	{

		d->col[3 * i + 2] = (i - 56) * 20;
		d->col[3 * i + 1] = 7;
		d->col[3 * i] = 250;

	}
	//  violet extr
	for (int i = 64; i < 76; i++)
	{

		d->col[3 * i + 2] =  160 + (i - 64) * 8;
		d->col[3 * i + 1] = 7;
		d->col[3 * i] = 250 ;

	}
	/*for (int i = 48; i < 52; i++)
	{

		d->col[3 * i + 2] = 50;
		d->col[3 * i + 1] = 4;
		d->col[3 * i] = 250;

	}*/

	if (d->vi->format->colorFamily == cmYUV)
	{
		unsigned char yuv[] = { 0,0,0 };
		
		for ( int i = 0 ; i < 82; i ++)
		{
			BGR8YUV(yuv, d->col + 3 * i);

			for (int k = 0; k < 3; k++)
			{
				d->col[3 * i + k] = yuv[k];
			}
		}
	}

	else
	{
		// this part is nonsense. May be my colorconverter is wrong. This corrects it
		// it looks the internal order is RGB and not BGR as in documentation?
		unsigned char BGR[] = { 0,0,0 };
		for (int i = 0; i < 82; i++)
		{
			for (int k = 0; k < 3; k++)
			{
				BGR[2 - k] = d->col[3 * i + k];
			}
			for (int k = 0; k < 3; k++)
			{
				d->col[3 * i + k] = BGR[2 - k];
			}
		}
	}
}

//------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC rainbowGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    RainbowData* d = (RainbowData*)*instanceData;

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
		
		// calculate values for current frame
		int initx = d->x + (n * (d->ex - d->x)) / nframes;
		int inity = d->y + (n * (d->ey - d->y)) / nframes;
		int radius = d->rad + (n * (d->erad - d->rad)) / nframes;
		int xleft = d->xleft + ((d->exleft - d->xleft) * n) / nframes;
		int xright = d->xright + ((d->exright - d->xright) * n) / nframes;

		if (initx + radius < 0 || initx - radius >= wd 
			|| inity + radius < 0 || inity - radius >= ht)
		
			return src;	// no work to be done as either dst or src circle is
								// outside frame area
		int sx = VSMAX(VSMAX(initx - radius , 4), xleft);
		int sy = VSMAX(inity - radius, 4);
		int ex = VSMIN(VSMIN(initx + radius + 4, wd - 1), xright);
		int ey = VSMIN(inity , ht  - 1);
		

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
						// now create rainbow
		int radsq = radius * radius;
		int thick = 1;
		int radtsq = (radius - thick) * (radius - thick);

		for (int i = 0; i < 72; i+= thick)
		{
			int rsq = (radius - i) * (radius - i);
			int rtsq = (radius - i - thick) * (radius - i - thick);			

			for (int h = sy; h < ey; h++)
			{				
				int hsq = (h  - inity) * (h  - inity);

				for (int w = sx; w < ex; w++)
				{
					int pblur = 0;
					// Add a little haziness in the border pixels
					if (i < 2 || i > 70)
					{
						pblur = (rand() % (radius / 4)) - radius / 4;
					}
					if (hsq + (w  - initx) * (w  - initx)  <= rsq + pblur
						&& hsq + (w  - initx) * (w  - initx)  >= rtsq + pblur )
					{
						int blur =  (rand() % 3); // to make rainbow colors appear lightly blurred

						for (int p = 0; p < np; p++)
						{
							if (fi->sampleType == stInteger && nbytes == 1)
							{
								if ( p == 0 || (subH[p] == 0 && subW[p] == 0 ))

									*(dp[p] + (h  * pitch[p]) + (w))
									= d->col[3 * (i + blur) + p ];

								else if ( ( h & andH) == 0 && (w & andW) == 0)

									*(dp[p] + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= d->col[3 * (i + blur) + p];

							}
							else if (fi->sampleType == stInteger &&  nbytes == 2)
							{
								if (p == 0 || (subH[p] == 0 && subW[p] == 0))

									*((uint16_t*)(dp[p]) + (h * pitch[p]) + (w))
										= d->col[3 * (i + blur) + p] << ( nbits - 8);

								else if ((h & andH) == 0 && (w & andW) == 0)

										*((uint16_t*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= d->col[3 * (i + blur) + p] << (nbits - 8);
							}

							else if (fi->sampleType == stFloat && nbytes == 4)
							{
								if (p == 0 || fi->colorFamily == cmRGB)

									*((float *)(dp[p]) + (h * pitch[p]) + (w))
										= d->col[3 * (i + blur) + p] / 255.0f;

								else if ((h & andH) == 0 && (w & andW) == 0)

										*((float*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p]))
										= (d->col[3 * (i + blur) + p] - 128.0f) / 256.0f;

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

static void VS_CC rainbowFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    RainbowData* d = (RainbowData*)instanceData;
    vsapi->freeNode(d->node);	
		
    free(d);
}

static void VS_CC rainbowCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	RainbowData d;
	RainbowData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Rainbow: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Rainbow: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Rainbow: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Rainbow: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}

	d.rad = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.rad = d.vi->height/2;
	if (d.rad < 128 || d.rad > d.vi->height / 2)
	{
		vsapi->setError(out, "Rainbow: rad  can have values 128 to frame height / 2 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.erad = int64ToIntS(vsapi->propGetInt(in, "erad", 0, &err));
	if (err)
		d.erad = d.rad;
	else if (d.erad < 128 || d.erad > d.vi->height / 2)
	{
		vsapi->setError(out, "Rainbow: erad  can have values 128 to frame height / 2 only");
		vsapi->freeNode(d.node);
		return;
	}

	d.x = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.x = d.vi->width / 2;
	if (d.x < 1 || d.x > d.vi->width - 1)
	{
		vsapi->setError(out, "Rainbow: x  can have values 1 to frame width - 1 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.ex = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.ex = d.x;
	if (d.ex < 1 || d.ex > d.vi->width - 1)
	{
		vsapi->setError(out, "Rainbow: ex  can have values 1 to frame width - 1 only");
		vsapi->freeNode(d.node);
		return;
	}

	d.y = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.y = d.vi->height / 2;
	if (d.y < 64 || d.y > d.vi->height - 1)
	{
		vsapi->setError(out, "Rainbow: y  can have values 1 to frame height - 1 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.ey = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.ey = d.y;
	if (d.ey < 64 || d.ey > d.vi->height -1)
	{
		vsapi->setError(out, "Rainbow: ex  can have values 64 to frame height - 1 only");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.xleft = int64ToIntS(vsapi->propGetInt(in, "lx", 0, &err));
	if (err)
		d.xleft = 0; // d.vi->width / 2;
	if (d.xleft < 0 || d.xleft > d.vi->width - 64)
	{
		vsapi->setError(out, "Rainbow: xleft  can have values 0 to frame width -64 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.exleft = int64ToIntS(vsapi->propGetInt(in, "exl", 0, &err));
	if (err)
		d.exleft = d.xleft;
	else if (d.exleft < 64 || d.exleft > d.vi->width - 64)
	{
		vsapi->setError(out, "Rainbow: exl  can have values 0 to frame width - 64 only");
		vsapi->freeNode(d.node);
		return;
	}

	d.xright = int64ToIntS(vsapi->propGetInt(in, "rx", 0, &err));
	if (err)
		d.xright = d.vi->width - 64;
	if (d.xright < 0 || d.xright > d.vi->width - 64)
	{
		vsapi->setError(out, "Rainbow: xright  can have values 0 to frame width -64 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.exright = int64ToIntS(vsapi->propGetInt(in, "erx", 0, &err));
	if (err)
		d.exright = d.xright;
	if (d.exright < 64 || d.exright > d.vi->width - 64)
	{
		vsapi->setError(out, "Rainbow: xright  can have values 64 to frame width -64 only");
		vsapi->freeNode(d.node);
		return;
	}
	

	
    data = (RainbowData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Rainbow", rainbowInit, rainbowGetFrame, rainbowFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Rainbow", "Effect rainbow ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Rainbow", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;erad:int:opt;"
				"x:int:opt;ex:int:opt;y:int:opt;ey:int:opt;"
				"lx:int:opt;elx:int:opt;rx:int:opt;erx:int:opt;", rainbowCreate, 0, plugin);
	
}
*/

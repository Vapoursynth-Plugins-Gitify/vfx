/*
FlowerPot is a function for vfx, a vapoursynth plugin
Creates a FlowerPot firework 


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
	int initx;
	int inity;
	int rise;
	//int yfloor;
	int endx;
	int endy;
	float zoom;
	bool color;
	int yfloor1, yfloor2;
	int nbeams;
	int spread;
	int* px, * py;
	unsigned char* red;
} FlowerPotData;


static void VS_CC flowerpotInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
    FlowerPotData* d = (FlowerPotData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

	d->nbeams = 100;
	d->spread = (int) (d->vi->fpsNum / d->vi->fpsDen);   //Each beam lasts this many frames

	// flowers do not drop below this level
	d->yfloor1 = d->inity;
	d->yfloor2 = d->endy;
	
	d->px = (int*)vs_aligned_malloc(sizeof(int) * 2 * (d->nbeams + 2), 32);
	d->py = d->px + d->nbeams + 2;
	d->red = (uint8_t*)vs_aligned_malloc(d->nbeams + 2, 8);
	int max = d->vi->format->colorFamily == cmRGB ? 255 : 235;

	srand(((d->EndFrame - d->StartFrame) * (d->EndFrame - d->StartFrame)
		* (d->EndFrame - d->StartFrame)) | 1);

	for (int i = 0; i < d->nbeams + 2; i++)
	{
		d->py[i] = 1 + rand() % (d->rise );	// parabola y
		d->px[i] = 1 + rand() % (d->rise / 4);	// parabola x

		if (d->color)
			d->red[i] = (uint8_t)(50 + rand() % (max - 50));
		else
			d->red[i] = (uint8_t)max;
	}
	
}
//------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC flowerpotGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    FlowerPotData* d = (FlowerPotData*)*instanceData;

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

		int nmax = n > d->spread ? n + d->spread : n + n + 1;
		// Parabola eqn is 4*P*Y=X*X
		int ylimit = d->yfloor1 + (n * (d->yfloor2 - d->yfloor1)) / nframes;
		if (ylimit > ht - 2)
			ylimit = ht - 2;
		int xcoord = d->initx + (n * (d->endx - d->initx)) / nframes;	// x direction movement
		int ycoord = d->inity + (n * (d->endy - d->inity)) / nframes;	// y direction movement
		float zmag = 1.0f + (n * (d->zoom - 1.0f) ) / nframes;	// z direction movement (magnification)
		// why we want this
		srand(((n + 1) * (n + 2) * (n + 3)) & 0xfffffffe);			

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

		uint8_t* dpt[] = { NULL, NULL, NULL, NULL };
		int dpitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{
			dpt[p] = vsapi->getWritePtr(dst, p);
			dpitch[p] = vsapi->getStride(dst, p) / nbytes;			
		}


		for (int nx = n; nx < nmax; nx++)
		{
			int modnx = nx % d->spread;
			// parabola constant P
			float pp = (d->px[modnx] * d->px[modnx]) / (4.0f * (d->py[modnx]));	
			// position of the ember wrt center of source
			int dx = ((d->px[modnx]) * (nmax - nx)) / (d->spread / 2);
			dx = (int)(zmag * dx);

			int sx = 16 * d->px[modnx] / d->spread;

			int  yy;
			
			unsigned char yuv[] = { d->red[modnx], d->red[modnx + 1], d->red[modnx + 2] };
			if (!d->color)
			{
				if (fi->colorFamily == cmYUV)
				{
					yuv[1] = 127;
					yuv[2] = 127;
				}
				else if (fi->colorFamily == cmRGB)
				{
					yuv[1] = yuv[0];
					yuv[2] = yuv[0];
				}
			}
			
			for (int ww = 0; ww < sx; ww++)
			{
				// relative y coord shift to image coord
				yy = (int)(((d->px[modnx] - dx - ww) * (d->px[modnx] - dx - ww) 
					/ (4.0 * pp) - d->py[modnx] + ycoord));  
				yy = (int)(zmag * (yy - ycoord)) + ycoord;
				yy = yy & 0xfffffffe;

				if (yy < ylimit && yy > 2)		// ensure within frame and above pot
				{
					for (int i = -1; i <= 1; i += 2)
					{
						int w = xcoord + i * (dx + ww);

						if (w < wd - 2 && w > 2)
						{
							for (int p = 0; p < np; p++)
							{
								if (fi->sampleType == stInteger && nbytes == 1)
								{
									uint8_t* dp = dpt[p];
									// give each beam a little body
									if (p == 0 || (subH[p] == 0 && subW[p] == 0))
									{
										*(dp + yy * dpitch[p] + w)
											= yuv[p];
										*(dp + (yy + 1) * dpitch[p] + w)
											= yuv[p];
										*(dp + yy * dpitch[p] + w + 1)
											= yuv[p];
										*(dp + (yy + 1) * dpitch[p] + w + 1)
											= yuv[p];

									}
									else if ((yy & andH) == 0 && (w & andW) == 0)
									{
										*(dp + (yy >> subH[p]) * dpitch[p] + (w >> subW[p]))
											= yuv[p];
									}
								}

								else if (fi->sampleType == stInteger &&  nbytes == 2)
								{
									uint16_t* dp = (uint16_t*)(dpt[p]);
									uint16_t val = (uint16_t)((int)(yuv[p]) << (nbits - 8));
									if (p == 0 || (subH[p] == 0 && subW[p] == 0))
									{
										*(dp + yy * dpitch[p] + w)
											= val;
										*(dp + (yy + 1) * dpitch[p] + w)
											= val;
										*(dp + yy * dpitch[p] + w + 1)
											= val;
										*(dp + (yy + 1) * dpitch[p] + w + 1)
											= val;

									}
									else if ((yy & andH) == 0 && (w & andW) == 0)
									{
										*(dp + (yy >> subH[p]) * dpitch[p] + (w >> subW[p]))
											= val;
									}
								}

								if (nbytes == 4)
								{
									float max = fi->colorFamily == cmRGB ? 255.0f : 235.0f;
									float* dp = (float*)(dpt[p]);
									float val = (float)((float)(yuv[p]) / max);

									if (fi->colorFamily == cmYUV && p > 0)
										val -= 0.5f;

									if (p == 0 || (subH[p] == 0 && subW[p] == 0))
									{
										*(dp + yy * dpitch[p] + w)
											= val;
										*(dp + (yy + 1) * dpitch[p] + w)
											= val;
										*(dp + yy * dpitch[p] + w + 1)
											= val;
										*(dp + (yy + 1) * dpitch[p] + w + 1)
											= val;

									}
									else if ((yy & andH) == 0 && (w & andW) == 0)
									{
										*(dp + (yy >> subH[p]) * dpitch[p] + (w >> subW[p]))
											= val;
									}
								}
							}


						}	// if w 
					}	// for i = - 1						
				}	// if yy
			}	// for int ww = 0;
		}	// for int nx
		// 
	

		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC flowerpotFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    FlowerPotData* d = (FlowerPotData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->px);
	vs_aligned_free(d->red);
    free(d);
}

static void VS_CC flowerpotCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	FlowerPotData d;
	FlowerPotData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "FlowerPot: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "FlowerPot: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "FlowerPot: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "FlowerPot: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.initx = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.initx = d.vi->width / 2;
	else if (d.initx < 0 || d.initx > d.vi->width - 1)
	{
		vsapi->setError(out, "FlowerPot: x must be within frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.inity = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.inity = d.vi->height - 1;
	else if (d.inity < 0 || d.inity > d.vi->height - 64)
	{
		vsapi->setError(out, "FlowerPot: y must be 0 to height - 64");
		vsapi->freeNode(d.node);
		return;
	}

	d.endx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.endx = d.initx;
	else if (d.endx < 0 || d.endx > d.vi->width - 1)
	{
		vsapi->setError(out, "FlowerPot: ex must be within frame");
		vsapi->freeNode(d.node);
		return;
	}
	d.endy = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.endy = d.inity;
	else if (d.endy < 0 || d.endy > d.vi->height - 64)
	{
		vsapi->setError(out, "FlowerPot: ey must be 0 to height - 64");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.rise = int64ToIntS(vsapi->propGetInt(in, "rise", 0, &err));
	if (err)
		d.rise = d.inity;
	else if (d.rise < 64 || d.rise > d.inity)
	{
		vsapi->setError(out, "FlowerPot: rise  must be 64 to 64 less than lesser of y and ey");
		vsapi->freeNode(d.node);
		return;
	}

	d.zoom = (float)vsapi->propGetFloat(in, "zoom", 0, &err);
	if (err)
		d.zoom = 1.0f;
	else if (d.zoom < 0.1f || d.zoom > 4.0f)
	{
		vsapi->setError(out, "FlowerPot: zoom can be 0.1 to 4.0 only");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.initx + d.rise / 4 < 2 || d.initx - d.rise / 4 > d.vi->width - 2)
	{
		vsapi->setError(out, "FlowerPot: rise and x values must ensure vflower pot is visible");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.endx + (d.rise * d.zoom) / 400  < 2 || d.endx - (d.rise * d.zoom) / 400 > d.vi->width - 2)
	{
		vsapi->setError(out, "FlowerPot: rise, zoom and endx values must ensure vflower pot is visible");
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

	
    data = (FlowerPotData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "FlowerPot", flowerpotInit, flowerpotGetFrame, flowerpotFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "FlowerPot", "Effect flowerpot ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("FlowerPot", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;rise:int:opt;"
	"ex:int:opt;ey:int:opt;zoom:float:opt;color:int:opt;", flowerpotCreate, 0, plugin);
	
}
*/

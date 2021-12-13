/*
Balloon is a function for vfx, a vapoursynth plugin
Creates a balloon with radius, direction of light , its location.
Balloon will hop if oped during the effect.


Author V.C.Mohan.
Date 18 Feb 2021, 18 july 2021
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

    int StartFrame;	// 0
    int EndFrame;	// end
    int radius;		// balloon radius 1/8 min(ht,wd)
   
    float opacity;	//  0.4
    int nhops;		// number of hops balloon makes in the effect 1
    int rise;		// rise of balloon over floor
    int startx;		// balloon center starting x
    int finalx;		// balloon center end x value
    		
	int floory;		// balloon will not come below this height
    
    int light;		// center, left, right, top, bottom, none.as per numeric pad.
    float refl;		// reflectivity 0.2
    float offset;		// fraction of radius light offset in the Z direction 0.8
    int lightx;		// frame light x coord 
    int lighty;		// frame light y coord

    int xoffset;
    int yoffset;
    //int bias;
    float px, py, p;// parabolic parameters
	uint8_t bgr[3], yuv[3]; // , reflBGR[3];	// balloon colors
	int* xoffs;
	int* yoffs;
	int  noffs;	// x and y offsets and number of offsets
    unsigned char* ballcol;	// balloon color in B,G,R or YUV  triplets for each offset
} BalloonData;

int DefineBalloon(uint8_t* ballcolor, int* xoff, int* yoff,
    const uint8_t* col, int radius, int xoffset, int yoffset,
    float refl, uint8_t max = 255);


static void VS_CC balloonInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi) {
    BalloonData* d = (BalloonData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

	// convert colors
		
	BGR2YUV(d->bgr, d->yuv);

	d->py = (float)(d->rise);

	if (d->nhops > 0)
	{
		d->px = ((float)d->finalx - (float)d->startx) / (2.0f * d->nhops);
	}
	else
	{
		//for safety. Not really used
		d->py = 1.0;
		d->px = 1.0;
	}
	// parabola eqn
	d->p = (d->px * d->px) / (4.0f * d->py);


	switch (d->light)
	{

		case  5:	// from front

			d->xoffset = 0;
			d->yoffset = 0;
			break;

		case 8:  //"north"

			d->xoffset = 0;
			d->yoffset = (int)(-(d->radius * d->offset));
			break;

		case 2: // "south"

			d->xoffset = 0;
			d->yoffset = (int)(d->radius * d->offset);
			break;

		case 6: //"east"

			d->yoffset = 0;
			d->xoffset = (int)(d->radius * d->offset);
			break;

		case 4: // "west"

			d->yoffset = 0;
			d->xoffset = (int)-(d->radius * d->offset);
			break;

		case 3: // "se"

			d->xoffset = (int)(d->radius * d->offset);
			d->yoffset = (int)(d->radius * d->offset);
			break;

		case 1: // "sw"

			d->xoffset = (int)-(d->radius * d->offset);
			d->yoffset = (int)(d->radius * d->offset);
			break;

		case 9: // "ne"

			d->xoffset = (int)(d->radius * d->offset);
			d->yoffset = (int)-(d->radius * d->offset);
			break;

		case 7: // "nw"

			d->xoffset = (int)-(d->radius * d->offset);
			d->yoffset = (int)-(d->radius * d->offset);
			break;
	}
	// as for case 0, light direction depends on balloon position, its done for each frame
	if ( d->light != 0)
	{
		int rsq = d->radius * d->radius;
		d->xoffs = (int*)vs_aligned_malloc<int>(sizeof(int) * 4 * rsq, 32);
		d->yoffs = (int*)vs_aligned_malloc<int>(sizeof(int) * 4 * rsq, 32);
		d->ballcol = (unsigned char*)vs_aligned_malloc<unsigned char>(  3 * 4 * rsq, 8);

		if ( d->vi->format->colorFamily == cmRGB)
			d->noffs = DefineBalloon(d->ballcol, d->xoffs, d->yoffs,
					d->bgr, d->radius, d->xoffset, d->yoffset,
					d->refl);
		else
			d->noffs = DefineBalloon(d->ballcol, d->xoffs, d->yoffs,
				d->yuv, d->radius, d->xoffset, d->yoffset,
				d->refl,(unsigned char)230);

	}
	//if ((int)d->yuv[0] < 200)
	///if ( d->vi->format->colorFamily == cmYUV)
	if(d->noffs == 0)
	{
		vsapi->setError(out, "Balloon: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d->node);
		return;
	}
	
}
//------------------------------------------------------------------------
int DefineBalloon(unsigned char* ballcolor, int* xoff, int* yoff,
	const unsigned char* col, int radius, int xoffset, int yoffset,
	float refl, unsigned char max)
{
	
	int rsq = radius * radius;
	float inc = (max * refl * rsq);
	
	int noffsets = 0;
	
	
	for (int x = -radius; x < radius; x++)
	{
		int xsq = x * x;
		for (int y = -radius; y < radius; y++)
		{
			if (xsq +  y * y <= rsq)
			{
				xoff[noffsets] = x;
				yoff[noffsets] = y;
				// find distance from light
				int dsq = (xoffset - x) * (xoffset - x) + (yoffset - y) * (yoffset - y) + rsq;
			
				for (int i = 0; i < 3; i++)
				{
					ballcolor[3 * noffsets + i] =
						(col[i] + inc / dsq) > max ? max :
						(uint8_t)(col[i] + inc / dsq);
				}

				noffsets++;

			}
		}
	}

	return noffsets;
}
//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC balloonGetFrame(int n, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    BalloonData* d = (BalloonData*)*instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    }
	else if (activationReason == arAllFramesReady) {
		const VSFrameRef* src = vsapi->getFrameFilter(n, d->node, frameCtx);

		if (n < d->StartFrame || n > d->EndFrame)
			return src;
		int in = n - d->StartFrame;
		int nframes = d->EndFrame - d->StartFrame + 1;
		const VSFormat* fi = d->vi->format;
		int xx;	// current x coord of balloon
		int yy;	// current y coord of balloon

		if (d->nhops > 0)
		{
			xx = d->startx + (in * (d->finalx - d->startx)) / nframes;	// x coord of balloon
			int xrel = (xx - d->startx) % (int)(2 * d->px);

			xrel = xrel > d->px ? xrel - (int)d->px : (int)d->px - xrel;	// Parabola eqn is 4*P*Y=X*X
			yy = d->vi->height - d->floory+ (int)( (xrel * xrel) / (4.0 * d->p));  // relative y coord shift to image coord;

			
		}

		else
		{
			xx = d->startx;
			yy =  d->floory - d->rise ;
		}

		int* xoff = NULL, * yoff = NULL;
		unsigned char* ballcolor = NULL;
		int noffsets = 0;
		
		if (d->light == 0) // "frame"
		{
			ballcolor = (unsigned char*)vs_aligned_malloc<unsigned char>( sizeof(unsigned char) * 3 * 4 * d->radius * d->radius, 16);
			xoff = (int*)vs_aligned_malloc<int>(sizeof(int) * 4 * d->radius * d->radius, 32);
			yoff = (int*)vs_aligned_malloc<int>(sizeof(int) * 4 * d->radius * d->radius, 32);	// second half of xoff buf
				// current balloon center x and y coord distances squares from light. prevent being zero
			int dsx = (d->lightx - xx) * (d->lightx - xx);
			int dsy = (d->lighty - yy) * (d->lighty - yy);
			int dsq = dsx + dsy;
			int dist = 1 + (int)sqrt( (float)(dsq) );
				// balloon center distances
			int xxoffset = (int)((d->lightx - xx) * d->radius * d->offset)/ dist ;
			int yyoffset = (int)((d->lighty - yy) * d->radius * d->offset )/ dist;
			
			if ( fi->colorFamily == cmRGB)
				noffsets = DefineBalloon(ballcolor, xoff, yoff,
					d->bgr, d->radius, xxoffset, yyoffset,
					d->refl);
			else

				noffsets = DefineBalloon(ballcolor, xoff, yoff,
					d->yuv, d->radius, xxoffset, yyoffset,
					d->refl, (unsigned char) 230);
		}

		else
		{
			ballcolor = d->ballcol;
			xoff = d->xoffs;
			yoff = d->yoffs;
			noffsets = d->noffs;
		}

		

		VSFrameRef* dst = vsapi->copyFrame(src, core);		
		int height = vsapi->getFrameHeight(src, 0);
		int width = vsapi->getFrameWidth(src, 0);
		int nbytes = fi->bytesPerSample;
		int nbits = fi->bitsPerSample;
		int nb = fi->bitsPerSample;
		int subH = fi->subSamplingH;
		int subW = fi->subSamplingW;

		int andH = ( 1 << subH) - 1;
		int andW = (1 << subW) - 1;
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;
		float transparency = 1.0f - d->opacity;

		uint8_t* dpt[] = { NULL, NULL, NULL, NULL };
		int dpitch[] = { 0,0,0 };		
		
		for (int p = 0; p < np; p++)
		{

			dpt[p] = vsapi->getWritePtr(dst, p);
			dpitch[p] = vsapi->getStride(dst, p);
			
		}


		for (int i = 0; i < noffsets; i++)
		{
			if (xx + xoff[i] < 0 || xx + xoff[i] >= width
				|| yy + yoff[i] < 0 || yy + yoff[i] >= height) continue;	// outside frame

			if (fi->sampleType == stInteger)
			{
				if (nbytes == 1)
				{
					for (int p = 0; p < np; p++)
					{
						if (p == 0 || fi->colorFamily == cmRGB)
						{
							//input clip val at this location
							float val = (float)(*(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i])));
							// check whether input pixel has higher value to overcome balloon color and its opacity
							if (val * transparency > ballcolor[3 * i + p])
							{
								*(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i])) =
									(uint8_t)(val * transparency + ballcolor[3 * i + p] * d->opacity);
							}

							else
							{
								// if not use balloon color itself
								*(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i])) = ballcolor[3 * i + p];
							}
						}

						else //  YUV
						{
							if (((yy + yoff[i]) & andH) == 0 && ((xx + xoff[i]) & andW) == 0)
								*(dpt[p] + ((yy + yoff[i]) >> subH) * dpitch[p] + ((xx + xoff[i]) >> subW))
								= d->yuv[p];

						}

					}
				}

				else if (nbytes == 2)
				{

					for (int p = 0; p < np; p++)
					{
						int bcolor = ((int)ballcolor[3 * i + p]) << (nbits - 8);
						if (p == 0 || fi->colorFamily == cmRGB)
						{
							//input clip val at this location
							int val = (int)(*(uint16_t*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes));
							// check whether input pixel has higher value to overcome balloon color and its opacity
							if (val * transparency > bcolor)
							{
								*(uint16_t*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes) =
									((uint16_t)(val * transparency + bcolor * d->opacity) );
							}

							else
							{
								// if not use balloon color itself
								*(uint16_t*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes) = (uint16_t) bcolor;
							}
						}

						else //  U V
						{
							if (((yy + yoff[i]) & andH) == 0 && ((xx + xoff[i]) & andW) == 0)
								*(uint16_t*)(dpt[p] + ((yy + yoff[i]) >> subH) * dpitch[p] + ((xx + xoff[i]) >> subW) * nbytes)
								= (uint16_t)bcolor;

						}

					}
				}

			}

			else if (nbytes == 4)
			{
				float max = fi->colorFamily == cmRGB ? 255.0f :230.0f ;
				
				for (int p = 0; p < np; p++)
				{
					float bcolor = ((float)ballcolor[3 * i + p]) / max;

					if (p == 0 || fi->colorFamily == cmRGB)
					{
						//input clip val at this location
						float val = *(float*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes);
						// check whether input pixel has higher value to overcome balloon color and its opacity
						if (val * transparency > bcolor )
						{
							*(float*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes) =
								(val * transparency + bcolor * d->opacity);
						}

						else
						{
							// if not use balloon color itself
							*(float*)(dpt[p] + (yy + yoff[i]) * dpitch[p] + (xx + xoff[i]) * nbytes) = bcolor;
						}
					}

					else //  U V
					{
						if (((yy + yoff[i]) & andH) == 0 && ((xx + xoff[i]) & andW) == 0)
							*(float*)(dpt[p] + ((yy + yoff[i]) >> subH) * dpitch[p] + ((xx + xoff[i]) >> subW) * nbytes)
							= ((float)(d->yuv[p]) - 128)/ max;

					}

				}
			}
		
		}
		
	
		if (d->light == 0)
		{
			vs_aligned_free(ballcolor);
			vs_aligned_free(xoff);
			vs_aligned_free(yoff);
		}

		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC balloonFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    BalloonData* d = (BalloonData*)instanceData;
    vsapi->freeNode(d->node);
	if (d->light != 0)
	{
		vs_aligned_free(d->xoffs);
		vs_aligned_free(d->yoffs);
		vs_aligned_free(d->ballcol);
	}

    free(d);
}

static void VS_CC balloonCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	BalloonData d;
	BalloonData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Balloon: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Balloon: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Balloon: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Balloon: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
	d.radius = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.radius = d.vi->height < d.vi->width ? d.vi->height / 8 : d.vi->width / 8;
	else if (d.radius < 8 || d.radius > d.vi->height / 4 || d.radius > d.vi->width / 4)
	{
		vsapi->setError(out, "Balloon: rad must be 8 to 1/4th of smaller dimension of frame");
		vsapi->freeNode(d.node);
		return;
	}

	d.light = int64ToIntS(vsapi->propGetInt(in, "light", 0, &err));
	if (err)
		d.light = 5;
	else if (d.light < 0 || d.light > 9)
	{
		vsapi->setError(out, "Balloon: light must be 1 to 9 on num key pad  as direction of distant light or 0 for within frame t0 1/4th of smaller dimension of frame");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.light == 0)
	{
		d.lightx = int64ToIntS(vsapi->propGetInt(in, "lx", 0, &err));
		if (err)
			d.lightx = d.vi->width / 2;
		else if (d.lightx < 0 || d.lightx > d.vi->width - 1)
		{
			vsapi->setError(out, "Balloon: lx must be within frame");
			vsapi->freeNode(d.node);
			return;
		}
		d.lighty = int64ToIntS(vsapi->propGetInt(in, "ly", 0, &err));
		if (err)
			d.lighty = 0;
		else if (d.lighty < 0 || d.lighty > d.vi->height - 1)
		{
			vsapi->setError(out, "Balloon: ly must be within frame");
			vsapi->freeNode(d.node);
			return;
		}
	}
	d.offset = (float)vsapi->propGetFloat(in, "offset", 0, &err);
	if (err)
		d.offset = 0.8f;
	else if (d.offset < 0 || d.offset > 1)
	{
		vsapi->setError(out, "Balloon: offset  must be 0 to 1.0");
		vsapi->freeNode(d.node);
		return;
	}
	d.refl = (float)vsapi->propGetFloat(in, "refl", 0, &err);
	if (err)
		d.refl = 0.2f;
	else if (d.refl < 0 || d.refl > 1.0)
	{
		vsapi->setError(out, "Balloon: refl  must be 0 to 1.0");
		vsapi->freeNode(d.node);
		return;
	}
	d.opacity = (float)vsapi->propGetFloat(in, "opacity", 0, &err);
	if (err)
		d.opacity = 0.4f;
	else if (d.opacity < 0 || d.opacity > 1.0f)
	{
		vsapi->setError(out, "Balloon: opacity  must be 0 to 1.0");
		vsapi->freeNode(d.node);
		return;
	}
	int temp[] = { 150, 150, 150 };

	int m = vsapi->propNumElements(in, "color");
	m = m > 3 ? 3 : m;

	for (int i = 0; i < m; i++)
	{
		temp[i] = int64ToIntS(vsapi->propGetInt(in, "color", i, &err));

		if (temp[i] < 0 || temp[i] > 255)
		{
			vsapi->setError(out, "Balloon: color parameter values must be between 0 and 255 ");
			vsapi->freeNode(d.node);
			return;
		}
	}
	for (int i = 0; i < 3; i++)
		d.bgr[i] = temp[2 - i];
	
	d.startx = int64ToIntS(vsapi->propGetInt(in, "sx", 0, &err));
	if (err)
		d.startx = d.radius + 2;
	else if (d.startx < d.radius || d.startx >d.vi->width - d.radius - 1)
	{
		vsapi->setError(out, "Balloon: sx  must be from radius to frame width - radus -1 ");
		vsapi->freeNode(d.node);
		return;
	}
	d.finalx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.finalx = d.vi->width - d.radius - 1;
	else if (d.finalx < 0 || d.finalx >d.vi->width - d.radius - 1)
	{
		vsapi->setError(out, "Balloon: ex  must be from radius to frame width - radus -1 ");
			vsapi->freeNode(d.node);
			return;
	}

	d.nhops = int64ToIntS(vsapi->propGetInt(in, "nhops", 0, &err));
	if (err)
		d.nhops = 1;
	else if (d.nhops < 0 || d.nhops >(d.EndFrame - d.StartFrame) / 8 || 16 * d.nhops > abs(d.finalx - d.startx))
	{
		vsapi->setError(out, "Balloon: nhops  must be 0 to 1/8 of EndFrame - StartFrame, and less than fx - sx / 16 ");
		vsapi->freeNode(d.node);
		return;
	}
	d.rise = int64ToIntS(vsapi->propGetInt(in, "rise", 0, &err));
	if (err)
		d.rise = d.vi->height - d.radius - 1;
	else if (d.rise < d.radius || d.rise >d.vi->height -  1)
	{
		vsapi->setError(out, "Balloon: rise  must be from radius to frame height  -1 ");
			vsapi->freeNode(d.node);
			return;
	}
	d.floory = int64ToIntS(vsapi->propGetInt(in, "fy", 0, &err));
	if (err)
		d.floory = d.vi->height - d.radius;
	else if (d.floory < d.radius || d.floory > d.vi->height + 2 * d.radius  )
	{
		vsapi->setError(out, "Balloon: fy  must be from radius to frame height + 2 * radius ");
		vsapi->freeNode(d.node);
		return;
	}
	

    data = (BalloonData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Balloon", balloonInit, balloonGetFrame, balloonFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Balloon", "Effect balloon ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Balloon", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;color:int[]:opt;opacity:float:opt;"
	"nhops:int:opt;rise:int:opt;sx:int:opt;fx:int:opt;fy:int:opt;light:int:opt;refl:float:opt;offset:float:opt;"
	"lx:int:opt;ly:int:opt;", balloonCreate, 0, plugin);

//	("Balloon", "c[sf]i[ef]i[radius]i[color]i[opacity]i[nhops]i[rise]i[sx]i[fx]i[fy]i[light]s[refl]i[offset]i[lightx]i[lighty]i", Create_Balloon, 0);

}
*/

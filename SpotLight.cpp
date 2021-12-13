/*
SpotLight is a function for vfx, a vapoursynth plugin
Creates a SpotLight Firework on frame 


Author V.C.Mohan.
Date 3 April 2021
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
	int rad;				// radius of spot
	int rgb[3];				// color of spot
	int startx;				// initial x coord	
	int starty;				// initial y coord
	int endx;				// Final x Coord
	int endy;				// Final Y coord
	float dim;				// dimming factor
	uint8_t color[3];	// converted colors
		

} SpotLightData;
/* Defined in colorconvertor.h
#define YUV2R(y, u, v) clamp((298 * ((y)-16) + 409 * ((v)-128) + 128) >> 8)
#define YUV2G(y, u, v) clamp((298 * ((y)-16) - 100 * ((u)-128) - 208 * ((v)-128) + 128) >> 8)
#define YUV2B(y, u, v) clamp((298 * ((y)-16) + 516 * ((u)-128) + 128) >> 8)

#define RGB2Y(r, g, b) (uint8_t)(((66 * (r) + 129 * (g) +  25 * (b) + 128) >> 8) +  16)
#define RGB2U(r, g, b) (uint8_t)(((-38 * (r) - 74 * (g) + 112 * (b) + 128) >> 8) + 128)
#define RGB2V(r, g, b) (uint8_t)(((112 * (r) - 94 * (g) -  18 * (b) + 128) >> 8) + 128)*/

template <typename finc>
void YUVspot(finc** dp, const finc** sp, int * pitch,
	int x, int y, int r, int wd, int ht, int subW, int subH,
	int np, int nbits, uint8_t * sbgr);

template <typename finc>
void YUVspot(finc** dp, const finc** sp, int* pitch,
	int x, int y, int r, int wd, int ht, int subW, int subH,
	int np, int nbits, uint8_t* sbgr)
{
	// x, y, r , wd, ht are values for y plane. 
	// gray will be 0 for y and some value for u, v
	int sx = (VSMIN(VSMAX(x - r, 0), wd - 1));
	int ex = (VSMIN(VSMAX(x + r, 0), wd - 1));

	int sy = (VSMIN(VSMAX(y - r, 0), ht - 1));
	int ey = (VSMIN(VSMAX(y + r, 0), ht - 1));
	uint8_t bgr[3];
	finc yuv[3];
	// initialize pointers

	int rsq = r * r;

	for (int h = sy; h < ey; h++)
	{
		int hsq = (h - y) * (h - y);

		for (int w = sx; w < ex; w++)
		{
			int wsq = (w - x) * (w - x);

			if (hsq + wsq <= rsq)
			{
				// get original colors in yuv
				yuv[0] = *(sp[0] + h * pitch[0] + w);
				for (int p = 1; p < np; p++)
					yuv[p] = *(sp[p] + (h >> subH) * pitch[p] + (w >> subW));

				if (np == 1)
				{
					*(dp[0] + h * pitch[0] + w) = yuv[0];
				}
				else
				{
					YUV_BGR8(bgr, yuv, nbits);

					for (int p = 0; p < np; p++)
					{
						bgr[p] = VSMIN(bgr[p], sbgr[p]);
					}

					BGR8_YUV(yuv, bgr, nbits);

					*(dp[0] + (h ) * pitch[0] + (w )) = yuv[0];
					for (int p = 1; p < np; p++)
					{
						*(dp[p] + (h >> subH) * pitch[p] + (w >> subW)) = yuv[p];

					}
					
				}
			}
		}
	}
}


static void VS_CC spotlightInit(VSMap* in, VSMap* out, void** instanceData,
	VSNode* node, VSCore* core, const VSAPI* vsapi)
{
	SpotLightData* d = (SpotLightData*)*instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);
	const VSFormat* fi = d->vi->format;

	
	for (int i = 0; i < 3; i++)
	{
		// put in bgr order
		d->color[2 - i] = d->rgb[i];
			
	}
}
//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC spotlightGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SpotLightData* d = (SpotLightData*)*instanceData;

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
						// now create spotlight		
		int xcoord = d->startx + (n * (d->endx - d->startx)) / (nframes);
		int ycoord = d->starty + (n * (d->endy - d->starty)) / (nframes);
		
		
		for (int p = 0; p < np; p++)
		{
			if (fi->sampleType == stInteger && nbytes == 1)
			{
				unsigned char* dpp = dp[p];
				const unsigned char* spp = sp[p];
				if (fi->colorFamily == cmRGB)
				{
					dimplaneRGB(dpp, spp, pitch[p],
						wd, ht, d->dim);
					RGBspotLight(dpp, spp, pitch[p],
						xcoord, ycoord, d->rad, wd, ht, d->color[p]);
				}
				else
				{
					uint8_t limit = 16;

					if (p == 0)
					{
						dimplaneYUV(dpp, spp, pitch[p],
							wd, ht, d->dim, limit);

						YUVspot(dp, sp, pitch,
							xcoord, ycoord, d->rad, wd, ht, subW[1], subH[1],
							np, nbits, d->color);
					}
				}
			}

			else if (fi->sampleType == stInteger && nbytes == 2)
			{
				const uint16_t* spp = (uint16_t*)(sp[p]);
				uint16_t* dpp = (uint16_t*)(dp[p]);
				

				if (fi->colorFamily == cmRGB)
				{
					dimplaneRGB(dpp, spp, pitch[p],
						wd, ht, d->dim);
					RGBspotLight(dpp, spp, pitch[p],
						xcoord, ycoord, d->rad, wd, ht, (uint16_t) ((d->color[p]) << (nbits - 8)) );
				}
				else
				{
					uint16_t limit = 16 << (nbits - 8);

					if (p == 0)
					{
						dimplaneYUV(dpp, spp, pitch[p],
							wd, ht, d->dim, limit);

						YUVspot((uint16_t**)dp, (const uint16_t**)sp, pitch,
							xcoord, ycoord, d->rad, wd, ht, subW[1], subH[1],
							np, nbits, d->color);
					}
				}
			}

			else if (fi->sampleType == stFloat && nbytes == 4)
			{
				float* dpp = (float*)(dp[p]);
				const float* spp = (float*)(sp[p]);

				

				if (fi->colorFamily == cmRGB)
				{
					dimplaneRGB(dpp, spp, pitch[p],
						wd, ht, d->dim);
					RGBspotLight(dpp, spp, pitch[p],
						xcoord, ycoord, d->rad, wd, ht, (float)(d->color[p] / 255.0f) );
				}
				else
				{
					float limit = 0.0;

					if (p == 0)
					{
						dimplaneYUV(dpp, spp, pitch[p],
							wd, ht, d->dim, limit);

						YUVspot((float**)dp, (const float**)sp, pitch,
							xcoord, ycoord, d->rad, wd, ht, subW[1], subH[1],
							np, nbits, d->color);
					}
				}

			}
		}
		
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC spotlightFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SpotLightData* d = (SpotLightData*)instanceData;
    vsapi->freeNode(d->node);	
	
    free(d);
}

static void VS_CC spotlightCreate(const VSMap* in, VSMap* out, void* userData,
									VSCore* core, const VSAPI* vsapi)
{
	SpotLightData d;
	SpotLightData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "SpotLight: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "SpotLight: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "SpotLight: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "SpotLight: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
		
	d.rad = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.rad = 64 > d.vi->width / 8 ? d.vi->width / 8: 64;
	else if (d.rad < 4 || d.rad > d.vi->width / 4)
	{
		vsapi->setError(out, "SpotLight:  rad can be 4 to frame width / 4");
		vsapi->freeNode(d.node);
		return;
	}
	d.startx = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.startx = d.vi->width / 2; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.startx < 0 || d.startx > d.vi->width )
	{
		vsapi->setError(out, "SpotLight:  x can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.starty = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.starty = d.vi->height / 2;
	else if (d.starty < 0 || d.starty > d.vi->height)
	{
		vsapi->setError(out, "SpotLight:  y can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}

	d.endx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.endx = d.startx; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.endx < 0 || d.endx > d.vi->width)
	{
		vsapi->setError(out, "SpotLight:  ex can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.endy = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.endy = d.starty; //
	else if (d.endy < 0 || d.endy > d.vi->height)
	{
		vsapi->setError(out, "SpotLight:  ey can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}
	int temp = vsapi->propNumElements(in, "rgb");
	if (temp == 0)
	{
		d.rgb[0] = 255;
		d.rgb[1] = 255;
		d.rgb[2] = 255;
	}
	else if (temp > 3)
	{
		vsapi->setError(out, "SpotLight: array rgb can have a maximum of 3 values");
		vsapi->freeNode(d.node);
		return;
	}
	else
	{

		for (int i = 0; i < temp; i++)
		{
			d.rgb[i] = int64ToIntS(vsapi->propGetInt(in, "rgb", i, &err));

			if (d.rgb[i] < 0 || d.rgb[i] > 255)
			{
				vsapi->setError(out, "SpotLight:  elements of array rgb must be 0 to 255 only");
				vsapi->freeNode(d.node);
				return;
			}
		}
		for (int i = temp; i < 3; i++)
		{
			d.rgb[i] = d.rgb[i - 1];
		}
	}
	d.dim = (float)vsapi->propGetFloat(in, "dim", 0, &err);
	if (err)
		d.dim = 0.20f;
	if ( d.dim < 0.1f || d.dim > 0.5f)
	{
		vsapi->setError(out, "SpotLight:  dim value must be 0.1 to 0.5 only");
		vsapi->freeNode(d.node);
		return;
	}

    data = (SpotLightData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "SpotLight", spotlightInit, spotlightGetFrame, spotlightFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "SpotLight", "Effect spotlight ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("SpotLight", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;"
	"x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;rgb:int[]:opt;dim:float:opt", spotlightCreate, 0, plugin);
	
}
*/

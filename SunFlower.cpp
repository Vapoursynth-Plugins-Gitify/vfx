/*
SunFlower is a function for vfx, a vapoursynth plugin
Creates a SunFlower Firework on frame 


Author V.C.Mohan.
Date 7 April 2021
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
	int startx;
	int starty;
	int radmax;		// max radius to which the beams expand
	bool color;
	int endx;
	int endy;
	float grav, persist;

	uint8_t* rayColors;
} SunFlowerData;



template <typename finc>
void drawRaysSunFlower(finc** dp, int* dpitch, int* subW, int* subH,
	int radius1, int radius2, int radmax, int x, int y, int wd, int ht,
	float alfa, float gravity, int np, finc* col, bool embers);

	
//--------------------------------------------------------------------------------------------

template <typename finc>
void drawRaysSunFlower(finc** dp, int* dpitch, int* subW, int* subH,
	int radius1, int radius2, int radmax, int x, int y, int wd, int ht,
	float alfa, float gravity, int np, finc * col, bool embers)
	
{
	
	for (int r = radius1; r < radius2; r++)
	{
		int rmod = (radius2 - radius1) / 3;

		if (embers && (r % rmod) != 0) continue;	// (rand() % 20) < 16) continue;

		int newx = (int)(r * cos(alfa) + 0.5);

		if (newx + x <= 0 || newx + x >= wd - 2)	// we will make line thick
			continue;

		int newy = (int)((r * sin(alfa) + 0.5) + (gravity * r * r) / radmax);

		if (newy + y <= 1 || newy + y >= ht - 2)	// we will make line thick
			continue;

		for (int p = 0; p < np; p++)
		{
			for ( int i = 0; i < 2; i ++)
			{
				for (int j = 0; j < 2; j++)
				{
					// draw a thick line
					*(dp[p] + ((y + newy + i) >> subH[p]) * dpitch[p] 
						+ ((x + newx + j) >> subW[p]))
						= col[p];
				}				
			}
		}
	}
}

static void VS_CC sunflowerInit(VSMap* in, VSMap* out, void** instanceData,
	VSNode* node, VSCore* core, const VSAPI* vsapi)
{
	SunFlowerData* d = (SunFlowerData*)*instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);
	const VSFormat* fi = d->vi->format;

	d->rayColors = (uint8_t*)vs_aligned_malloc(sizeof(float) * 8 * 3, 32);
	
	uint8_t bgr[3] = { 0,0,0 };
	uint8_t yuv[3] = { 0,0,0 };

	// 0 th will be gray. 7th will be white. in between all colors
	for (int i = 0; i < 8; i ++ )
	{		
		bgr[0] = (i & 1) * 128 + 127;
		bgr[1] = (i & 2) * 64 + 127;
		bgr[2] = (i & 4) * 32 + 127;

		BGR8_YUV(yuv, bgr, 8 );

		for ( int x = 0; x < 3; x ++)
		{
			if (fi->sampleType == stInteger && fi->bytesPerSample == 1)
			{
				if (fi->colorFamily == cmRGB)
					d->rayColors[3 * i + x] = bgr[x];
				else
					d->rayColors[3 * i + x] =yuv[x];
			}

			else if (fi->sampleType == stInteger && fi->bytesPerSample == 2)
			{
				if (fi->colorFamily == cmRGB)
					*((uint16_t*)d->rayColors + 3 * i + x) = (uint16_t)(bgr[x] << (fi->bitsPerSample - 8));
				else
					*((uint16_t*)d->rayColors + 3 * i + x) = (uint16_t)(yuv[x] << (fi->bitsPerSample - 8));
			}		

			else if (fi->sampleType == stFloat && fi->bytesPerSample == 4)
			{
				if (fi->colorFamily == cmRGB)
					*((float*)d->rayColors + 3 * i + x) = (float)(bgr[x] / 255.0f);
				else
				{
					if (x == 0)
						*((float*)d->rayColors + 3 * i + x) = (float)(yuv[x] / 255.0f);
					else
						*((float*)d->rayColors + 3 * i + x) = (float)(yuv[x] - 127.0f) / 255.0f;
				}
			}

		}
	}

}
//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC sunflowerGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    SunFlowerData* d = (SunFlowerData*)*instanceData;

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
						// now create sunflower		
		int perception = (int)((d->vi->fpsNum / d->vi->fpsDen) * d->persist); // number of frames vision retention .ray continues


		float len = (d->radmax) / (3.0f * nframes / 4.0f); //increase per frame reaches max by 3/4 length of clip
		float lsmoke = d->radmax / (2.0f * nframes);	//increase per frame. smoke continues expansion till end of clip
		int lray = (5 * d->radmax) / 6;	//long rays. beyond this embers
		float slen = (len / 2);	// short ray length. per frame
		int rsmoke = (int)(n * lsmoke > d->radmax? d->radmax : n * lsmoke);
		int lraystart = (int)(n * len);
		int lrayend = (int)(lraystart + perception * len);

		int sraystart = (int)(n * slen);
		int srayend = (int)(sraystart + perception * slen);
		// gravity increases fall rate
		float gravity = d->grav * n / (d->vi->fpsNum / d->vi->fpsDen);	// arbitrarily arrived at a value of 0.25
		float gravity2 = 4 * gravity;
		// center coordinates of sunflower and smoke
		int xcoord = d->startx + (n * (d->endx - d->startx)) / nframes;
		int ycoord = d->starty + (n * (d->endy - d->starty)) / nframes;
		uint8_t smoke[3];

		// generate smoke
		for (int h = ycoord - rsmoke; h < ycoord + rsmoke; h++)
		{
			if (h > 2 && h < ht - 2)
			{
				int hsq = (h - ycoord) * (h - ycoord);

				for (int w = xcoord - rsmoke; w < xcoord + rsmoke; w++)
				{
					if (w > 2 && w < wd - 2)
					{
						int delta = rsmoke - rand() % (2 + rsmoke / 4);	// to create hazy boundary

						if ((w - xcoord) * (w - xcoord) + (hsq) <= delta * delta)
						{
							if (fi->colorFamily == cmYUV)
							{
								smoke[0] = (rand() % 60) + 60;
								smoke[1] = 127;
								smoke[2] = 127;
							}
							else //RGB
							{
								smoke[0] = (rand() % 60) + 60;
								smoke[1] = smoke[0];
								smoke[2] = smoke[0];
							}

							for (int p = 0; p < np; p++)
							{
								if (fi->sampleType == stInteger && nbytes == 1)
								{
									unsigned char* dpp = dp[p];
									uint8_t gray = smoke[p];
									*(dpp + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = gray;
								}

								else if (fi->sampleType == stInteger && nbytes == 2)
								{
									uint16_t* dpp = (uint16_t*)(dp[p]);
									uint16_t gray = smoke[p] << (nbits - 8);
									*(dpp + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = gray;
								}

								else if (fi->sampleType == stFloat && nbytes == 4)
								{
									float* dpp = (float *)(dp[p]);
									float gray = p == 0 ? smoke[p] / 255.0f : (smoke[p] - 128) / 255.0f;
									
									*(dpp + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = gray;
								}
							}
						}
					}
				}
			}
		}
		// create sunflower with 18 long and 18 short rays
		for (int i = 0; i < 36; i++)
		{
			int lmod = (int)(4 * perception * len);
			int smod = (int)(4 * perception * slen);

			bool embers1 = lraystart > d->radmax - rand() % lmod ? true : false;
			bool embers2 = sraystart > d->radmax / 2 - rand() % smod ? true : false;

			float alfa = (float)((M_PI * i * 10) / 180.0f);
			float alfa2 = (float)(M_PI * ((35 - i) * 10 + 5.0f)) / 180.0f;			

			if (fi->sampleType == stInteger && nbytes == 1)
			{
				// As our color array 0th is grey and 1 to 6 are colors and 7 is white
				uint8_t* rayCol = d->rayColors + 3 * (i % 7) + 3;
				if (!d->color)
					rayCol = d->rayColors + 21;

				// long rays	
				drawRaysSunFlower(dp, pitch, subW, subH, lraystart, lrayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa, gravity, np, rayCol, embers1);
				// short rays
				drawRaysSunFlower(dp, pitch, subW, subH, sraystart, srayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa2, gravity2, np, rayCol, embers2);
			}
			else if (fi->sampleType == stInteger && nbytes == 2)
			{
				// As our color array 0th is grey and 1 to 6 are colors and 7 is white
				uint16_t* rayCol = (uint16_t*)d->rayColors + 3 * (i % 7) + 3;
				uint16_t** dpp = (uint16_t**)dp;
				if (!d->color)
					rayCol = (uint16_t*)d->rayColors + 21;

				// long rays	
				drawRaysSunFlower(dpp, pitch, subW, subH, lraystart, lrayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa, gravity, np, rayCol, embers1);
				// short rays
				drawRaysSunFlower(dpp, pitch, subW, subH, sraystart, srayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa2, gravity2, np, rayCol, embers2);
			}
			else if (fi->sampleType == stFloat && nbytes == 4)
			{
				// As our color array 0th is grey and 1 to 6 are colors and 7 is white
				float* rayCol = (float*)d->rayColors + 3 * (i % 7) + 3;
				float** dpp = (float**)dp;
				if (!d->color)
					rayCol = (float*)d->rayColors + 21;

				// long rays	
				drawRaysSunFlower(dpp, pitch, subW, subH, lraystart, lrayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa, gravity, np, rayCol, embers1);
				// short rays
				drawRaysSunFlower(dpp, pitch, subW, subH, sraystart, srayend, d->radmax,
					xcoord, ycoord, wd, ht, alfa2, gravity2, np, rayCol, embers2);
			}

		}			
		
		vsapi->freeFrame( src);
		return (dst);
    }

    return 0;
}

static void VS_CC sunflowerFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    SunFlowerData* d = (SunFlowerData*)instanceData;
    vsapi->freeNode(d->node);	
	vs_aligned_free(d->rayColors);
    free(d);
}

static void VS_CC sunflowerCreate(const VSMap* in, VSMap* out, void* userData,
									VSCore* core, const VSAPI* vsapi)
{
	SunFlowerData d;
	SunFlowerData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "SunFlower: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "SunFlower: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "SunFlower: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "SunFlower: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
		
	d.radmax = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
	if (err)
		d.radmax = d.vi->height / 4 ;
	else if (d.radmax < 64 || d.radmax > d.vi->width || d.radmax > d.vi->height)
	{
		vsapi->setError(out, "SunFlower:  rad can be 64 to frame width or height");
		vsapi->freeNode(d.node);
		return;
	}
	d.startx = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
	if (err)
		d.startx = d.vi->width / 2; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.startx < 0 || d.startx > d.vi->width )
	{
		vsapi->setError(out, "SunFlower:  x can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.starty = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
	if (err)
		d.starty = d.vi->height / 4;
	else if (d.starty < 0 || d.starty > d.vi->height)
	{
		vsapi->setError(out, "SunFlower:  y can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}

	d.endx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
	if (err)
		d.endx = d.startx; // 64 > d.vi->width / 8 ? d.vi->width / 8 : 64;
	else if (d.endx < 0 || d.endx > d.vi->width)
	{
		vsapi->setError(out, "SunFlower:  ex can be 0 to frame width");
		vsapi->freeNode(d.node);
		return;
	}
	d.endy = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
	if (err)
		d.endy = d.starty; //
	else if (d.endy < 0 || d.endy > d.vi->height)
	{
		vsapi->setError(out, "SunFlower:  ey can be 0 to frame height");
		vsapi->freeNode(d.node);
		return;
	}
	
	d.grav = (float)vsapi->propGetFloat(in, "gravity", 0, &err);

	if (err)
		d.grav = 0.5f;

	if (d.grav < 0 || d.grav > 2.0)
	{
		vsapi->setError(out, "SunFlower:   gravity must be 0 to 2.0");
		vsapi->freeNode(d.node);
		return;
	}
	d.persist = (float)vsapi->propGetFloat(in, "persistance", 0, &err);

	if (err)
		d.persist = 0.5f;

	if ( d.persist * d.vi->fpsNum / d.vi->fpsDen < 2 || d.persist * d.vi->fpsNum / d.vi->fpsDen  > d.EndFrame - d.StartFrame)
	{
		vsapi->setError(out, "SunFlower:  persistance must result in atleast 2 frames and not more than duration of effect");
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
	
    data = (SunFlowerData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "SunFlower", sunflowerInit, sunflowerGetFrame, sunflowerFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init waveLength appears to be reserved for python. so using wavelen
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "SunFlower", "Effect sunflower ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("SunFlower", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;x:int:opt;y:int:opt;"
	"ex:int:opt;ey:int:opt;color:int:opt;gravity:float:opt;persistance:float:opt;", sunflowerCreate, 0, plugin);
	
}
*/

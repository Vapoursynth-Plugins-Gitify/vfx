/*
Rain is a function for vfx, a vapoursynth plugin
Creates a Rain firework 


Author V.C.Mohan.
Date 21 Mar 2021
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
	int type;	//Light, medium, heavy Rain
	int etype;
	int slant;	// direction of slant 1 to left, 2 vertical, 3 to right
	int eslant;
	float opq;	// how much  rain drops are opaque
	int box;
	unsigned char col[3];
	int span;
	int nwBox, boxw;
	int nhBox, boxh;

} RainData;


static void VS_CC rainInit(VSMap* in, VSMap* out, void** instanceData, 
		VSNode* node, VSCore* core, const VSAPI* vsapi) {
    RainData* d = (RainData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
	unsigned char bgr[3] = { (uint8_t)(255 * d->opq), 
	(uint8_t)(255 * d->opq) , (uint8_t)(255 * d->opq) };
	unsigned char yuv[3];
	
	BGR8YUV(yuv, bgr);
	for (int i = 0; i < 3; i++)
	{
		if (d->vi->format->colorFamily == cmRGB)

			d->col[i] = bgr[i];
		else
			d->col[i] = yuv[i];
	}
	// calculate number of boxes and their width, height
	int temp = d->vi->width / d->box;
	if (temp == 0)
		temp++;	
	d->boxw = d->vi->width / temp;
	d->nwBox = temp;

	temp = d->vi->height / d->box;
	if (temp == 0)
		temp++;
	d->boxh = d->vi->height / temp;
	d->nhBox = temp;
	
}
//------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------
static const VSFrameRef* VS_CC rainGetFrame(int in, int activationReason, void** instanceData,
					void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    RainData* d = (RainData*)*instanceData;

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
		float cspan = 0.0; // ratio of Horizontal to vertical travel  of rain drop
		// calculate current coordinates, width and height
		if (d->slant == 1 )
		{
			if (d->eslant == 1) //
				cspan = -0.5f;
			else if (d->eslant == 2)
				cspan = -0.5f + 0.5f * (float)n / nframes;
			else
				cspan = -0.5f + (float)n / nframes;

		}
		else if (d->slant == 2)
		{
			if (d->eslant == 1)//
				cspan = -0.5f * (float)n / nframes;
			else if (d->eslant == 2)
				cspan = 0.0f;
			else
				cspan = 0.5f * (float)n / nframes;

		}
		else if (d->slant == 3)
		{
			if (d->eslant == 1)//
				cspan = 0.5f - (float) n / nframes;
			else if (d->eslant == 2)
				cspan = 0.5f - 0.5f * (float)n / nframes;
			else
				cspan = 0.5f ;

		}
		
		srand(((n + 101) * (n + 203) * (n + 307)) % (1 << 15));
		
		int yspan =  d->span;	// length of rain streak along Y
		int xspan = (int)(yspan * cspan);// length of rain streak along X
		int absxspan = abs((int)(yspan * cspan));	
		int ndrops;	// this decides number of rain drops imaged in frame
		int light = 1 << 6, medium = 1 << 7, heavy = 1 << 8;
		if (d->type == 1)
		{
			if (d->etype == 1)
				ndrops = light;	// const fewer drops
			else if (d->etype == 2)
				ndrops = light + (n * (medium - light)) / nframes;// increase to medium
			else if (d->etype == 3)
				ndrops = light + (n * (heavy - light)) / nframes; // increase to heavy
		}

		else if (d->type == 2)
		{
			if (d->etype == 1)// decrease fewer drops
				ndrops = medium + (n * (light - medium)) / nframes;
			else if (d->etype == 2)
					// const medium					
				ndrops = medium; 
			else if (d->etype == 3)
					// increase to heavy
				ndrops = medium + (n * (heavy - medium)) / nframes; 
		}

		else if (d->type == 3)
		{
			if (d->etype == 1)
				// decrease fewer drops
				ndrops = heavy + (n * (light - heavy)) / nframes;	
			else if (d->etype == 2)
					// decrese to medium
					ndrops = heavy + (n * (medium - heavy)) / nframes;
			else if (d->etype == 3)
						// const  heavy
					ndrops = heavy; 
		}

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
						// now create rain
		
		for (int i = 0; i < ndrops; i++)
		{
			int droph = rand() % (d->boxh );
			int dropw = rand() % (d->boxw);

			for (int nhb = 0; nhb < d->nhBox; nhb++)
			{
				int hoffset = nhb * d->boxh + droph;

				for (int nwb = 0; nwb < d->nwBox; nwb++)
				{
					int woffset = nwb * d->boxw + dropw;

					for (int y = 0; y < yspan; y++)
					{
						int x = (int)(y * cspan); //  wspan[y];
						int cy = (hoffset + y) % ht;
						int cx = (woffset + x) % wd;
						if (cx < 0) continue;

						for (int p = 0; p < np; p++)
						{
							if (fi->sampleType == stInteger && nbytes == 1)
							{
								if (p == 0 || fi->colorFamily == cmRGB)
								{
									uint8_t lcol = *(sp[p] + (cy)*pitch[p] + (cx));
									*(dp[p] + (cy)*pitch[p] + (cx))
										= lcol > d->col[p] ? (lcol + d->col[p]) / 2 : d->col[p];
									// adjacent pixel
									lcol = *(sp[p] + (cy)*pitch[p] + (cx));
									*(dp[p] + (cy)*pitch[p] + (cx))
										= lcol > d->col[p] ? (lcol + d->col[p]) / 2 : d->col[p];
								}

								else
								{
									// yuv U and V planes
									if (((cx)&andW) == 0 && ((cy)&andH) == 0)
									{
										uint8_t lcol = *(sp[p] + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]));
										*(dp[p] + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]))
											= (lcol + d->col[p]) / 2;

									}

									if (((cx + 1) & andW) == 0 && ((cy) & andH) == 0)
									{
										uint8_t lcol = *(sp[p] + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]));
										*(dp[p] + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]))
											= (lcol + d->col[p]) / 2;
									}

								}
							}

							else if (fi->sampleType == stInteger && nbytes == 2)
							{
								if (p == 0 || fi->colorFamily == cmRGB)
								{
									uint16_t lcol = *((uint16_t*)(sp[p]) + (cy)*pitch[p] + (cx));
									uint16_t rcol = (uint16_t)((int)d->col[p] << (nbits - 8));
									*( (uint16_t*)(dp[p]) + (cy)*pitch[p] + (cx))
										= lcol > rcol ? (lcol + rcol) / 2 : rcol;
									// adjacent pixel
									lcol = *((uint16_t*)(sp[p]) + (cy)*pitch[p] + (cx));
									*((uint16_t*)(dp[p]) + (cy)*pitch[p] + (cx))
										= lcol > rcol ? (lcol + rcol) / 2 : rcol;
								}

								else
								{
									// yuv U and V planes
									if (((cx) & andW) == 0 && ((cy) & andH) == 0)
									{
										uint16_t lcol = *((uint16_t*)(sp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]));
										uint16_t rcol = (uint16_t)((int)d->col[p] << (nbits - 8));
										*((uint16_t*)(dp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]))
											= (lcol + rcol) / 2;

									}

									if (((cx + 1) & andW) == 0 && ((cy)&andH) == 0)
									{
										uint16_t lcol = *((uint16_t*)(sp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]));
										uint16_t rcol = (uint16_t)((int)d->col[p] << (nbits - 8));
										*((uint16_t*)(dp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]))
											= (lcol + rcol) / 2;
									}

								}
							}

							else if (fi->sampleType == stFloat && nbytes == 4)
							{
								if (p == 0 || fi->colorFamily == cmRGB)
								{
									float lcol = *((float*)(sp[p]) + (cy)*pitch[p] + (cx));
									float rcol = (float)(d->col[p]/ 256.0f);
									*((float*)(dp[p]) + (cy)*pitch[p] + (cx))
										= lcol > rcol ? (lcol + rcol) / 2 : rcol;
									// adjacent pixel
									lcol = *((float*)(sp[p]) + (cy)*pitch[p] + (cx));
									*((float*)(dp[p]) + (cy)*pitch[p] + (cx))
										= lcol > rcol ? (lcol + rcol) / 2 : rcol;
								}

								else
								{
									// yuv U and V planes
									if (((cx)&andW) == 0 && ((cy)&andH) == 0)
									{
										float lcol = *((float*)(sp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]));
										float rcol = (float)(d->col[p]-128) / 256.0f;
										*((float*)(dp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx) >> subW[p]))
											= (lcol + rcol) / 2;

									}

									if (((cx + 1) & andW) == 0 && ((cy)&andH) == 0)
									{
										float lcol = *((float*)(sp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]));
										float rcol = (float)(d->col[p] - 128) / 256.0f;
										*((float*)(dp[p]) + ((cy) >> subH[p]) * pitch[p]
											+ ((cx + 1) >> subW[p]))
											= (lcol + rcol) / 2;
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

static void VS_CC rainFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    RainData* d = (RainData*)instanceData;
    vsapi->freeNode(d->node);	
		
    free(d);
}

static void VS_CC rainCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
	RainData d;
	RainData* data;
	int err;

	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	if (d.vi->format->colorFamily != cmRGB && d.vi->format->colorFamily != cmYUV
		&& d.vi->format->colorFamily != cmGray)
	{
		vsapi->setError(out, "Rain: RGB, YUV and Gray format input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	if (d.vi->format->sampleType != stInteger && d.vi->format->sampleType != stFloat)
	{
		vsapi->setError(out, "Rain: 8 to 16 bit integer and 32 bit float input only is supported");
		vsapi->freeNode(d.node);
		return;
	}
	d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
	if (err)
		d.StartFrame = 0;
	else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
	{
		vsapi->setError(out, "Rain: sf must be within video");
		vsapi->freeNode(d.node);
		return;
	}
	d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
	if (err)
		d.EndFrame = d.vi->numFrames - 1;
	else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
	{
		vsapi->setError(out, "Rain: ef must be within video and greater than sf");
		vsapi->freeNode(d.node);
		return;
	}
	d.type = int64ToIntS(vsapi->propGetInt(in, "type", 0, &err));
	if (err)
		d.type = 2;
	if (d.type < 1 || d.type > 3)
	{
		vsapi->setError(out, "Rain: type  can have values 1, 2, 3 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.etype = int64ToIntS(vsapi->propGetInt(in, "etype", 0, &err));
	if (err)
		d.etype = d.type;
	if (d.etype < 1 || d.etype > 3)
	{
		vsapi->setError(out, "Rain: eslant  can have values 1, 2, 3 only");
		vsapi->freeNode(d.node);
		return;
	}

	
	d.slant = int64ToIntS(vsapi->propGetInt(in, "slant", 0, &err));
	if (err)
		d.slant = 2;
	if (d.slant < 1 || d.slant > 3)
	{
		vsapi->setError(out, "Rain: slant  can have values 1, 2, 3 only");
		vsapi->freeNode(d.node);
		return;
	}
	d.eslant = int64ToIntS(vsapi->propGetInt(in, "eslant", 0, &err));
	if (err)
		d.eslant = d.slant;
	if (d.eslant < 1 || d.eslant > 3)
	{
		vsapi->setError(out, "Rain: eslant  can have values 1, 2, 3 only");
		vsapi->freeNode(d.node);
		return;
	}	

	

	d.opq = (float)vsapi->propGetInt(in, "opq", 0, &err);
	if (err)
		d.opq = 0.5;
	else if (d.opq < 0.1f || d.opq > 1.0f)
	{
		vsapi->setError(out, "Rain: opq must be in range  0.1 to 1.0");
		vsapi->freeNode(d.node);
		return;
	}

	d.box = int64ToIntS(vsapi->propGetInt(in, "box", 0, &err));
	if (err)
		d.box = 200;
	else if (d.box < 100 || d.box > 400)
	{
		vsapi->setError(out, "Rain: box must be in range  100 to 400");
		vsapi->freeNode(d.node);
		return;
	}

	d.span = int64ToIntS(vsapi->propGetInt(in, "span", 0, &err));
	if (err)
		d.span = 16;
	else if (d.span <4 || d.span > 64)
	{
		vsapi->setError(out, "Rain: span must be in range  4 to 64");
		vsapi->freeNode(d.node);
		return;
	}
	
	
    data = (RainData*)malloc(sizeof(d));
    *data = d;	

    vsapi->createFilter(in, out, "Rain", rainInit, rainGetFrame, rainFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.effects.vxf", "Rain", "Effect rain ", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Rain", "clip:clip;sf:int:opt;ef:int:opt;type:int:opt;etype:int:opt;"
				"slant:int:opt;eslant:int:opt;opq:float:opt;box:int:opt;span:int:opt;", rainCreate, 0, plugin);
	
}
*/

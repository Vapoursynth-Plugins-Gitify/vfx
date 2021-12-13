/******************************************************************************
RearViewMirror filter plugin for Vapoursynth+ 32/64 bit version
RearViewMirror corrects input image recorded as  circular (fishEye) projection
to a rectangular image
Fish eye correction as in the paper "A Flexible Architecture for RearViewMirror
Correction in Automotive Rear-View Cameras" of ALTERA Manipal DOT NET of 2008
Material from Wikipedia used for barrel and pincushion corrections
 Thread agnostic (operates under multi thread mode)
 Author V.C.Mohan

 16 oct 2021
Copyright (C) <2021>  <V.C.Mohan>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, version 3 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	A copy of the GNU General Public License is at
	see <http://www.gnu.org/licenses/>.

	For details of how to contact author see <http://www.avisynth.nl/users/vcmohan/vcmohan.html>


********************************************************************************/

//#include "windows.h"
//#include <stdint.h>const finc* sp,
// #define _USE_MATH_DEFINES
//#include "math.h"
//#include "InterpolationPack.h"
//
//#include "VapourSynth.h"
//

//-------------------------------------------------------------------------
typedef struct {
	VSNodeRef* node;
	VSNodeRef* bnode;
	const VSVideoInfo* vi;
	//VSVideoInfo vi;

	// starting or constant for x and y or for x alone coefficients
	int mcy;
	int mcx;
	bool test;	// whether a test run

	int dots;	// dot density 1 for 16, 2 for 12, 3 for 8, 4 for 4.
	int method;
	float dim;
	double fov;
	double cvx;
	int frad;
	int mwd;
	int mht;
	int borderWidth;
	int fborder;
	int iRadius;
	int q;		// type of interpolation. 0 near pt, 1 ninept 2x2, 2 bilenear 2x2, 3 cubic 3x3, 4 lanczos 6x6
	bool oval;	// is squaring circle required?
	float* iCoeff;
	int ddensity;	// pixel interval of dots of 
	int quantile;	// Accuracy of fraction to which values are interpolated
	int span;		// interpolation function 1d span or taps
	unsigned char col[16];		// color components for fill
	int* xyAndQ;
	double rNorm;
	int nEntries;
} RearViewMirrorData;


/*--------------------------------------------------
 * The following is the implementation
 * of the defined functions.
 --------------------------------------------------*/
 //Here is the acutal constructor code used

static void VS_CC rearviewmirrorInit(VSMap* in, VSMap* out, void** instanceData,
	VSNode* node, VSCore* core, const VSAPI* vsapi)
{
	RearViewMirrorData* d = (RearViewMirrorData*)*instanceData;
	vsapi->setVideoInfo(d->vi, 1, node);

	int sMajor = d->mwd / 2;
	int sMinor = d->mht / 2;

	if (d->oval)
	{
		d->frad = (d->mwd > d->mht ? d->mwd : d->mht) / 2;
		sMajor = d->mwd / 2;
		sMinor = d->mht / 2;
	}
	else
		d->frad = (int)sqrt((double)(d->mwd * d->mwd + d->mht * d->mht)) / 2;


	double focal = getFocalLength(d->frad, d->method + 5, d->fov);

	d->iRadius = getOutputRadius(d->frad, focal, d->cvx);
	
	// input frame dimensions
	int swidth = d->vi->width;
	int sheight = d->vi->height;
	
	// output
	
	if ( swidth / 2 <= d->iRadius || sheight / 2 <= d->iRadius)
	{
		vsapi->setError(out, "RearViewMirror: Frame dimensions are less than needed. Alter frad and or fov");
		vsapi->freeNode(d->node);
		vsapi->freeNode(d->bnode);
		return;
	}

	const VSFormat* fi = d->vi->format;
	int nbytes = fi->bytesPerSample;
	int nbits = fi->bitsPerSample;
	d->quantile = 64;
	d->nEntries = d->test ? 2 : d->q == 1 ? 3 : 4;
	d->fborder = d->frad + d->borderWidth;
	//double rNorm = (double)d->iRadius; // dummy value. not used
	d->xyAndQ = (int*)vs_aligned_malloc<int>(sizeof(int) * d->fborder * d->fborder * d->nEntries, 32);

	int* xyQ = d->xyAndQ;
	float xy[2];
	int x, y, qx, qy;

	d->iCoeff = NULL;

	if (!d->test)
		d->iCoeff = setInterpolationScheme(d->q, d->quantile, &d->span);
	int count = 0;
	int hoff, woff;
	bool insideFrame = false;
	bool onMirror = false;
	bool inBorder = false;
	float sMJsq = (float)sMajor * sMajor;
	float sMJBsq = (float)(sMajor + d->borderWidth) * (sMajor + d->borderWidth);
	float sMNsq = (float)sMinor * sMinor;
	float sMNBsq = (float)(sMinor + d->borderWidth) * (sMinor + d->borderWidth);
	float absq = sMNsq * sMJsq;
	float abbsq = sMNBsq * sMJBsq;

	int mwdB = sMajor + d->borderWidth;
	int mhtB = sMinor + d->borderWidth;

	for (int h = 0; h < d->fborder; h++)
	{
		hoff = h * d->fborder * d->nEntries;

		for (int w = 0; w < d->fborder; w++)
		{
			onMirror = false;
			inBorder = false;
			insideFrame = false;

			woff = d->nEntries * w;
			if (d->oval)
			{

				if ((w * w) * sMNsq + (h * h) * sMJsq <= absq)
				{
					onMirror = true;
					inBorder = false;
				}

				else if ((w * w) * sMNBsq + (h * h) * sMJBsq <= abbsq)
				{
					inBorder = true;
					onMirror = false;
					insideFrame = false;
				}

			}
			else
			{
				// rectangle
				if (w <= d->mwd / 2 && h <= d->mht / 2)
				{
					onMirror = true;
				}
				else if (w <= mwdB && h <= mhtB)
					inBorder = true;

			}

			if (onMirror)
			{
				insideFrame = false;
				getSourceXY(xy, (float)w, (float)h, d->method + 5, focal,focal, d->cvx);

				x = (int)floor(xy[0]);
				y = (int)floor(xy[1]);

				if (x < swidth / 2 && y < sheight / 2 && x >= 0 && y >= 0)
				{
					insideFrame = true;

					if (insideFrame)
					{
						// calculate nearest quantile of the fraction
						qx = (int)((xy[0] - x) * d->quantile);
						qy = (int)((xy[1] - y) * d->quantile);

						d->xyAndQ[hoff + woff] = x;
						d->xyAndQ[hoff + woff + 1] = y;

						if (!d->test)
						{
							if (d->q > 1)
							{
								d->xyAndQ[hoff + woff + 2] = qx;
								d->xyAndQ[hoff + woff + 3] = qy;
							}
							else
							{
								// manipal hybrid near point
								d->xyAndQ[hoff + woff + 2] = bestOfNineIndex(qx, qy, d->quantile);

							}
						}

					}
				}
			}

			else if (inBorder)
			{				
				d->xyAndQ[hoff + woff] = -1;
				d->xyAndQ[hoff + woff + 1] = -1;
				count++;
			}
			else if (!insideFrame )
			{
				d->xyAndQ[hoff + woff] = -swidth - sheight;
				d->xyAndQ[hoff + woff + 1] = -swidth - sheight;
			}
		}
	}
	// color to blacken out of area points
	uint8_t bgr[] = { 0,0,0 }, yuv[] = { 16,128,128 };

	if (d->test)
	{
		// will have white dots
		d->ddensity = (5 - d->dots) * 16;
		bgr[0] = 255;
		bgr[1] = 255;
		bgr[2] = 255;
	}

	convertBGRforInputFormat(d->col, bgr, fi);	
	
}
//------------------------------------------------------------------------------------------------

static const VSFrameRef* VS_CC rearviewmirrorGetFrame(int n, int activationReason, void** instanceData,
	void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
	RearViewMirrorData* d = (RearViewMirrorData*)*instanceData;

	if (activationReason == arInitial)
	{
		vsapi->requestFrameFilter(n, d->node, frameCtx);
		//if (!d->test)
			vsapi->requestFrameFilter(n, d->bnode, frameCtx);
	}
	else if (activationReason == arAllFramesReady)
	{
		const VSFrameRef* src = vsapi->getFrameFilter(n, d->node, frameCtx);
		const VSFrameRef* bsrc = vsapi->getFrameFilter(n, d->bnode, frameCtx);
		VSFrameRef* dst;
		if (!d->test)
		{
			
			dst = vsapi->copyFrame(bsrc, core);
			//vsapi->releaseFrameEarly(d->bnode, n, frameCtx);// seems does not work
		}
		else
		{
			// get src on which dots will be overlain			
			dst = vsapi->copyFrame(src, core);
		}

		
		const VSFormat* fi = d->vi->format;
		int sheight = vsapi->getFrameHeight(src, 0);
		int swidth = vsapi->getFrameWidth(src, 0);
		int nbits = fi->bitsPerSample;
		int nbytes = fi->bytesPerSample;
		//will not process A plane
		int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;
		
		int kb = 1;
		
		int dwidth = vsapi->getFrameWidth(dst, 0);
		int dheight = vsapi->getFrameHeight(dst, 0);		

		for (int p = 0; p < np; p++)
		{
			const uint8_t* sp = vsapi->getReadPtr(src, p);
			uint8_t* dp = vsapi->getWritePtr(dst, p);
			int spitch = vsapi->getStride(src, p) / nbytes;
			int dpitch = vsapi->getStride(dst, p) / nbytes;
			int subH = p == 0 || fi->colorFamily == cmRGB ? 0 : fi->subSamplingH;
			int subW = p == 0 || fi->colorFamily == cmRGB ? 0 : fi->subSamplingW;
			if (d->test)
			{
				if (fi->colorFamily == cmRGB)
				{
					if (nbytes == 1)
						dimplaneRGB(dp, sp, spitch, swidth, sheight, d->dim);
					else if (nbytes == 2)
						dimplaneRGB((uint16_t*)dp, (uint16_t*)sp, spitch, swidth, sheight, d->dim);
					else if (nbytes == 4)
						dimplaneRGB((float*)dp, (float*)sp, spitch, swidth, sheight, d->dim);
				}

				else if (p == 0 && fi->colorFamily == cmYUV)
				{
					if (nbytes == 1)
					{
						uint8_t limit = (uint8_t)16;
						dimplaneYUV(dp, sp, spitch, swidth, sheight, d->dim, limit);
					}
					else if (nbytes == 2)
					{
						uint16_t limit = (uint16_t)(16 << (nbits - 8));
						dimplaneYUV((uint16_t*)dp, (uint16_t*)sp, spitch, swidth, sheight, d->dim, limit);
					}
					else if (nbytes == 4)
						dimplaneYUV((float*)dp, (float*)sp, spitch, swidth, sheight, d->dim, 0.0f);
				}

				int iCenter = (sheight >> subH) / 2 * spitch + (swidth >> subW) / 2;// input for fish
				int oCenter = iCenter; // in test only input frame copy seen

				// we will put dots
				for (int h = d->ddensity / 2; h < d->fborder; h += d->ddensity)
				{
					int hoff = d->nEntries * h * d->fborder;

					for (int w = d->ddensity / 2; w < d->fborder; w += d->ddensity)
					{
						int woff = d->nEntries * w;

						int x = d->xyAndQ[hoff + woff];
						int y = d->xyAndQ[hoff + woff + 1];
						// ensure points are within frame
						if (x >= 0)
						{
							// white dots are placed
							if (nbytes == 1)
								paint4FoldSym(dp + oCenter, dpitch, 1, x >> subW, y >> subH, d->col[p]);
							else if (nbytes == 2)
								paint4FoldSym((uint16_t*)dp + oCenter, dpitch, 1, x >> subW, y >> subH, *((uint16_t*)d->col + p));
							else if (nbytes == 4)
								paint4FoldSym((float*)dp + oCenter, dpitch, 1, x >> subW, y >> subH, *((float*)d->col + p));
						}
					}
				}
			}	// if test

			else	// not test. normal processing
			{
				// offset to center of output image
				int oCenter = (d->mcy >> subH) * dpitch + (d->mcx >> subW);// fish image is centered about this origin
				// offset to center of input image
				int iCenter = (sheight >> subH) / 2 * spitch + (swidth >> subW) / 2;
				// during interpolation limiting values for clamping
				uint8_t min8 = (uint8_t)(fi->colorFamily == cmYUV ? 16 : 0);
				uint8_t max8 = (uint8_t)(fi->colorFamily == cmYUV ? 235 : 255);
				uint16_t min16 = (uint16_t)(fi->colorFamily == cmYUV ? 16 << (nbits - 8) : 0);
				uint16_t max16 = (uint16_t)((fi->colorFamily == cmYUV ? 235 : 255 << (nbits - 8)) << (nbits - 8));
				float minf = 0, maxf = 1.0f;

				if (p > 0 && fi->colorFamily == cmYUV)
				{
					minf = -0.5f;
					maxf = 0.5f;
				}

				int x, y, qx, qy, span2 = d->span / 2;
				int offh, offw;
				int index;

				for (int h = 0; h < d->fborder; h++)
				{
					offh = h * d->fborder * d->nEntries;

					for (int w = 0; w < d->fborder; w++)
					{
						offw = d->nEntries * w;

						x = d->xyAndQ[offh + offw];
						y = d->xyAndQ[offh + offw + 1];

						if (d->q > 1)
						{
							qx = d->xyAndQ[offh + offw + 2];
							qy = d->xyAndQ[offh + offw + 3];
						}
						else
						{
							index = d->xyAndQ[offh + offw + 2];
						}

						if (x == -swidth - sheight && y == -swidth - sheight)
						{
							continue; // out of field flag seen
						}

						else if (x == -1 && y == -1)
						{
							// in the border flag seen. paint black
							if (nbytes == 1)
								paint4FoldSym(dp + oCenter, dpitch, 1, w >> subW, h >> subH, d->col[p]);
							else if (nbytes == 2)
								paint4FoldSym((uint16_t*)dp + oCenter, dpitch, 1, w >> subW, h >> subH, *((uint16_t*)d->col + p));
							else if (nbytes == 4)
								paint4FoldSym((float*)dp + oCenter, dpitch, 1, w >> subW, h >> subH, *((float*)d->col + p));
						}

						else if ((x >= swidth / 2 - span2 - 1 && x < swidth) || (y >= sheight / 2 - span2 - 1 && y < sheight))
						{
							
							//  interpolation does not have sufficient points
							// points are within src frame
							if (nbytes == 1)
							{
								// near point
								copy4FoldSym(dp + oCenter, dpitch, sp + iCenter, spitch, 1,
									w >> subW, h >> subH, x >> subW, y >> subH);
							}
							else if (nbytes == 2)
							{
								copy4FoldSym((uint16_t*)dp + oCenter, dpitch, (uint16_t*)sp + iCenter, spitch, 1,
									w >> subW, h >> subH, x >> subW, y >> subH);
							}
							else if (nbytes == 4)
							{
								// near point
								copy4FoldSym((float*)dp + oCenter, dpitch, (float*)sp + iCenter, spitch, 1,
									w >> subW, h >> subH, x >> subW, y >> subH);
							}
						}
						
						else if (x < swidth / 2 - span2 - 1 && y < sheight / 2 - span2 - 1)
						{							
							
							// sufficient points for interpolation are available
							if (nbytes == 1)
							{
								if (subH > 0 || subW > 0)
								{
									copy4FoldSym(dp + oCenter, dpitch, sp + iCenter, spitch, 1,
										w >> subW, h >> subH, x >> subW, y >> subH);
								}
								else if (d->q == 1)
									interpolate9pt4FoldSym(dp + oCenter, dpitch, sp + iCenter, spitch,
										1, w, h, x, y, index);
								else
									interpolate4FoldSym(dp + oCenter, dpitch, sp + iCenter, spitch,
										1, w, h, x, y, qx, qy, d->span, d->iCoeff, min8, max8);
							}

							else if (nbytes == 2)
							{
								if (subH > 0 || subW > 0)
								{
									copy4FoldSym((uint16_t*)dp + oCenter, dpitch, (uint16_t*)sp + iCenter, spitch, 1,
										w >> subW, h >> subH, x >> subW, y >> subH);
								}
								else if (d->q == 1)
									interpolate9pt4FoldSym((uint16_t*)dp + oCenter, dpitch, (uint16_t*)sp + iCenter, spitch,
										1, w, h, x, y, index);
								else

									interpolate4FoldSym((uint16_t*)dp + oCenter, dpitch, (uint16_t*)sp + iCenter, spitch,
										1, w, h, x, y, qx, qy, d->span, d->iCoeff, min16, max16);
							}

							else if (nbytes == 4)
							{
								if (subH > 0 || subW > 0)
								{
									copy4FoldSym((float*)dp + oCenter, dpitch, (float*)sp + iCenter, spitch, 1,
										w >> subW, h >> subH, x >> subW, y >> subH);
								}
								else if (d->q == 1)
									interpolate9pt4FoldSym((float*)dp + oCenter, dpitch, (float*)sp + iCenter, spitch,
										1, w, h, x, y, index);
								else

									interpolate4FoldSym((float*)dp + oCenter, dpitch, (float*)sp + iCenter, spitch,
										1, w, h, x, y, qx, qy, d->span, d->iCoeff, minf, maxf);
							}
						}
					}
				}
			}
		}
		
		vsapi->freeFrame(src);
		vsapi->freeFrame(bsrc);
		
		return dst;
	}
	return 0;
}

/***************************************************************/
static void VS_CC rearviewmirrorFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
	RearViewMirrorData* d = (RearViewMirrorData*)instanceData;
	vsapi->freeNode(d->node);
	vsapi->freeNode(d->bnode);

	vs_aligned_free(d->xyAndQ);
	if (!d->iCoeff == NULL)
		vs_aligned_free(d->iCoeff);

	free(d);
}

static void VS_CC rearviewmirrorCreate(const VSMap* in, VSMap* out, void* userData,
	VSCore* core, const VSAPI* vsapi)
{
	RearViewMirrorData d;
	RearViewMirrorData* data;
	int err;
	int temp;

	// Get a clip reference from the input arguments. This must be freed later.
	d.node = vsapi->propGetNode(in, "clip", 0, 0);
	d.vi = vsapi->getVideoInfo(d.node);
	d.bnode = vsapi->propGetNode(in, "clip", 0, 0);
	const VSVideoInfo* bvi = vsapi->getVideoInfo(d.node);

	// In this first version we only want to handle 8bit integer formats. Note that
	// vi->format can be 0 if the input clip can change format midstream.
	if (!isConstantFormat(d.vi) || d.vi->width == 0 || d.vi->height == 0
		|| (d.vi->format->colorFamily != cmYUV && d.vi->format->colorFamily != cmGray
			&& d.vi->format->colorFamily != cmRGB))
	{
		vsapi->setError(out, "RearViewMirror: only RGB, Yuv or Gray color constant formats and const frame dimensions input supported");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}
	
	if (d.vi->format->sampleType == stFloat && d.vi->format->bitsPerSample == 16)
	{
		vsapi->setError(out, "RearViewMirror: half float input not allowed.");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	if (!isSameFormat(d.vi, bvi) || d.vi->numFrames != bvi->numFrames)
	{
		vsapi->setError(out, "RearViewMirror: both input clips must have identical formats and lengths.");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	// If a property read fails for some reason (index out of bounds/wrong type)
	// then err will have flags set to indicate why and 0 will be returned. This
	// can be very useful to know when having optional arguments. Since we have
	// strict checking because of what we wrote in the argument string, the only
	// reason this could fail is when the value wasn't set by the user.
	// And when it's not set we want it to default to enabled.

	d.method = int64ToIntS(vsapi->propGetInt(in, "method", 0, &err));
	if (err)
		d.method = 3;
	if (d.method < 1 || d.method > 5)
	{
		vsapi->setError(out, "RearViewMirror: method must be between 1 and 5 ");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	d.mcx = int64ToIntS(vsapi->propGetInt(in, "mcx", 0, &err));
	if (err)
		d.mcx = d.vi->width / 2;

	d.mcy = int64ToIntS(vsapi->propGetInt(in, "mcy", 0, &err));
	if (err)
		d.mcy = d.vi->height / 2;

	d.q = int64ToIntS(vsapi->propGetInt(in, "q", 0, &err));
	if (err)
		d.q = 1;
	else if (d.q < 1 || d.q > 4)
	{
		vsapi->setError(out, "RearViewMirror: q must be 1 to 4 only ");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	d.fov = (double)vsapi->propGetFloat(in, "fov", 0, &err);

	if (err)
		d.fov = 120.0;
	else if (d.fov < 20 || d.fov > 170)
	{
		vsapi->setError(out, "RearViewMirror: fov can be 20 to 170 only ");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	temp = !!int64ToIntS(vsapi->propGetInt(in, "oval", 0, &err));
	if (err)
		d.oval = true;
	else
		d.oval = temp == 0 ? false : true;


	d.cvx = (double)(vsapi->propGetFloat(in, "cvx", 0, &err));
	if (err)
		d.cvx = 1.15;
	if (d.cvx < 1.0 || d.cvx > 1.5)
	{
		vsapi->setError(out, "RearViewMirror: cvx must be 1.0 to 1.5 ");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	d.mwd = int64ToIntS(vsapi->propGetInt(in, "mwd", 0, &err));
	if (err)
		d.mwd = d.vi->width / 4;
	d.mht = int64ToIntS(vsapi->propGetInt(in, "mht", 0, &err));
	if (err)
		d.mht = d.vi->height / 4;
	d.borderWidth = int64ToIntS(vsapi->propGetInt(in, "border", 0, &err));
	if (err)
		d.borderWidth = 5;

	if (d.mwd < 40 || d.mht < 40 || d.borderWidth < 0 || d.borderWidth > 30
		|| d.mcx - d.mwd / 2 - d.borderWidth < 0 || d.mcx + d.mwd / 2 + d.borderWidth >= d.vi->width
		|| d.mcy - d.mht / 2 - d.borderWidth < 0 || d.mcy + d.mht / 2 + d.borderWidth >= d.vi->height)
	{
		vsapi->setError(out, "RearViewMirror: mwd and mht must be 40 or more, border between 0 and 30 and together mirror must be fully inframe");
		vsapi->freeNode(d.node);
		vsapi->freeNode(d.bnode);
		return;
	}

	temp = !!int64ToIntS(vsapi->propGetInt(in, "test", 0, &err));
	if (err)
		d.test = false;
	else
		d.test = temp == 0 ? false : true;
	if (d.test)
	{

		d.dots = int64ToIntS(vsapi->propGetInt(in, "dots", 0, &err));
		if (err)
			d.dots = 2;
		else if (d.dots < 1 || d.dots > 4)
		{
			vsapi->setError(out, "RearViewMirror: dots must be 1 to 4 only ");
			vsapi->freeNode(d.node);
			vsapi->freeNode(d.bnode);
			return;
		}

		d.dim = (float)(1.0 - vsapi->propGetFloat(in, "dim", 0, &err));
		if (err)
			d.dim = 0.75f;
		if (d.dim < 0.0f || d.dim > 1.0f)
		{
			vsapi->setError(out, "RearViewMirror: dim must be from 0 to 1.0 only ");
			vsapi->freeNode(d.node);
			vsapi->freeNode(d.bnode);
			return;
		}

	}
	
	// I usually keep the filter data struct on the stack and don't allocate it
// until all the input validation is done.
	data = (RearViewMirrorData*)malloc(sizeof(d));
	*data = d;


	vsapi->createFilter(in, out, "RearViewMirror", rearviewmirrorInit, rearviewmirrorGetFrame, rearviewmirrorFree, fmParallel, 0, data, core);

}
// registerFunc("RearViewMirror", "clip:clip;bclip:clip;method:int:opt;mcx:int:opt;mcy:int:opt;"
//				"mwd:int:opt;mht:int:opt;oval:int:opt;border:int:opt;cvx:float:opt;fov:float:opt;'
//				'test:int:opt;dim:float:opt;q:int:opt;dots:int:opt;", rearviewmirrorCreate, 0, plugin);



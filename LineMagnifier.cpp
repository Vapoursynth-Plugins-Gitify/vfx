//////////////////////////////////////////
/******************************************************************************
The LineMagnifier Filter copies  circular disks from source dst to destination dst
after magnifying them as with circular linemagnifieres formed by figured glass.


Author V.C.Mohan.
Date 18 Mar 2021
copyright  2021

This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    A copy of the GNU General Public License is at
    see <http://www.gnu.org/licenses/>.

-----------------------------------------------------------------------------*/
#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int StartFrame;
    int EndFrame;

    bool vert;
    int lwidth;		// width of lens
    float mag;		// magnification
    int xy;		    // initial xy coord	
    int exy;		// Final xy Coord
    //bool simple;
    bool drop;		// is drop effect required?
    
    int quantiles;  // pre set interpolation intervals
    int span;
   
    float* cubicCoeff;
    int* nearxy;
    int* qxy;
} LineMagnifierData;

static void VS_CC linemagnifierInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    LineMagnifierData *d = (LineMagnifierData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

    d->quantiles = 64;
    d->span = 4;
    // using cubic interpolation at 64 quantiles preset intervals
    d->cubicCoeff = (float*)vs_aligned_malloc(sizeof(float) * (d->quantiles + 1) * d->span, 32);
    CubicIntCoeff(d->cubicCoeff, d->quantiles);

    d->nearxy = (int*)vs_aligned_malloc(sizeof(int) * d->lwidth * 2, 32);
    d->qxy = d->nearxy + d->lwidth;

    float lwsq = (float)(d->lwidth * d->lwidth * 0.25f);

    for (int lw = 0; lw < d->lwidth; lw++)
    {
        float lmag = d->drop ? d->mag * (1.0f             
            + (float)((lw - d->lwidth / 2) * (lw - d->lwidth / 2)) / lwsq) / 2.0f : d->mag;
        float xy = lw / lmag;
        int xynear = (int)xy;	// nearest lower point 
        d->nearxy[lw] = xynear;
        d->qxy[lw] = (int)(fabs(xy - xynear) * d->quantiles);
    }
    
}

static const VSFrameRef *VS_CC linemagnifierGetFrame(int in, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    LineMagnifierData *d = (LineMagnifierData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) 
    {
        
        const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

        if (in < d->StartFrame || in > d->EndFrame)
        {
            return src;
        }

        int n = in - d->StartFrame;
        int nFrames = d->EndFrame - d->StartFrame + 1;

        const VSFormat* fi = d->vi->format;
       // int ht = d->vi->height;
       // int wd = d->vi->width;
        // to ensure outside of linemagnifier spaces are filled properly
        VSFrameRef* dst = vsapi->copyFrame(src, core);

       // const unsigned char* sp[] = { NULL, NULL, NULL };
       // unsigned char* dp[] = { NULL, NULL, NULL };
        int subH = fi->subSamplingH;
        int subW = fi->subSamplingW;
        int andH = (1 << subH) - 1;
        int andW = (1 << subW) - 1;       
        int nbytes = fi->bytesPerSample;
        int nbits = fi->bitsPerSample;
        int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;

        
        // calculate current values of parameters
        int cxy = d->xy + ((d->exy - d->xy) * n) / nFrames;        
        //float mag = d->imag + ((d->emag - d->imag) * n) / nFrames;
        //int lensWidth = d->lwidth;
        
        if (d->vert)
        {
            int w = cxy;
            for (int p = 0; p < np; p++)
            {
                const uint8_t* sp = vsapi->getReadPtr(src, p);
                uint8_t* dp = vsapi->getWritePtr(dst, p);
                int stride = vsapi->getStride(src, p);
                int ht = vsapi->getFrameHeight(src, p);

                for (int h = 0; h < ht; h++)
                {

                    if (fi->sampleType == stInteger && nbytes == 1)
                    {

                        uint8_t min = fi->colorFamily == cmRGB ? 0 : 16;
                        uint8_t max = fi->colorFamily == cmRGB ? 255 : 236;

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 ||  subW == 0)

                                *(dp + w + lw) = clamp(alongLineInterpolate(sp + w + d->nearxy[lw], 1,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if (((w + lw) & andW) == 0)

                                *(dp + ((w + lw) >> subW)) = *(sp + ((w + d->nearxy[lw]) >> subW));
                        }
                    }

                    else if (fi->sampleType == stInteger && nbytes == 2)
                    {

                        uint16_t min = fi->colorFamily == cmRGB ? 0 : 16 << (nbits - 8);
                        uint16_t max = fi->colorFamily == cmRGB ? (1 << nbits) - 1: 236 << (nbits - 8);

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 || subW == 0)

                                *(( uint16_t*)dp + w + lw) 
                                = clamp(alongLineInterpolate((const uint16_t*)sp + w + d->nearxy[lw], 1,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if (((w + lw) & andW) == 0)

                                *((uint16_t*)dp + ((w + lw) >> subW)) 
                                    = *((const uint16_t*)sp + ((w + d->nearxy[lw]) >> subW));
                        }
                    }

                    else if (fi->sampleType == stFloat && nbytes == 4)
                    {

                        float min = fi->colorFamily == cmRGB ? 0 : (p > 0) ? - 0.5f : 0.0f;
                        float max = fi->colorFamily == cmRGB ? 1.0f : p > 0 ? 0.5f : 1.0f;

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 || subW == 0)

                                *((float*)dp + w + lw)
                                = clamp(alongLineInterpolate((const float*)sp + w + d->nearxy[lw], 1,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if ( ((w + lw) & andW) == 0)

                                *((float*)dp + ((w + lw) >> subW))
                                = *((const float*)sp + ((w + d->nearxy[lw]) >> subW));
                        }
                    }
                    sp += stride;
                    dp += stride;
                }

            }

        }

        else //if (!d->vert)
        {
            int h = cxy;
            for (int p = 0; p < np; p++)
            {
                const uint8_t* sp = vsapi->getReadPtr(src, p);
                uint8_t* dp = vsapi->getWritePtr(dst, p);
                int stride = vsapi->getStride(src, p);
                int wd = vsapi->getFrameWidth(src, p);
                int pitch = stride / nbytes;

                for (int w = 0; w < wd; w++)
                {

                    if (fi->sampleType == stInteger && nbytes == 1)
                    {

                        uint8_t min = fi->colorFamily == cmRGB ? 0 : 16;
                        uint8_t max = fi->colorFamily == cmRGB ? 255 : 236;

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 || subH == 0)

                                *(dp + (h + lw) * pitch + w)  
                                = clamp(alongLineInterpolate(sp + (h + d->nearxy[lw]) * pitch + w, pitch,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if ( ((h + lw) & andH) == 0)

                                *(dp + ((h + lw) >> subH) * pitch + w) 
                                = *(sp + ((h + d->nearxy[lw]) >> subH)* pitch + w);
                        }
                    }

                    else if (fi->sampleType == stInteger && nbytes == 2)
                    {

                        uint16_t min = fi->colorFamily == cmRGB ? 0 : 16 << (nbits - 8);
                        uint16_t max = fi->colorFamily == cmRGB ? (1 << nbits) - 1 : 236 << (nbits - 8);

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 || subH == 0)

                                *((uint16_t*)dp + (h + lw) * pitch + w)
                                = clamp(alongLineInterpolate((const uint16_t*)sp + (h + d->nearxy[lw]) * pitch + w, pitch,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if (((h + lw) & andH) == 0)

                                *((uint16_t*)dp + ((h + lw) >> subH) * pitch + w)
                                = *((const uint16_t*)sp + ((h + d->nearxy[lw]) >> subH) * pitch + w);
                        }
                    }

                    else if (fi->sampleType == stFloat && nbytes == 4)
                    {

                        float min = fi->colorFamily == cmRGB ? 0 : (p > 0) ? -0.5f : 0.0f;
                        float max = fi->colorFamily == cmRGB ? 1.0f : p > 0 ? 0.5f : 1.0f;

                        for (int lw = 0; lw < d->lwidth; lw++)
                        {

                            if (p == 0 || subH == 0)

                                *((float*)dp + (h + lw) * pitch + w)
                                = clamp(alongLineInterpolate((const float*)sp + (h + d->nearxy[lw]) * pitch + w, pitch,
                                    d->span, d->qxy[lw], d->cubicCoeff), min, max);

                            else if (((h + lw) & andH) == 0)

                                *((float*)dp + ((h + lw) >> subH) * pitch + w)
                                = *((const float*)sp + ((h + d->nearxy[lw]) >> subH) * pitch + w);
                        }
                    }
                    
                }

            }
        }

        
        vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC linemagnifierFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    LineMagnifierData *d = (LineMagnifierData *)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->cubicCoeff);
    vs_aligned_free(d->nearxy);
    free(d);
}

static void VS_CC linemagnifierCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    LineMagnifierData d;
    LineMagnifierData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat && !isConstantFormat(d.vi))
    {
        vsapi->setError(out, "LineMagnifier: only RGB, YUV, Gray constant formats, with integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }
    
    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "LineMagnifier: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "LineMagnifier: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        return;
    }

    //-----------------

    int temp = !! int64ToIntS(vsapi->propGetInt(in, "vert", 0, &err));
    if (err)
        d.vert = true;
    else if (temp == 0)
        d.vert = false;
    else
        d.vert = true;

    d.lwidth = int64ToIntS(vsapi->propGetInt(in, "lwidth", 0, &err));
    if (err)
        d.lwidth = d.vert ? d.vi->width / 16 : d.vi->height / 16;
    else if (d.lwidth < 4 || d.lwidth >(d.vert ? d.vi->width / 8 : d.vi->height / 8))
    {
        vsapi->setError(out, "LineMagnifier: lwidth must be  4 to 1/8 th of corresponding frame dimension ");
        vsapi->freeNode(d.node);
        return;
    }

    temp = !!int64ToIntS(vsapi->propGetInt(in, "drop", 0, &err));
    if (err)
        d.drop = false;
    else if (temp == 0)
        d.drop = false;
    else
        d.drop = true;


    d.mag = (float)vsapi->propGetFloat(in, "mag", 0, &err);
    if (err)
        d.mag = 4.0f;
    else if (d.mag < (d.drop ? 2.0f : 1.5f) || d.mag >8.0f)
    {
        vsapi->setError(out, "LineMagnifier: mag can have values from 1.5 to 8.0 or if drop is set, 2.0 to 8.0 ");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.vert)
    {
        d.xy = int64ToIntS(vsapi->propGetInt(in, "xy", 0, &err));
        if (err)
            d.xy = d.vi->width / 2;
        else if (d.xy < d.lwidth / 2 + 4 || d.xy > d.vi->width - 4 - d.lwidth / 2)
        {
            vsapi->setError(out, "LineMagnifier: xy be at least 4 + half lwidth pixels inside frame ");
            vsapi->freeNode(d.node);
            return;
        }

        d.exy = int64ToIntS(vsapi->propGetInt(in, "exy", 0, &err));
        if (err)
            d.exy = d.xy;
        else if (d.exy < d.lwidth / 2 + 4 || d.exy > d.vi->width - 4 - d.lwidth / 2)
        {
            vsapi->setError(out, "LineMagnifier: exy be at least 4 + half lwidth pixels inside frame ");
            vsapi->freeNode(d.node);
            return;
        }
    }

    else
    {
        d.xy = int64ToIntS(vsapi->propGetInt(in, "xy", 0, &err));
        if (err)
            d.xy = d.vi->height / 2;
        else if (d.xy < d.lwidth / 2 + 4 || d.xy > d.vi->height - 4 - d.lwidth / 2)
        {
            vsapi->setError(out, "LineMagnifier: xy be at least 4 + half lwidth pixels inside frame ");
            vsapi->freeNode(d.node);
            return;
        }

        d.exy = int64ToIntS(vsapi->propGetInt(in, "exy", 0, &err));
        if (err)
            d.exy = d.xy;
        else if (d.exy < d.lwidth / 2 + 4 || d.exy > d.vi->height - 4 - d.lwidth / 2)
        {
            vsapi->setError(out, "LineMagnifier: exy be at least 4 + half lwidth pixels inside frame ");
            vsapi->freeNode(d.node);
            return;
        }
    }


    
   
    
    data = (LineMagnifierData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "LineMagnifier", linemagnifierInit, linemagnifierGetFrame, linemagnifierFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
/*

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.linemagnifier.vfx", "LineMagnifier", "VapourSynth vfx plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("LineMagnifier", "clip:clip;sf:int:opt;ef:int:opt;lwidth:int:opt;mag:float:opt;drop:int:opt;"
    "xy:int:opt;exy:int:opt;vert:int:opt;", linemagnifierCreate, 0, plugin);
}*/

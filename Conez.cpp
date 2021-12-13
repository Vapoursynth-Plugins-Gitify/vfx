/******************************************************************************
Conez filter plugin for Avisynth+ by V.C.Mohan
The image wraps around a cone/cylinder.   base radius. top radius and
 shading along cone length are the params
 26 Feb 2021

********************************************************************************/
#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;
    VSNodeRef * bnode;
    int StartFrame;
    int EndFrame;
    bool vert;
    bool progressive;
    int base;
    int top;
    
    float* cubic;
    int quantiles;
    int span;
   
} ConezData;

static void VS_CC conezInit(VSMap *in, VSMap *out, void **instanceData,
                            VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    ConezData *d = (ConezData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
    d->quantiles = 64;
    d->span = 4;
    d->cubic = (float*)vs_aligned_malloc(sizeof(float) * 4 * (d->quantiles + 1), 32);
    CubicIntCoeff(d->cubic, d->quantiles);
}

static const VSFrameRef* VS_CC conezGetFrame(int in, int activationReason, void** instanceData, 
                        void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    ConezData* d = (ConezData*)*instanceData;

    if (activationReason == arInitial) 
    {
         vsapi->requestFrameFilter(in, d->node, frameCtx);
         vsapi->requestFrameFilter(in, d->bnode, frameCtx);
    }

    else if (activationReason == arAllFramesReady) 
    {
        const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);
        const VSFrameRef* bkg = vsapi->getFrameFilter(in, d->bnode, frameCtx);
    
        if (in < d->StartFrame || in > d->EndFrame)
        {
            vsapi->freeFrame(bkg);
            return src;
        }
        int n = in - d->StartFrame;
        int nframes = d->EndFrame - d->StartFrame + 1;

        VSFrameRef* dst = vsapi->copyFrame(bkg, core);
        const VSFormat* fi = d->vi->format;
        const unsigned char* sp[] = { NULL, NULL, NULL };
        unsigned char* dp[] = { NULL, NULL, NULL };
        int subH = fi->subSamplingH;
        int subW = fi->subSamplingW;
        int andH = (1 << subH) - 1;
        int andW = (1 << subW) - 1;

        int pitch[] = { 0,0,0 };
        int nbytes = fi->bytesPerSample;
        int nbits = fi->bitsPerSample;
        int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;

        int ht = d->vi->height;
        int wd = d->vi->width;

        for (int p = 0; p < np; p++)
        {
            sp[p] = vsapi->getReadPtr(src, p);
            dp[p] = vsapi->getWritePtr(dst, p);
            pitch[p] = vsapi->getStride(dst, p) / nbytes;
        }
        // process
        if (d->vert)
        {
            int diaTop = wd - ((wd - d->top) * n) / nframes;
            int diaBot = wd - ((wd - d->base) * n) / nframes;

            if (!d->progressive)
            {
                diaTop = d->top;
                diaBot = d->base;
            }
            for (int h = 0; h < ht; h++)
            {
                // radius at current h. 
                int rad = (diaBot - ((diaBot - diaTop) * (ht - h)) / ht) / 2;
                
                float ratio = (float)(wd / (M_PI * rad)); // width /  (pi * r)
                
                for (int w = 0; w < rad; w++)
                {
                    //  alfa is angle between vertical axis line through center , and line joining 
                    //projection point on to circle perimeter.
                    // so angle = acos(x/r). alfa = (PI / 2 - angle). 
                    // length of arc = r * alfa. Half perimeter = PI * rad
                    // mult ratio = wd /(half perimeter) 
                    float angle = acos((float)w / (float)rad);
                   // float alfa = M_PI_2 - angle;
                    float x = (float)(rad * (M_PI_2 - angle));
                    float originalLocation = x * ratio;
                    // lower integer nearest
                    int wnew = (int)originalLocation;
                    // fraction is at this quantile
                    int qx = (int)((originalLocation - wnew) * d->quantiles);

                    if (fi->sampleType == stInteger && nbytes == 1)
                    {
                        for (int p = 0; p < np; p++)
                        {
                            uint8_t min = 0, max = 255;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = 16;
                                max = 240;
                            }

                            const uint8_t* spp = sp[p];
                            uint8_t* dpp = dp[p];

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                int newval = (int)clamp( alongLineInterpolate(spp + wd / 2 + wnew,
                                                1, d->span, qx, d->cubic), min, max ) ;
                                dpp[wd / 2 + w] = newval;
                                // symmetrical position
                                newval = (int)clamp(alongLineInterpolate(spp + wd / 2 - wnew ,
                                                -1, d->span, qx, d->cubic), min, max);
                                dpp[ wd / 2 - w] = newval;

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((wd / 2 + w) >> subW) )
                                    = *(spp + ( (wd / 2 + wnew) >> subW) );
                                    *(dpp + ( (wd / 2 - w) >> subW) )
                                    = *(spp + ( (wd / 2 - wnew) >> subW) );
                                }
                            }
                        }
                    }

                    if (fi->sampleType == stInteger && nbytes == 2)
                    {
                        for (int p = 0; p < np; p++)
                        {
                            uint16_t min = 0, max = (1 << nbits) - 1;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = (16 << (nbits - 8) );
                                max = (240 << (nbits - 8) );
                            }

                            const uint16_t* spp = (uint16_t*)(sp[p]);
                            uint16_t* dpp = (uint16_t *)(dp[p]);

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                int newval = (int)clamp(alongLineInterpolate(spp + wd / 2 + wnew,
                                    1, d->span, qx, d->cubic), min, max);
                                dpp[wd / 2 + w] = newval;
                                // symmetrical position
                                newval = (int)clamp(alongLineInterpolate(spp + wd / 2 - wnew,
                                    -1, d->span, qx, d->cubic), min, max);
                                dpp[wd / 2 - w] = newval;

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((wd / 2 + w) >> subW))
                                        = *(spp + ((wd / 2 + wnew) >> subW));
                                    *(dpp + ((wd / 2 - w) >> subW))
                                        = *(spp + ((wd / 2 - wnew) >> subW));
                                }
                            }
                        }
                    }
                    if (fi->sampleType == stFloat)
                    {
                        for (int p = 0; p < np; p++)
                        {
                            float min = 0, max = 1.0f;

                            if (fi->colorFamily == cmYUV && p > 0)
                            {
                                min = -0.5f;
                                max = 0.5f;
                            }

                            const float* spp = (float*)(sp[p]);
                            float* dpp = (float*)(dp[p]);

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                
                                float newval = (float)clamp(alongLineInterpolate(spp + wd / 2 + wnew,
                                    1, d->span, qx, d->cubic), min, max);
                                dpp[wd / 2 + w] = newval; 
                                // symmetrical position
                                newval = (float)clamp(alongLineInterpolate(spp + wd / 2 - wnew,
                                    -1, d->span, qx, d->cubic), min, max);
                                dpp[wd / 2 - w] = newval;

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((wd / 2 + w) >> subW))
                                        = *(spp + ((wd / 2 + wnew) >> subW));
                                    *(dpp + ((wd / 2 - w) >> subW))
                                        = *(spp + ((wd / 2 - wnew) >> subW));
                                }
                            }
                        }
                    }
                }

                for (int p = 0; p < np; p++)
                {
                    if (p == 0 || (h & andH) == 0)
                    {
                        sp[p] += pitch[p] * nbytes;
                        dp[p] += pitch[p] * nbytes;
                    }
                    
                }
            }
        }

        else // if (!d->vert) Horizontal orientation
        {

            int diaTop = ht - ((ht - d->top) * n) / nframes;
            int diaBot = ht - ((ht - d->base) * n) / nframes;

            if (!d->progressive)
            {
                diaTop = d->top;
                diaBot = d->base;
            }
            for (int w = 0; w < wd; w++)
            {
                // even number as we will divide by 2 for each half.
                // diameter of cone at this value of W
                int rad = (diaBot - ((diaBot - diaTop) * (wd - w)) / wd) / 2;
               
                float ratio = (float)(ht / ( M_PI * rad)); 
                //unsigned char max = 255;

                for (int h = 0; h < rad; h++)
                {
                    //  alfa is angle between vertical axis line through center , and line joining 
                    //projection point on to circle perimeter.
                    // so angle = acos(x/r). alfa = (PI / 2 - angle). 
                    // length of arc = r * alfa. Half perimeter = PI * rad
                    // mult ratio = wd /(half perimeter) 
                    float angle = acos((float)h / (float)rad);
                   // float alfa = M_PI_2 - angle;
                    float y = (float)(rad * (M_PI_2 - angle));

                    float originalLocation = y * ratio;
                    int hOrig = (int)originalLocation;
                    int qy = (int)((originalLocation - hOrig) * d->quantiles);

                    for (int p = 0; p < np; p++)
                    {
                        if (fi->sampleType == stInteger && nbytes == 1)
                        {

                            uint8_t min = 0, max = 255;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = 16;
                                max = 240;
                            }

                            const uint8_t* spp = sp[p];
                            uint8_t* dpp = dp[p];

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                int newval = (int)clamp(alongLineInterpolate(spp + (ht / 2 + hOrig) * pitch[p] + w,
                                    pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 + h) * pitch[p] + w] = newval; 
                                // symmetrical position
                                newval = (int)clamp(alongLineInterpolate(spp + (ht / 2 - hOrig) * pitch[p] + w,
                                    -pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 - h) * pitch[p] + w] = newval; 

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((ht / 2 + h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 + hOrig) >> subH) * pitch[p] + (w >> subW));
                                    *(dpp + ((ht / 2 - h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 - hOrig) >> subH) * pitch[p] + (w >> subW));
                                }
                            }
                        }


                        if (fi->sampleType == stInteger && nbytes == 2)
                        {

                            uint16_t min = 0, max = (1 << nbits) - 1;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = (16 << (nbits - 8));
                                max = (240 << (nbits - 8));
                            }

                            const uint16_t* spp = (uint16_t*)(sp[p]);
                            uint16_t* dpp = (uint16_t*)(dp[p]);

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                int newval = (int)clamp(alongLineInterpolate(spp + (ht / 2 + hOrig) * pitch[p] + w,
                                    pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 + h) * pitch[p] + w] = newval; 
                                // symmetrical position
                                newval = (int)clamp(alongLineInterpolate(spp + (ht / 2 - hOrig) * pitch[p] + w,
                                    -pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 - h) * pitch[p] + w] = newval; 

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((ht / 2 + h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 + hOrig) >> subH) * pitch[p] + (w >> subW));
                                    *(dpp + ((ht / 2 - h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 - hOrig) >> subH) * pitch[p] + (w >> subW));
                                }
                            }

                        }
                        if (fi->sampleType == stFloat)
                        {

                            float min = 0, max = 1.0f;

                            if (fi->colorFamily == cmYUV && p > 0)
                            {
                                min = -0.5f;
                                max = 0.5f;
                            }

                            const float* spp = (float*)(sp[p]);
                            float* dpp = (float*)(dp[p]);
                          

                            if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                            {
                                float newval = clamp(alongLineInterpolate(spp + (ht / 2 + hOrig) * pitch[p] + w,
                                    pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 + h) * pitch[p] + w] = newval; 
                                // symmetrical position
                                newval = clamp(alongLineInterpolate(spp + (ht / 2 - hOrig) * pitch[p] + w,
                                    -pitch[p], d->span, qy, d->cubic), min, max);
                                dpp[(ht / 2 - h) * pitch[p] + w] = newval; 

                            }
                            else
                            {
                                if ((w & andW) == 0 && (h & andH) == 0)
                                {
                                    *(dpp + ((ht / 2 + h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 + hOrig) >> subH) * pitch[p] + (w >> subW));
                                    *(dpp + ((ht / 2 - h) >> subH) * pitch[p] + (w >> subW))
                                        = *(spp + ((ht / 2 - hOrig) >> subH) * pitch[p] + (w >> subW));
                                }
                            }

                        }
                    }

                }
            }
        }
        vsapi->freeFrame(src);
        vsapi->freeFrame(bkg);
        return dst;
    }

    return 0;
}

static void VS_CC conezFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    ConezData *d = (ConezData *)instanceData;
    vsapi->freeNode(d->node);
    vsapi->freeNode(d->bnode);
    vs_aligned_free(d->cubic);
    free(d);
}

static void VS_CC conezCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    ConezData d;
    ConezData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);
    d.bnode = vsapi->propGetNode(in, "bkg", 0, 0);
    const VSVideoInfo* bvi = vsapi->getVideoInfo(d.bnode);
    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat)
    {
        vsapi->setError(out, "Conez: only RGB, YUV, Gray color formats, with integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.bnode);
        return;
    }
    if ( ! isConstantFormat(d.vi) && ! isSameFormat(d.vi, bvi) && d.vi->numFrames > bvi->numFrames)
    {
        vsapi->setError(out, "Conez: both clips must have constant identical formats and clip should not have more number of frames than bkg ");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.bnode);
        return;
    }

    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "Conez: sf must be within video");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "Conez: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.node);
        return;
    }

    d.vert = !! int64ToIntS(vsapi->propGetInt(in, "vert", 0, &err));
    if (err)
        d.vert = true;
    else if (d.vert == 0)
        d.vert = false;
    else
        d.vert = true;

    d.progressive = !!int64ToIntS(vsapi->propGetInt(in, "prog", 0, &err));
    if (err)
        d.progressive = true;
    else if (d.progressive == 0)
        d.progressive = false;
    else
        d.progressive = true;


    int temp = (int)(((d.vert) ? d.vi->width - 8 : d.vi->height - 8) / M_PI_2);
    
    d.top = int64ToIntS(vsapi->propGetInt(in, "top", 0, &err));
    if (err)
        d.top = temp / 8;
    else if (d.top < 4 || d.top > temp  )
    {
        vsapi->setError(out, "Conez: top can be 8 to frame width - 8 for vert and frame height - 8 for hor");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.node);
        return;
    }
    d.base = int64ToIntS(vsapi->propGetInt(in, "base", 0, &err));
    if (err)
        d.base = temp ;
    else if (d.base < 8 || d.base >  temp)
    {
        vsapi->setError(out, "Conez: base can be 8 to frame width - 8 for vert and frame height - 8 for hor");
        vsapi->freeNode(d.node);
        vsapi->freeNode(d.node);
        return;
    }
    

    data = (ConezData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Conez", conezInit, conezGetFrame, conezFree, fmParallel, 0, data, core);
}
/*
//////////////////////////////////////////
// Init

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.conez.vfx", "conez", "VapourSynth Filter Skeleton", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Conez", "clip:clip;bkg:clip;sf:int:opt;ef:int:opt;"
                        "vert:int:opt;prog:int:opt;top:int:opt;base:int:opt;", conezCreate, 0, plugin);
}
*/
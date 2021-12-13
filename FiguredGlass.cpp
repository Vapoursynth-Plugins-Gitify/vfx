//////////////////////////////////////////
/******************************************************************************
The FiguredGlass Filter copies  circular disks from source dst to destination dst
after magnifying them as with circular lenses formed by figured glass.
12 Mar 2021
Author V.C.Mohan
*************************************************************************************************/
#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int StartFrame;
    int EndFrame;
    int  rad;
    float imag;
    bool drop;
    
    int quantiles;
    int span;
   // int nOffsets, noffsetsUV;
    float* cubicCoeff;
  //  int* offsets, * offsetsUV;
} FiguredGlassData;

static void VS_CC figuredglassInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    FiguredGlassData *d = (FiguredGlassData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

    d->quantiles = 64;
    d->span = 4;
    // using cubic interpolation at 64 quantiles preset intervals
    d->cubicCoeff = (float*)vs_aligned_malloc(sizeof(float) * (d->quantiles + 1) * d->span, 32);
    CubicIntCoeff(d->cubicCoeff, d->quantiles);
    
}

static const VSFrameRef *VS_CC figuredglassGetFrame(int in, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    FiguredGlassData *d = (FiguredGlassData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        
        const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

        if (in < d->StartFrame || in > d->EndFrame)
        {
            return src;
        }

        int n = in - d->StartFrame;
        const VSFormat* fi = d->vi->format;
        int ht = d->vi->height;
        int wd = d->vi->width;
        // to ensure inbetween spaces are filled properly
        VSFrameRef* dst = vsapi->copyFrame(src, core);

        const unsigned char* sp[] = { NULL, NULL, NULL };
        unsigned char* dp[] = { NULL, NULL, NULL };
        int subH[] = { 0, fi->subSamplingH, fi->subSamplingH };
        int subW[] = { 0, fi->subSamplingW, fi->subSamplingW };
       // int andH = (1 << subH[1]) - 1;
        //int andW = (1 << subW[1]) - 1;

        int pitch[] = { 0,0,0 };
        int nbytes = fi->bytesPerSample;
        int nbits = fi->bitsPerSample;
        int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;

        for (int p = 0; p < np; p++)
        {
            sp[p] = vsapi->getReadPtr(src, p);
            dp[p] = vsapi->getWritePtr(dst, p);
            pitch[p] = vsapi->getStride(dst, p) / nbytes;
        }
        
        // create discs with magnifications
        for (int cx = d->rad ; cx < wd   ; cx += 2 * d->rad)
        {
            for (int cy = d->rad ; cy < ht  ; cy += 2 * d->rad)
            {
                if (fi->sampleType == stInteger && nbytes == 1)
                {
                    uint8_t min = fi->colorFamily == cmRGB ? 0 : 16;
                    uint8_t max = fi->colorFamily == cmRGB ? 255 : 236;

                    circularLensMagnification(dp, sp, np, pitch, ht, wd, subW, subH, min, max,
                                                d->rad, cx, cy, d->imag, d->span,
                                                d->cubicCoeff, d->quantiles, d->drop);

                }

                else if (fi->sampleType == stInteger && nbytes == 2)
                {
                    uint16_t min = (fi->colorFamily == cmRGB ? 0 : 16) << (nbits - 8);
                    uint16_t max = (fi->colorFamily == cmRGB ? 255 : 236) << (nbits - 8);

                    circularLensMagnification((uint16_t**)dp, (const uint16_t**)sp, np, pitch, ht, wd, subW, subH, min, max,
                        d->rad, cx, cy, d->imag, d->span,
                        d->cubicCoeff, d->quantiles, d->drop);

                }

                else if (fi->sampleType == stFloat && nbytes == 4)
                {
                    
                    if (fi->colorFamily != cmYUV)
                    {
                        float min = 0;
                        float max = 1.0f;

                        circularLensMagnification((float**)dp, (const float**)sp, np, pitch, ht, wd, subW, subH, min, max,
                            d->rad, cx, cy, d->imag, d->span,
                            d->cubicCoeff, d->quantiles, d->drop);
                    }

                    else if (fi->colorFamily == cmYUV)
                    {
                        // go around for yuv as min and max values differ for Y and UV planes
                        float min = 0;
                        float max = 1.0f;

                        circularLensMagnification((float**)dp, (const float**)sp, 1, pitch, ht, wd, subW, subH, min, max,
                            d->rad, cx, cy, d->imag, d->span,
                            d->cubicCoeff, d->quantiles, d->drop);


                        if (np > 1)
                        {
                            // U,V planes 
                            float min = -0.5f;
                            float max = 0.5f;

                            circularLensMagnification((float**)(dp + 1), (const float**)(sp + 1), 2, pitch, ht, wd, subW, subH, min, max,
                                d->rad, cx, cy, d->imag, d->span,
                                d->cubicCoeff, d->quantiles, d->drop);
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

static void VS_CC figuredglassFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FiguredGlassData *d = (FiguredGlassData *)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->cubicCoeff);
    free(d);
}

static void VS_CC figuredglassCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    FiguredGlassData d;
    FiguredGlassData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat && !isConstantFormat(d.vi))
    {
        vsapi->setError(out, "FiguredGlass: only RGB, YUV, Gray constant formats, with integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }
    /*if ( fi->colorFamily == cmYUV && fi->sampleType == stFloat )
    {
        vsapi->setError(out, "FiguredGlass:  YUV  format float samples as Input not allowed ");
        vsapi->freeNode(d.node);
        return;
    }*/


    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "FiguredGlass: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "FiguredGlass: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        return;
    }

    int	rmax = d.vi->width > d.vi->height ? d.vi->height / 2 : d.vi->width / 2;

    d.rad = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
    if (err)
        d.rad = rmax / 8 > 4 ? rmax / 8 : 4;
    else if (d.rad < 4 || d.rad > rmax )
    {
        vsapi->setError(out, "FiguredGlass: rad must be 4 to half of frame smaller dimension");
        vsapi->freeNode(d.node);
        return;
    }
    d.imag = (float)vsapi->propGetFloat(in, "mag", 0, &err);
    if (err)
        d.imag = 4;
    else if (d.imag < 1.5f || d.imag > 8.0f)
    {
        vsapi->setError(out, "FiguredGlass: mag must be within 1.5 to 8.0");
        vsapi->freeNode(d.node);
        return;
    }
    
    int drop = !! int64ToIntS(vsapi->propGetInt(in, "drop", 0, &err));
    if (err)
        d.drop = true;
    else if (drop != 0)
        d.drop = true;
    else
        d.drop = false;
    
    data = (FiguredGlassData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "FiguredGlass", figuredglassInit, figuredglassGetFrame, figuredglassFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
/*

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.figuredglass.vfx", "FiguredGlass", "VapourSynth vfx plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("FiguredGlass", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;mag:float:opt;drop:int:opt;", figuredglassCreate, 0, plugin);
}*/

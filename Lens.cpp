//////////////////////////////////////////
/******************************************************************************
The Lens Filter copies  circular disks from source dst to destination dst
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

    int irad, erad;		// radius of lens
    float imag, emag;		// magnification
    int ix, ex;		// initial x coord	
    int iy, ey;		// initial y coord    
    bool drop;		// is drop effect required?
    
    int quantiles;  // pre set interpolation intervals
    int span;
   
    float* cubicCoeff;
  
} LensData;

static void VS_CC lensInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    LensData *d = (LensData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

    d->quantiles = 64;
    d->span = 4;
    // using cubic interpolation at 64 quantiles preset intervals
    d->cubicCoeff = (float*)vs_aligned_malloc(sizeof(float) * (d->quantiles + 1) * d->span, 32);
    CubicIntCoeff(d->cubicCoeff, d->quantiles);
    
}

static const VSFrameRef *VS_CC lensGetFrame(int in, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    LensData *d = (LensData *) * instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        
        const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

        if (in < d->StartFrame || in > d->EndFrame)
        {
            return src;
        }

        int n = in - d->StartFrame;
        int nFrames = d->EndFrame - d->StartFrame + 1;

        const VSFormat* fi = d->vi->format;
        int ht = d->vi->height;
        int wd = d->vi->width;
        // to ensure outside of lens spaces are filled properly
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
        // calculate current values of parameters
        int cx = d->ix + ((d->ex - d->ix) * n) / nFrames;
        int cy = d->iy + ((d->ey - d->iy) * n) / nFrames;
        int rad = d->irad + ((d->erad - d->irad) * n) / nFrames;
        float mag = (float)(d->imag + ((d->emag - d->imag) * n) / nFrames);
        // create discs with magnifications
        
        if (fi->sampleType == stInteger && nbytes == 1)
        {
            uint8_t min = fi->colorFamily == cmRGB ? 0 : 16;
            uint8_t max = fi->colorFamily == cmRGB ? 255 : 236;

            circularLensMagnification(dp, sp, np, pitch, ht, wd, subW, subH, min, max,
                rad, cx, cy, mag, d->span,
                d->cubicCoeff, d->quantiles, d->drop);

        }

        else if (fi->sampleType == stInteger && nbytes == 2)
        {
            uint16_t min = (fi->colorFamily == cmRGB ? 0 : 16) << (nbits - 8);
            uint16_t max = (fi->colorFamily == cmRGB ? 255 : 236) << (nbits - 8);

            circularLensMagnification((uint16_t**)dp, (const uint16_t**)sp, np, 
                pitch, ht, wd, subW, subH, min, max,
                rad, cx, cy, mag, d->span,
                d->cubicCoeff, d->quantiles, d->drop);

        }

        else if (fi->sampleType == stFloat && nbytes == 4)
        {
            if (fi->colorFamily != cmYUV)
            {
                float min = 0;
                float max = 1.0f;

                circularLensMagnification((float**)dp, (const float**)sp, np, 
                    pitch, ht, wd, subW, subH, min, max,
                    rad, cx, cy, mag, d->span,
                    d->cubicCoeff, d->quantiles, d->drop);
            }

            else if (fi->colorFamily == cmYUV)
            {
                // go around for yuv as min and max values differ for Y and UV planes
                float min = 0;
                float max = 1.0f;

                circularLensMagnification((float**)dp, (const float**)sp, 1, 
                    pitch, ht, wd, subW, subH, min, max,
                    rad, cx, cy, mag, d->span,
                    d->cubicCoeff, d->quantiles, d->drop);


                if (np > 1)
                {
                    // U,V planes 
                    float min = -0.5f;
                    float max = 0.5f;

                    circularLensMagnification((float**)(dp + 1), (const float**)(sp + 1), 2,
                        pitch, ht, wd, subW, subH, min, max,
                        rad, cx, cy, mag, d->span,
                        d->cubicCoeff, d->quantiles, d->drop);
                }
            }

        }
            
        
        vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC lensFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    LensData *d = (LensData *)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->cubicCoeff);
    free(d);
}

static void VS_CC lensCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    LensData d;
    LensData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat && !isConstantFormat(d.vi))
    {
        vsapi->setError(out, "Lens: only RGB, YUV, Gray constant formats, with integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }
    
    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "Lens: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "Lens: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        return;
    }

    int	rmax = d.vi->width > d.vi->height ? d.vi->height / 2 : d.vi->width / 2;

    d.irad = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
    if (err)
        d.irad = rmax / 4 > 4 ? rmax / 4 : 4;
    else if (d.irad < 4 || d.irad > rmax )
    {
        vsapi->setError(out, "Lens: rad must be 4 to half of frame smaller dimension");
        vsapi->freeNode(d.node);
        return;
    }

    d.erad = int64ToIntS(vsapi->propGetInt(in, "erad", 0, &err));
    if (err)
        d.erad = d.irad;
    else if (d.erad < 4 || d.erad > rmax)
    {
        vsapi->setError(out, "Lens: erad must be 4 to half of frame smaller dimension");
        vsapi->freeNode(d.node);
        return;
    }
    //-----------------
    
    d.ix = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
    if (err)
        d.ix = d.vi->width / 2;
    else if (d.ix < 4 || d.ix > d.vi->width - 4)
    {
        vsapi->setError(out, "Lens: x must be at least 4 pixels inside frame");
        vsapi->freeNode(d.node);
        return;
    }

    d.ex = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
    if (err)
        d.ex = d.ix;
    else if (d.ex < 4 || d.ex > d.vi->width - 4)
    {
        vsapi->setError(out, "Lens: ex must be at least 4 pixels inside frame");
        vsapi->freeNode(d.node);
        return;
    }

    d.iy = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
    if (err)
        d.iy = d.vi->height / 2;
    else if (d.iy < 4 || d.iy > d.vi->height - 4)
    {
        vsapi->setError(out, "Lens: y must be at least 4 pixels inside frame");
        vsapi->freeNode(d.node);
        return;
    }

    d.ey = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
    if (err)
        d.ey = d.iy;
    else if (d.ey < 4 || d.ey > d.vi->height - 4)
    {
        vsapi->setError(out, "Lens: ey must be at least 4 pixels inside frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.imag = (float)vsapi->propGetFloat(in, "mag", 0, &err);
    if (err)
        d.imag = 4;
    else if (d.imag < 1.5f || d.imag > 8.0f)
    {
        vsapi->setError(out, "Lens: mag must be within 1.5 to 8.0");
        vsapi->freeNode(d.node);
        return;
    }

    d.emag = (float)vsapi->propGetFloat(in, "emag", 0, &err);
    if (err)
        d.emag = d.imag;
    else if (d.emag < 1.5f || d.emag > 8.0f)
    {
        vsapi->setError(out, "Lens: emag must be within 1.5 to 8.0");
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
    
    data = (LensData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Lens", lensInit, lensGetFrame, lensFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
/*

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.lens.vfx", "Lens", "VapourSynth vfx plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Lens", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;mag:float:opt;drop:int:opt;"
    "x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;erad:int:opt;emag:float:opt;", lensCreate, 0, plugin);
}*/

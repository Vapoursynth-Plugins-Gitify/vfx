//////////////////////////////////////////
/******************************************************************************
The Binoculars Filter for avisynth+ creates twin magnified circular
windows which can be panned
The radius of viewing disc,  co-ords of the center point of the discs  (and final
co-ord) to be given. If final are omiited, the view remains stationary at initial co-ords
The co-ords must be within frame and ensure that the discs are also inside frame
Magnification can also be varied from start to end.
24 feb 2021
Author V.C. Mohan
*************************************************************************************************/


#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef* node;
    const VSVideoInfo* vi;
    int StartFrame;
    int EndFrame;
    int radius;			// radius of view
    int smagx;			// start magnification x

    int scenterx;		// start center x coord	
    int scentery;		// start center y coord
    int ecenterx;		// end center x coord
    int ecentery;		// end center y coord
    int emagx;			// ending magnification
    float* cubic;       // cubic interpolation coefficients buffer
    int quantiles;      // number of quants of interpolation
} BinocularsData;



static void VS_CC binocularsInit(VSMap* in, VSMap* out, void** instanceData,
                                VSNode* node, VSCore* core, const VSAPI* vsapi)
{
    BinocularsData* d = (BinocularsData*)*instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);
    // create LUT for cubic interpolation. Array of 4 X Quantiles
    d->quantiles = 64;
    d->cubic = (float*)vs_aligned_malloc<float>(sizeof(float) * 4 * (d->quantiles + 1), 32);
    // populate buffer with interpolation coefficients for quantile number of intervals
    CubicIntCoeff(d->cubic, d->quantiles);
}


static const VSFrameRef* VS_CC binocularsGetFrame(int in, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    BinocularsData* d = (BinocularsData*)*instanceData;

    if (activationReason == arInitial) {
        vsapi->requestFrameFilter(in, d->node, frameCtx);
    }
    else if (activationReason == arAllFramesReady) {
        const VSFrameRef* src = vsapi->getFrameFilter(in, d->node, frameCtx);

        if (in < d->StartFrame || in > d->EndFrame)
            return src;
        int n = in - d->StartFrame;
        int nframes = d->EndFrame - d->StartFrame + 1;

        VSFrameRef* dst = vsapi->copyFrame(src, core);
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
        // calculate values for current frame
        int centerx = d->scenterx + (n * (d->ecenterx - d->scenterx)) / nframes;
        int centery = d->scentery + (n * (d->ecentery - d->scentery)) / nframes;
        float magx = d->smagx + (float)((d->emagx - d->smagx) * n) / nframes;
        int radius = d->radius;

        int sx = (centerx - radius) < 0 ? 0 : (centerx - radius) > wd - 1 ? wd - 1 : (centerx - radius);
        int sy = (centery - radius) < 0 ? 0 : (centery - radius) > ht - 1 ? ht - 1 : (centery - radius);
        int ex = (centerx + radius) < 0 ? 0 : (centerx + radius) > wd - 1 ? wd - 1 : (centerx + radius);
        int ey = (centery + radius) < 0 ? 0 : (centery + radius) > ht - 1 ? ht - 1 : (centery + radius);

        int rsq = radius * radius;
        float mag = 1.0f / magx;

        for (int p = 0; p < np; p++)
        {
            sp[p] = vsapi->getReadPtr(src, p);
            dp[p] = vsapi->getWritePtr(dst, p);
            pitch[p] = vsapi->getStride(dst, p) / nbytes;
        }

        for (int h = sy; h <= ey; h++)
        {
            int hsq = (h - centery) * (h - centery);

            for (int w = sx; w < ex; w++)
            {
                if (hsq + (w - centerx) * (w - centerx) <= rsq)
                {
                    float ih = (h - centery) * mag + centery;
                    int ihy = (int)ih;
                    float fy = ih - ihy;
                    if (ihy < 1 || ihy >= ht - 1) continue;
                    int qy = (int)(fy * d->quantiles);

                    float iw = (w - centerx) * mag + centerx;
                    int iwx = (int)iw;
                    float fx = iw - iwx;

                    if (iwx < 1 || iwx >= wd - 1) continue;

                    int qx = (int)(fx * d->quantiles);

                    for (int p = 0; p < np; p++)
                    {
                        if (fi->sampleType == stInteger && nbytes == 1)
                        {
                            uint8_t* dpp = dp[p];
                            const uint8_t* spp = sp[p];
                            uint8_t min = 0, max = 255;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = 16;
                                max = 235;
                            }

                            if (p == 0 || (subH == 0 && subW == 0))
                            {

                                if (needNotInterpolate(sp[p] + ihy * pitch[p] + iwx, pitch[p], 1) )
                                {
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = spp[p + ihy * pitch[p] + iwx];

                                        if ((w - radius) >= 0 && (w - radius) < wd)
                                            dpp[h * pitch[p] + (w - radius)] = spp[p + ihy * pitch[p] + iwx];
                                }
                                else
                                {


                                    unsigned char val = clamp(LaQuantile(spp + (ihy)*pitch[p] + iwx, pitch[p], 4, qx, qy, d->cubic), min, max);

                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[h * pitch[p] + (w - radius)] = val;
                                }
                            }

                            else
                            {
                                if ((h & andH) == 0 && (w & andW) == 0)
                                {
                                    unsigned char val = spp[(ihy >> subH) * pitch[p] + (iwx >> subW)];
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w + radius) >> subW)] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w - radius) >> subW)] = val;
                                }
                            }
                        }

                        else if (fi->sampleType == stInteger && nbytes == 2)
                        {
                            uint16_t* dpp = (uint16_t*)dp[p];
                            const uint16_t* spp = (uint16_t*)sp[p];
                            uint16_t min = 0, max = (1 << nbits) - 1;

                            if (fi->colorFamily == cmYUV)
                            {
                                min = (uint16_t) (16 << (nbits - 8));
                                max = (uint16_t) (240 << (nbits - 8));
                            }

                            if (p == 0 || (subH == 0 && subW == 0))
                            {

                                if (needNotInterpolate(spp + ihy * pitch[p] + iwx, pitch[p], 1))
                                {
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = spp[p + ihy * pitch[p] + iwx];

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[h * pitch[p] + (w - radius)] = spp[p + ihy * pitch[p] + iwx];
                                }
                                else
                                {


                                    uint16_t val = clamp(LaQuantile(spp + (ihy)*pitch[p] + iwx, pitch[p], 4, qx, qy, d->cubic), min, max);

                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[h * pitch[p] + (w - radius)] = val;
                                }
                            }

                            else
                            {
                                if ((h & andH) == 0 && (w & andW) == 0)
                                {
                                    uint16_t val = spp[(ihy >> subH) * pitch[p] + (iwx >> subW)];
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w + radius) >> subW)] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w - radius) >> subW)] = val;
                                }
                            }
                        }

                        else if (fi->sampleType == stFloat)
                        {
                            float* dpp = (float*)dp[p];
                            const float* spp = (float*)sp[p];
                            float min = 0, max = 1.0;                            

                            if (p == 0 || (subH == 0 && subW == 0))
                            {
                                if ( p != 0 && fi->colorFamily == cmYUV)
                                {
                                    min = -0.5f;
                                    max = 0.5f;
                                }

                                if (needNotInterpolate(spp + ihy * pitch[p] + iwx, pitch[p], 1))
                                {
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = spp[p + ihy * pitch[p] + iwx];

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[h * pitch[p] + (w - radius)] = spp[p + ihy * pitch[p] + iwx];
                                }
                                else
                                {


                                    float val = clamp(LaQuantile(spp + (ihy)*pitch[p] + iwx, pitch[p], 4, qx, qy, d->cubic), min, max);

                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[h * pitch[p] + w + radius] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[h * pitch[p] + (w - radius)] = val;
                                }
                            }

                            else
                            {
                                if ((h & andH) == 0 && (w & andW) == 0)
                                {
                                    float val = spp[(ihy >> subH) * pitch[p] + (iwx >> subW)];
                                    if ((w + radius) >= 0 && (w + radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w + radius) >> subW)] = val;

                                    if ((w - radius) >= 0 && (w - radius) < wd)
                                        dpp[(h >> subH) * pitch[p] + ((w - radius) >> subW)] = val;
                                }
                            }
                        }
                    }
                }
            }
        }

        // your code here...
        vsapi->freeFrame(src);
        return dst;
    }

    return 0;
}

static void VS_CC binocularsFree(void* instanceData, VSCore* core, const VSAPI* vsapi) {
    BinocularsData* d = (BinocularsData*)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->cubic);
    free(d);
}

static void VS_CC binocularsCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi) {
    BinocularsData d;
    BinocularsData* data;

    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);
    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat)
    {
        vsapi->setError(out, "Binoculars: only RGB, YUV, Gray color formats, integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }
    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "Binoculars: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "Binoculars: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        return;
    }
    int temp = d.vi->height > d.vi->width ? d.vi->width / 8 : d.vi->height / 8;
    d.radius = int64ToIntS(vsapi->propGetInt(in, "rad", 0, &err));
    if (err)
        d.radius = temp;
    else if (d.radius < 8 || d.radius > 2 * temp)
    {
        vsapi->setError(out, "Binoculars: rad must be 8 to 1/4 th smaller frame dimension");
        vsapi->freeNode(d.node);
        return;
    }
    d.scenterx = int64ToIntS(vsapi->propGetInt(in, "sx", 0, &err));
    if (err)
        d.scenterx = d.radius / 2;
    else if (d.scenterx < 0 || d.scenterx > d.vi->width - 1)
    {
        vsapi->setError(out, "Binoculars: sx Binocular center x coord at start must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.scentery = int64ToIntS(vsapi->propGetInt(in, "sy", 0, &err));
    if (err)
        d.scentery = d.vi->height / 2;
    else if (d.scentery < 0 || d.scentery > d.vi->height - 1)
    {
        vsapi->setError(out, "Binoculars: sy Binocular center x coord at start must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.ecenterx = int64ToIntS(vsapi->propGetInt(in, "ex", 0, &err));
    if (err)
        d.ecenterx = d.vi->width - d.radius / 2;
    else if (d.ecenterx < 0 || d.ecenterx > d.vi->width - 1)
    {
        vsapi->setError(out, "Binoculars: ex Binocular center x coord at end must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.ecentery = int64ToIntS(vsapi->propGetInt(in, "ey", 0, &err));
    if (err)
        d.ecentery = d.vi->height / 2;
    else if (d.ecentery < 0 || d.ecentery > d.vi->height - 1)
    {
        vsapi->setError(out, "Binoculars: sy Binocular center x coord at end must be within frame");
        vsapi->freeNode(d.node);
        return;
    }

    d.smagx = int64ToIntS(vsapi->propGetInt(in, "mag", 0, &err));
    if (err)
        d.smagx = 2;
    else if (d.smagx < 0 || d.smagx > d.radius / 4)
    {
        vsapi->setError(out, "Binoculars: mag Binocular magnification at start must be 2 to 1/4th rad");
        vsapi->freeNode(d.node);
        return;
    }
    d.emagx = int64ToIntS(vsapi->propGetInt(in, "emag", 0, &err));
    if (err)
        d.emagx = d.smagx;
    else if (d.emagx < 0 || d.emagx > d.radius / 4)
    {
        vsapi->setError(out, "Binoculars: emag magnification at end must be 2 to rad/4");
        vsapi->freeNode(d.node);
        return;
    }

    data = (BinocularsData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Binoculars", binocularsInit, binocularsGetFrame, binocularsFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
    configFunc("com.example.binoculars", "binoculars", "VapourSynth Filter Skeleton", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Binoculars", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;sx:int:opt;sy:int:opt;ex:int:opt;"
        "ey:int:opt;mag:int:opt;emag:int:opt;", binocularsCreate, 0, plugin);
}
*/
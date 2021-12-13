//////////////////////////////////////////
// This file contains a simple flashes
// skeleton you can use to get started.
// With no changes it simply passes
// frames through.

#include "VapourSynth.h"
#include "VSHelper.h"

typedef struct {
    VSNodeRef *node;
    const VSVideoInfo *vi;

    int StartFrame;
    int EndFrame;
    int xcoord;
    int ycoord;
    int radmax;		// max radius to which the smoke cloud expand
   // float Tsmoke;	// smoke filling time in seconds
  //  float Tflash;	// flash duration seconds
    int nFlashes;
    int nSmoke;
    int ntFlash;
    int* coord;
} FlashesData;

static void VS_CC flashesInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    FlashesData *d = (FlashesData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

    d-> nFlashes = 200;
    d->coord = (int *)vs_aligned_malloc(sizeof(int) * d->nFlashes, 32);

    srand(((d->EndFrame - d->StartFrame) * (d->EndFrame - d->StartFrame)
        * (d->EndFrame - d->StartFrame)) | 1);
    for (int i = 0; i < d->nFlashes; i++)
    {
        // flash coordinates - rad to + rad wrt cloud center
        d->coord[i] = - d->radmax + rand() % (2 * d->radmax);

    }
}

static const VSFrameRef *VS_CC flashesGetFrame(int in, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    FlashesData *d = (FlashesData *) * instanceData;

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

        VSFrameRef* dst = vsapi->copyFrame(src, core);

        const unsigned char* sp[] = { NULL, NULL, NULL };
        unsigned char* dp[] = { NULL, NULL, NULL };
        int subH[] = { 0, fi->subSamplingH, fi->subSamplingH };
        int subW[] = { 0, fi->subSamplingW, fi->subSamplingW };
        int andH = (1 << subH[1]) - 1;
        int andW = (1 << subW[1]) - 1;

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
        // get upto 24 flashes in a frame lasting for 1 second each. Each frame starts new flashes
    // first 4 seconds frames smoke to fill area. Flashes to start then

        int nSmoke = d->nSmoke;	// number of frames to fill smoke 
        int flash_duration = d->ntFlash;
        int smoke_radius = n < nSmoke ? (n * d->radmax) / nSmoke : d->radmax;
        int index = (n - nSmoke);
        int nflashes = 2 * (n > nSmoke ? (n - nSmoke > flash_duration ? flash_duration : n - nSmoke) : 0);
        int fcycle = flash_duration / 4;	// 4 times the point flashes before disappearing 

        int add[] = { 0, 127, 127 };	// required for float data
            // fill with smoke
        for (int h = d->ycoord - smoke_radius; h < d->ycoord + smoke_radius; h++)
        {
            if (h > 2 && h < ht - 2)
            {
                for (int w = d->xcoord - smoke_radius; w < d->xcoord + smoke_radius; w++)
                {
                    if (w > 2 && w < wd - 2)
                    {
                        // to create wooly effect
                        int delta = smoke_radius - rand() % (2 + smoke_radius / 8);	// to create hazy boundary

                        if ((w - d->xcoord) * (w - d->xcoord) + (h - d->ycoord) * (h - d->ycoord) <= delta * delta)
                        {
                            uint8_t grey[] = { (uint8_t)((rand() % 60) + 60), (uint8_t)127, (uint8_t)127 };// for changing smoke grey value

                            if (fi->colorFamily == cmRGB)
                            {
                                grey[1] = grey[0], grey[2] = grey[0];

                            }
                            for (int p = 0; p < np; p++)
                            {
                                if (nbytes == 1)

                                    *(dp[p] + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = grey[p];

                                else if (nbytes == 2)

                                    *((uint16_t*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = (uint16_t)((int)grey[p] << (nbits - 8));

                                else if (nbytes == 4)

                                    *((float*)(dp[p]) + (h >> subH[p]) * pitch[p] + (w >> subW[p])) = (grey[p] - add[p]) / 256.0f;

                            }
                        }
                    }
                }
            }
        }

        for (int f = 0; f < nflashes; f++)
        {
            int factor = (abs(flash_duration / 2 - f)) % fcycle;		// flashing color multiplier
            unsigned char *color;
            unsigned char coloryuv[] = { 0,0,0 };
            unsigned char colorrgb[] = { (uint8_t)(255 - (factor * 100) / (fcycle)), (uint8_t)20, (uint8_t)20 };
            // unable to understand logic. Internally supposed to be in B,G,R order
            // but gives wrong colors. so this illogical conversion
            if (fi->colorFamily == cmYUV)
            {    
                unsigned char colorbgr[] = { (uint8_t)20, (uint8_t)20, (uint8_t)(255 - (factor * 100) / (fcycle)) };
            
                BGR8YUV(coloryuv, colorbgr);
                color = coloryuv;
            }
            else
            {                
                color = colorrgb;
            }

            int m = (index - f) % d->nFlashes;	// for y
            int nf = (m + 1) % d->nFlashes;		// for x	this way  effectively double number of random coordinates
            int w = (d->xcoord + d->coord[nf]) & 0xfffffffe;
            int h = (d->ycoord + d->coord[m]) & 0xfffffffe;
            // ensure flashes occur within 7/8 th of smoke cloud
            if (d->coord[m] * d->coord[m] + d->coord[nf] * d->coord[nf] <= (7 * d->radmax * 7 * d->radmax) / 64) 	// within smoke
            {
                if (d->ycoord + d->coord[m] > 8 && d->ycoord + d->coord[m] < ht - 8)				// within frame height
                {
                    if (d->xcoord + d->coord[nf] > 8 && d->xcoord + d->coord[nf] < wd - 8)		// within frame width
                    {
                        // body of flash 
                        for (int i = 0; i < 8; i++)
                        {
                            for (int p = 0; p < np; p++)
                            {
                                if (fi->sampleType == stInteger && nbytes == 1)
                                {
                                    *(dp[p] + ((h + i) >> subH[p]) * pitch[p] + (w >> subW[p])) 
                                        = color[p];
                                    *(dp[p] + (h >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = color[p];
                                    *(dp[p] + ((h + i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = color[p];
                                    *(dp[p] + ((h + 8 - i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = color[p];
                                }

                                else if (fi->sampleType == stInteger && nbytes == 2)
                                {
                                    *( (uint16_t*)(dp[p]) + ((h + i) >> subH[p]) * pitch[p] + (w >> subW[p])) 
                                        = (uint16_t)(((int)color[p]) << (nbits - 8));
                                    *((uint16_t*)(dp[p]) + (h >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = (uint16_t)(((int)color[p]) << (nbits - 8));
                                    *((uint16_t*)(dp[p]) + ((h + i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = (uint16_t)(((int)color[p]) << (nbits - 8));
                                    *((uint16_t*)(dp[p]) + ((h + 8 - i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p])) 
                                        = (uint16_t)(((int)color[p]) << (nbits - 8));
                                }

                                else if (fi->sampleType == stFloat && nbytes == 4)
                                {
                                    float col = color[p] / 255.0f;;
                                    if (fi->colorFamily == cmYUV)
                                        if (p == 0)
                                            col = (color[p] - 16) / 235.0f;
                                        else
                                            col = (color[p] - 128)/ 235.0f;

                                    *((float*)(dp[p]) + ((h + i) >> subH[p]) * pitch[p] + (w >> subW[p]))
                                        = (col);
                                    *((float*)(dp[p]) + (h >> subH[p]) * pitch[p] + ((w + i) >> subW[p]))
                                        = (col);
                                    *((float*)(dp[p]) + ((h + i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p]))
                                        = (col);
                                    *((float*)(dp[p]) + ((h + 8 - i) >> subH[p]) * pitch[p] + ((w + i) >> subW[p]))
                                        = (col);
                                }
                            }
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

static void VS_CC flashesFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    FlashesData *d = (FlashesData *)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->coord);
    free(d);
}

static void VS_CC flashesCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    FlashesData d;
    FlashesData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);

    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
        && fi->sampleType != stInteger && fi->sampleType != stFloat && !isConstantFormat(d.vi))
    {
        vsapi->setError(out, "Flashes: only RGB, YUV, Gray constant formats, with integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }


    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "Flashes: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame < d.StartFrame)
    {
        vsapi->setError(out, "Flashes: ef must be within video and not less than sf");
        vsapi->freeNode(d.node);
        return;
    }

    d.xcoord = int64ToIntS(vsapi->propGetInt(in, "x", 0, &err));
    if (err)
        d.xcoord = d.vi->width / 2;
    else if (d.xcoord < 0 || d.xcoord > d.vi->width - 1 )
    {
        vsapi->setError(out, "Flashes: x must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.ycoord = int64ToIntS(vsapi->propGetInt(in, "y", 0, &err));
    if (err)
        d.ycoord = d.vi->height / 2;
    else if (d.ycoord < 0 || d.ycoord > d.vi->height - 1)
    {
        vsapi->setError(out, "Flashes: y must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    int	rmax = d.vi->width > d.vi->height ? d.vi->height / 2 : d.vi->width / 2;
    d.radmax = int64ToIntS(vsapi->propGetInt(in, "rmax", 0, &err));
    if (err)
        d.radmax = rmax / 2;
    else if (d.radmax < 8 || d.radmax > rmax)
    {
        vsapi->setError(out, "Flashes: rmax value can be 8 to 1/2 of smaller dimension of frame");
        vsapi->freeNode(d.node);
        return;
    }
    float tmax = (float)( ((d.EndFrame - d.StartFrame) * d.vi->fpsNum) / d.vi->fpsDen);
    float ts = (float)(vsapi->propGetFloat(in, "ts", 0, &err));
    if (err)
        d.nSmoke = (int)(2.0 * d.vi->fpsNum / d.vi->fpsDen);
    else if (ts < 0 || ts > tmax)
    {
        vsapi->setError(out, "Flashes: ts time in seconds for smoke cloud to expand to max rmax must be within effect duration in seconds");
        vsapi->freeNode(d.node);
        return;
    }
    else
        d.nSmoke = (int)ts;

    ts = (float)(vsapi->propGetFloat(in, "tf", 0, &err));
    if (err)
        d.ntFlash = (int)(0.25 * d.vi->fpsNum / d.vi->fpsDen);
    else if (ts < 0 || ts > tmax)
    {
        vsapi->setError(out, "Flashes: tf time in seconds each flash lasts must be 0.25 to 1.0 seconds");
        vsapi->freeNode(d.node);
        return;
    }
    else d.ntFlash = (int)ts;

    if ( d.ntFlash + d.nSmoke > d.EndFrame - d.StartFrame)    
    {
        vsapi->setError(out, "Flashes: tf + ts must be at least cover duration of effect");
        vsapi->freeNode(d.node);
        return;
    }
    data = (FlashesData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Flashes", flashesInit, flashesGetFrame, flashesFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
/*

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.flashes.vfx", "Flashes", "VapourSynth vfx plugin", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Flashes", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;rmax:int:opt;ts:float:opt;tf:int:opt;", flashesCreate, 0, plugin);
}*/

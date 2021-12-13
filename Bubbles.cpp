//////////////////////////////////////////
/*---------------------------------------------------------------------------------------
Bubbles a function of vfx vapoursynth plugin 
Creates bubbles from specified source white or colored with a life span 
Bubbles travel max up to a specified point. 
Author V.C.Mohan.
Date 22 Feb 2021
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
    int StartFrame; // :0
    int EndFrame;   // :last frame
    int srcx;       // bubbles source x coord: 0
    int srcy;       // bubbles source y coord: frame height
    int farx;       // how far bubbles travel along x : width - srcx
    int floory;     // y coord of floor below which bubbles will not go : srcy
    int rise;       // max rise of bubbles above srcy : srcy
    bool color;     // Are bubbles colored?: true
    int life;       // how many frames bubbles persist to live: (EndFrame - startframe) / 2 or 200
    int radius;	    //radius of bubbles : 8
    int nbf;        // number of new bubblescreated in frame
    int* px, * py; 	// parabola px values
} BubblesData;

static void VS_CC bubblesInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    BubblesData *d = (BubblesData *) * instanceData;
    vsapi->setVideoInfo(d->vi, 1, node);

    //int radius = 8;
   // d->nbf = 200;
    d->px = (int *) vs_aligned_malloc<int>(sizeof(int)* 2 * d->life * d->nbf, 32);
    d->py = d->px + d->nbf * d->life;
    srand (((d->EndFrame - d->StartFrame) * (d->EndFrame - d->StartFrame)
        * (d->EndFrame - d->StartFrame)) | 1 );
    for (int i = 0; i < d->nbf * d->life; i++)
    {
        // trajectory constants for 200 bubbles
        d->px[i] = (rand() % (d->farx - d->srcx)) + 10 * d->radius;
        d->py[i] = 10 * d->radius + (rand() % (d->rise - 10 * d->radius));
    }

 }

static const VSFrameRef* VS_CC bubblesGetFrame(int in, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi) {
    BubblesData* d = (BubblesData*)*instanceData;

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

        unsigned char* dp[] = { NULL, NULL, NULL };
        int subH = fi->subSamplingH;
        int subW = fi->subSamplingW;
        int andH = (1 << subH) - 1;
        int andW = (1 << subW) - 1;

        int dpitch[] = { 0,0,0 };
        int nbytes = fi->bytesPerSample;
        int nbits = fi->bitsPerSample;
        int np = fi->numPlanes > 3 ? 3 : fi->numPlanes;

        int ht = d->vi->height;
        int wd = d->vi->width;

        for (int p = 0; p < np; p++)
        {
            dp[p] = vsapi->getWritePtr(dst, p);
            dpitch[p] = vsapi->getStride(dst, p) / nbytes;
        }

        int nbbls = d->life * d->nbf;
        // each frame nbf new bubbles start. after n = life remains 200 total living
      //  int nmax = n > d->life ? n + d->life : n + n + 1;
        int nmax = n  > d->life ? n  + nbbls :   n * d->nbf  + n;
        
        for (int nx = n; nx < nmax; nx++)
        {		// origin of parabola 0,0 is  frame coords px+srcx, srcy-py=0
                // the parabola coord of bubble origin is px[nx],py=srcx
            int modnx = nx % nbbls; // 
            // int modnx = nx % d->nbf;
            float pp = (d->px[modnx] * d->px[modnx]) / (4.0f * (d->py[modnx]));	// parameter p of parabola

            int dx = ((d->farx - d->srcx) % (2 * d->px[modnx])) * (nmax - nx) / (nbbls / 2 + (nmax - nx) / 2);
            // distance interval travel by bubble relative to source
            // in the denominator (nmax-nx)/2 is to slow down bubbles progressively						
            // in their travel towards farx
            int xx = d->px[modnx] - dx;	// xx relative to parabola zero
            int fx = d->srcx + dx;

            if (d->farx < d->srcx)
                xx = d->px[modnx] + dx;	// relative x

            int yy = (int)((xx * xx) / (4 * pp));				//yy relative to parabola zero

            int fy = d->srcy - d->py[modnx] + yy;       // frame y
                    // limits of check to ensure minimum search
            
            int bradius = 5 + d->px[modnx] % 10;	//  radius value dependant on px to get some variation of size

            int rsq = bradius * bradius;
            int r1sq = (bradius - 1) * (bradius - 1);   // rsq to r1sq is outer rim
            int r4sq = (bradius - 4) * (bradius - 4);   // r4sq  and r6sq is for glint
            int r6sq = (bradius - 6) * (bradius - 6);   // even if radius becomes -ve square sts right
            int sx = fx - bradius;
            int sy = fy - bradius;
            int ex = fx + bradius;
            int ey = fy + bradius;
            // using px and pp which are random but continue frame to frame
            //for consistant but random color of bubble
            int red = d->px[modnx] % 140;
            int green = ((int)pp) % 140;
            int blue = (2 * (red + green)) % 140;
            if (!d->color)
            {
                red = 128;
                green = 128;
                blue = 128;
            }

            unsigned char bgr[] = { (uint8_t)blue, (uint8_t)green, (uint8_t)red }, BGR[] = { (uint8_t)200, (uint8_t)200, (uint8_t)200 };
            unsigned char yuv[3], YUV[3];
            unsigned char* col, * Gray;

            if (fi->colorFamily != cmRGB)
            {
                BGR2YUV(bgr, yuv);	// for use of YUV formats
                BGR2YUV(BGR, YUV);
                if (d->color)
                {
                    col = yuv;
                    Gray = YUV;
                }
                else
                {
                    col = YUV;
                    Gray = YUV;
                }
            }
            else
            {
                if (d->color)
                {
                    col = bgr;
                    Gray = BGR;
                }
                else
                {
                    col = BGR;
                    Gray = BGR;
                }
            }

            float max = fi->colorFamily == cmRGB ? 255.0f : 235.0f;

            if (sy > 0 && sy < d->floory && ey < d->floory && ey > d->srcy - d->rise)
            {
                int hdpitch = (sy)* dpitch[0];

                for (int h = sy; h < ey; h++)
                {

                    int hsq = (h - fy) * (h - fy);

                    for (int w = sx; w < ex; w++)
                    {
                        int radsq = hsq + (w - fx) * (w - fx);

                        if (radsq <= rsq)
                        {

                            for (int p = 0; p < np; p++)
                            {
                                if (fi->sampleType == stInteger)
                                {
                                    if (nbytes == 1)
                                    {
                                        unsigned char hue = col[p];
                                        unsigned char HUE = Gray[p];
                                        //  unsigned char val = *(dp[p] + (h >> subH) * dpitch[p] + (w >> subW));
                                        if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                                        {

                                            if (radsq > (r1sq))
                                            {
                                                *(dp[p] + (h)*dpitch[p] + (w)) = HUE;

                                            }
                                            // To get a glint
                                            else if (radsq < (r4sq)
                                                && radsq >(r6sq)
                                                && (w < fx + 3 && w > fx - 3))
                                            {
                                                *(dp[p] + (h)*dpitch[p] + (w)) = HUE;

                                            }
                                            else 
                                            {
                                                unsigned char val = *(dp[p] + (h)*dpitch[p] + (w));
                                                *(dp[p] + (h)*dpitch[p] + (w)) = val > hue ?
                                                    (val * 2 + hue) / 3 : hue;
                                            }

                                        }
                                        else
                                        {
                                            if ((h & andH) == 0 && (w & andW) == 0)
                                            {
                                                
                                                if (radsq > (r1sq))
                                                {
                                                    *(dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                                }
                                                // To get a glint
                                                else if (radsq < (r4sq)
                                                    && radsq >(r6sq)
                                                    && (w < fx + 3 && w > fx - 3))
                                                {
                                                    *(dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                                }
                                                else 
                                                {
                                                    unsigned char val = *(dp[p] + (h >> subH) * dpitch[p] + (w >> subW));
                                                    *(dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = val > hue ?
                                                        (val * 2 + hue) / 3 : hue;

                                                }
                                            }
                                        }
                                    }

                                    else if ( nbytes == 2)
                                    {
                                        uint16_t hue = (uint16_t)( (int)col[p] << (nbits - 8) );
                                        uint16_t HUE = (uint16_t)( (int)Gray[p] << (nbits - 8) );
                                        
                                        if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                                        {

                                            if (radsq > (r1sq))
                                            {
                                                *((uint16_t*)dp[p] + (h)*dpitch[p] + (w)) =  HUE;

                                            }
                                            // To get a glint
                                            else if (radsq < (r4sq)
                                                && radsq >(r6sq)
                                                && (w < fx + 3 && w > fx - 3))
                                            {
                                                *((uint16_t*)dp[p] + (h)*dpitch[p] + (w)) =  HUE;

                                            }
                                            else 
                                            {
                                                uint16_t val = *((uint16_t*)dp[p] + (h)*dpitch[p] + (w));
                                                *( (uint16_t*)dp[p] + (h)*dpitch[p] + (w)) = val > hue ?
                                                    (val * 2 + hue) / 3 : hue;
                                            }

                                        }
                                        else
                                        {
                                            if ((h & andH) == 0 && (w & andW) == 0)
                                            {
                                                uint16_t val = *( (uint16_t*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW));
                                                *( (uint16_t*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = val > hue ?
                                                    (val * 2 + hue) / 3 : hue;

                                                if (radsq > (r1sq))
                                                {
                                                    *((uint16_t*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                                }
                                                // To get a glint
                                                else if (radsq < (r4sq)
                                                    && radsq >(r6sq)
                                                    && (w < fx + 3 && w > fx - 3))
                                                {
                                                    *((uint16_t*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                                }
                                                else
                                                {
                                                    uint16_t val = *((uint16_t*)dp[p] + (h >> subH)*dpitch[p] + (w >> subW));
                                                    *((uint16_t*)dp[p] + (h >> subH)*dpitch[p] + (w >> subW)) = val > hue ?
                                                        (val * 2 + hue) / 3 : hue;
                                                }
                                            }
                                        }
                                    }

                                }
                                else if (fi->sampleType == stFloat)
                                {
                                
                                    float hue = (float)col[p] / max;
                                    float HUE = (float)Gray[p] / max;
                                    if (p != 0 && fi->colorFamily == cmYUV)
                                    {
                                        hue -= 0.5f;
                                        HUE -= 0.5f;
                                    }

                                    if (p == 0 || fi->colorFamily == cmRGB || (subH == 0 && subW == 0))
                                    {

                                        if (radsq > (r1sq))
                                        {
                                            *((float*)dp[p] + (h)*dpitch[p] + (w)) = HUE;

                                        }
                                        // To get a glint
                                        else if (radsq < (r4sq)
                                            && radsq >(r6sq)
                                            && (w < fx + 3 && w > fx - 3))
                                        {
                                            *((float*)dp[p] + (h)*dpitch[p] + (w)) = HUE;

                                        }
                                        else
                                        {
                                            float val = *((float*)dp[p] + (h)*dpitch[p] + (w));
                                            *((float*)dp[p] + (h)*dpitch[p] + (w)) = val > hue ?
                                                (val * 2 + hue) / 3 : hue;
                                        }

                                    }
                                    else
                                    {
                                        if ((h & andH) == 0 && (w & andW) == 0)
                                        {
                                            float val = *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW));
                                            *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = val > hue ?
                                                (val * 2 + hue) / 3 : hue;

                                            if (radsq > (r1sq))
                                            {
                                                *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                            }
                                            // To get a glint
                                            else if (radsq < (r4sq)
                                                && radsq >(r6sq)
                                                && (w < fx + 3 && w > fx - 3))
                                            {
                                                *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = HUE;

                                            }
                                            else
                                            {
                                                float val = *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW));
                                                *((float*)dp[p] + (h >> subH) * dpitch[p] + (w >> subW)) = val > hue ?
                                                    (val * 2 + hue) / 3 : hue;
                                            }
                                        }
                                    }
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

static void VS_CC bubblesFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    BubblesData *d = (BubblesData *)instanceData;
    vsapi->freeNode(d->node);
    vs_aligned_free(d->px);
    free(d);
}

static void VS_CC bubblesCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    BubblesData d;
    BubblesData *data;
    int err;

    d.node = vsapi->propGetNode(in, "clip", 0, 0);
    d.vi = vsapi->getVideoInfo(d.node);
    const VSFormat* fi = d.vi->format;

    if (fi->colorFamily != cmRGB && fi->colorFamily != cmYUV && fi->colorFamily != cmGray
         && fi->sampleType != stInteger && fi->sampleType != stFloat)
    {
        vsapi->setError(out, "Bubbles: only RGB, YUV, Gray color formats, integer or float samples as Input allowed ");
        vsapi->freeNode(d.node);
        return;
    }
    d.StartFrame = int64ToIntS(vsapi->propGetInt(in, "sf", 0, &err));
    if (err)
        d.StartFrame = 0;
    else if (d.StartFrame < 0 || d.StartFrame > d.vi->numFrames - 1)
    {
        vsapi->setError(out, "Bubbles: sf must be within video");
        vsapi->freeNode(d.node);
        return;
    }
    d.EndFrame = int64ToIntS(vsapi->propGetInt(in, "ef", 0, &err));
    if (err)
        d.EndFrame = d.vi->numFrames - 1;
    else if (d.EndFrame < 0 || d.EndFrame > d.vi->numFrames - 1 || d.EndFrame <= d.StartFrame)
    {
        vsapi->setError(out, "Bubbles: ef must be within video and greater than sf");
        vsapi->freeNode(d.node);
        return;
    }
    d.srcx = int64ToIntS(vsapi->propGetInt(in, "sx", 0, &err));
    if (err)
        d.srcx = 8;
    else if (d.srcx < 0 || d.srcx > d.vi->width - 1)
    {
        vsapi->setError(out, "Bubbles: sx source of bubbles must be within frame");
        vsapi->freeNode(d.node);
        return;
    }
    d.srcy = int64ToIntS(vsapi->propGetInt(in, "sy", 0, &err));
    if (err)
        d.srcy = d.vi->height - 1;
    else if (d.srcy < 32 || d.srcy > d.vi->height / 2)
    {
        vsapi->setError(out, "Bubbles: sy source of bubbles must be 32 to less than half of frame height");
        vsapi->freeNode(d.node);
        return;
    }
    d.farx = int64ToIntS(vsapi->propGetInt(in, "farx", 0, &err));
    if (err)
        d.farx = d.vi->width - d.srcx;
    else if (d.farx < 32 || d.farx > d.vi->width - 1 || abs (d.farx - d.srcx) > d.vi->width )
    {
        vsapi->setError(out, "Bubbles: farx fartest x coord to which bubbles travel must be 0 to less than frame width");
        vsapi->freeNode(d.node);
        return;
    }
    d.rise = int64ToIntS(vsapi->propGetInt(in, "rise", 0, &err));
    if (err)
        d.rise = d.srcy;
    else if (d.rise < 180 || d.rise > d.srcy )
    {
        vsapi->setError(out, "Bubbles: rise must be between 180 and sy");
        vsapi->freeNode(d.node);
        return;
    }
    d.floory = int64ToIntS(vsapi->propGetInt(in, "floory", 0, &err));
    if (err)
        d.floory = d.srcy;
    else if (d.floory < 0 || d.floory > d.srcy)
    {
        vsapi->setError(out, "Bubbles: floory lowest point Y coord to which bubbles travel must be 0 to less than sy");
        vsapi->freeNode(d.node);
        return;
    }

    d.radius = int64ToIntS(vsapi->propGetInt(in, "radius", 0, &err));
    if (err)
        d.radius = 8;
    else if (d.radius < 6 || d.radius > 16)
    {
        vsapi->setError(out, "Bubbles: radius must be 6 to 16");
        vsapi->freeNode(d.node);
        return;
    }
    int temp = !! int64ToIntS(vsapi->propGetInt(in, "color", 0, &err));
    if (err)
        d.color = true;
    else if (temp == 0)
        d.color = false;
    else
        d.color = true;

    
    d.life = int64ToIntS(vsapi->propGetInt(in, "life", 0, &err));
    if (err)
        d.life = ((d.EndFrame - d.StartFrame) >> 1) > 50? 50 : ((d.EndFrame - d.StartFrame) >> 1);
    else if (d.life < 2 || d.life > d.EndFrame - d.StartFrame)
    {
        vsapi->setError(out, "Bubbles: life must be 2  to 100 and half of frames in this bubbles effect");
        vsapi->freeNode(d.node);
        return;
    }
    
    d.nbf = int64ToIntS(vsapi->propGetInt(in, "nbf", 0, &err));
    if (err)
        d.nbf = 4;
    else if (d.nbf < 1 || d.nbf  > 8)
    {
        vsapi->setError(out, "Bubbles: nbf must be 1 to 8 ");
        vsapi->freeNode(d.node);
        return;
    }
    

    data = (BubblesData*)malloc(sizeof(d));
    *data = d;

    vsapi->createFilter(in, out, "Bubbles", bubblesInit, bubblesGetFrame, bubblesFree, fmParallel, 0, data, core);
}

//////////////////////////////////////////
// Init
/*
VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.example.bubbles", "bubbles", "VapourSynth bubbles Skeleton", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("bubbles", "clip:clip;sf:int:opt;ef:int:opt;sx:int:opt;sy:int:opt;farx:int:opt;floory:int:opt;"
        "rad:int:opt;rise:int:opt;life:int:opt;nbf:int:opt;", bubblesCreate, 0, plugin);
}
*/
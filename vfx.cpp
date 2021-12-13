/*---------------------------------------------------------------------------------------
vfx is a vapoursynth plugin and has several functions that modify, move pixel values. 
Accepts variable frame sizes and formats. Mostly applicable to images rather than videos.


Author V.C.Mohan.
Date 28 Dec 2020, 16 oct 2021
copyright  2020- 2021

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
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <fstream>

#define _USE_MATH_DEFINES
#include "math.h"
#include "VapourSynth.h"
#include "VSHelper.h"


#include "interpolationMethods.h"
#include "statsAndOffsetsLUT.h"
#include "colorconverter.h"
#include "ConvertBGRforInput.h"
#include "lensMagnification.h"
#include "spotlightDim.h"
#include "raysAndFlowers.h"
#include "FisheyeMethods.h"
#include "FourFoldSymmetricMarking.h"
#include "Squircles.h"

#include "Balloon.cpp"
//#include "Bokeh3d.cpp"
#include "Bubbles.cpp"
#include "Binoculars.cpp"
#include "Conez.cpp"
#include "DiscoLights.cpp"
#include "Flashes.cpp"
#include "FiguredGlass.cpp"
#include "FlowerPot.cpp"
#include "Fog.cpp"
#include "Lens.cpp"
# include "LineMagnifier.cpp"
#include "Pool.cpp"
#include "Rain.cpp"
#include "Rainbow.cpp"
#include "RearViewMirror.cpp"
#include "Ripple.cpp"
#include "Rockets.cpp"
#include "Snow.cpp"
#include "SnowStorm.cpp"
#include "Sparkler.cpp"
#include "SpotLight.cpp"
#include "SunFlower.cpp"
#include "Swirl.cpp"





VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin) {
	configFunc("com.mohanvc.vfx", "vfx", "Special Effects ", VAPOURSYNTH_API_VERSION, 1, plugin);
	
	registerFunc("Balloon", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;color:int[]:opt;opacity:float:opt;nhops:int:opt;"
		"rise:int:opt;sx:int:opt;fx:int:opt;fy:int:opt;light:int:opt;refl:float:opt;offset:float:opt;"
		"lx:int:opt;ly:int:opt;", balloonCreate, 0, plugin);

	/*registerFunc("Bokeh3D", "clip:clip;sf:int:opt;ef:int:opt;grid:int:opt;rmax:int:opt;"
							"drad:int:opt;theeta:int:opt;proc:int[]:opt;thresh:float:opt;"
							"mode:int:opt;x:int:opt;y:int:opt;range:float:opt", bokeh3dCreate, 0, plugin);
	*/
	registerFunc("Bubbles", "clip:clip;sf:int:opt;ef:int:opt;sx:int:opt;sy:int:opt;farx:int:opt;floory:int:opt;"
							"rad:int:opt;rise:int:opt;life:int:opt;nbf:int:opt;", bubblesCreate, 0, plugin);

	registerFunc("Binoculars", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;sx:int:opt;sy:int:opt;ex:int:opt;"
								"ey:int:opt;mag:int:opt;emag:int:opt;", binocularsCreate, 0, plugin);

	registerFunc("Conez", "clip:clip;bkg:clip;sf:int:opt;ef:int:opt;vert:int:opt;prog:int:opt;"
							"top:int:opt;base:int:opt;", conezCreate, 0, plugin);

	registerFunc("DiscoLights", "clip:clip;sf:int:opt;ef:int:opt;life:int:opt;type:int:opt;"
							"nspots:int:opt;minrad:int:opt;dim:float:opt;", discolightsCreate, 0, plugin);

	registerFunc("Flashes", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;rmax:int:opt;"
							"ts:float:opt;tf:int:opt;", flashesCreate, 0, plugin);

	registerFunc("FiguredGlass", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;"
						"mag:float:opt;drop:int:opt;", figuredglassCreate, 0, plugin);
	
	registerFunc("FlowerPot", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;rise:int:opt;"
					"ex:int:opt;ey:int:opt;zoom:float:opt;color:int:opt;", flowerpotCreate, 0, plugin);

	registerFunc("Fog", "clip:clip;sf:int:opt;ef:int:opt;fog:float:opt;efog:float:opt;"
							"vary:float:opt;", fogCreate, 0, plugin);

	registerFunc("Lens", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;mag:float:opt;drop:int:opt;"
			"x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;erad:int:opt;emag:float:opt;", lensCreate, 0, plugin);
	
	registerFunc("LineMagnifier", "clip:clip;sf:int:opt;ef:int:opt;lwidth:int:opt;mag:float:opt;drop:int:opt;"
				"xy:int:opt;exy:int:opt;vert:int:opt;", linemagnifierCreate, 0, plugin);

	registerFunc("Pool", "clip:clip;sf:int:opt;ef:int:opt;x:int:opt;y:int:opt;ex:int:opt;"
				"ey:int:opt;wd:int:opt;ewd:int:opt;ht:int:opt;eht:int:opt;wavelen:int:opt;"
				"amp:int:opt;eamp:int:opt;speed:float:opt;espeed:float:opt;"
				"paint:int:opt;color:int[]:opt;", poolCreate, 0, plugin);

	registerFunc("Rain", "clip:clip;sf:int:opt;ef:int:opt;type:int:opt;etype:int:opt;"
					"slant:int:opt;eslant:int:opt;opq:float:opt;box:int:opt;span:int:opt;", rainCreate, 0, plugin);

	registerFunc("Rainbow", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;erad:int:opt;"
					"x:int:opt;ex:int:opt;y:int:opt;ey:int:opt;"
					"lx:int:opt;elx:int:opt;rx:int:opt;erx:int:opt;", rainbowCreate, 0, plugin);
	registerFunc("RearViewMirror", "clip:clip;bclip:clip;method:int:opt;"
				"mcx:int:opt;mcy:int:opt;mwd:int:opt;mht:int:opt;oval:int:opt;border:int:opt;"
				"cvx:float:opt;fov:float:opt;test:int:opt;dim:float:opt;q:int:opt;dots:int:opt;", rearviewmirrorCreate, 0, plugin);

	registerFunc("Ripple", "clip:clip;sf:int:opt;ef:int:opt;wavelen:int:opt;speed:float:opt;"
		"espeed:float:opt;poolx:int:opt;pooly:int:opt;"
		"wd:int:opt;ht:int:opt;origin:int:opt;xo:int:opt;yo:int:opt;"
		"amp:int:opt;eamp:int:opt;ifr:int:opt;dfr:int:opt;", rippleCreate, 0, plugin);

	registerFunc("Rockets", "clip:clip;sf:int:opt;ef:int:opt;life:float:opt;interval:float:opt;"
						"lx:int:opt;rx:int:opt;y:int:opt;rise:int:opt;"
						"target:int:opt;tx:int:opt;ty:int:opt;", rocketsCreate, 0, plugin);
	
	registerFunc("Snow", "clip:clip;sf:int:opt;ef:int:opt;density:int:opt;"
						"big:int:opt;fall:int:opt;drift:int:opt;", snowCreate, 0, plugin);

	registerFunc("SnowStorm", "clip:clip;sf:int:opt;ef:int:opt;type:int[]:opt;", snowstormCreate, 0, plugin);

	registerFunc("Sparkler", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;"
		"x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;color:int:opt;", sparklerCreate, 0, plugin);

	registerFunc("SpotLight", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;"
		"x:int:opt;y:int:opt;ex:int:opt;ey:int:opt;rgb:int[]:opt;dim:float:opt", spotlightCreate, 0, plugin);

	registerFunc("SunFlower", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;x:int:opt;y:int:opt;"
		"ex:int:opt;ey:int:opt;color:int:opt;gravity:float:opt;persistance:float:opt;", sunflowerCreate, 0, plugin);

	registerFunc("Swirl", "clip:clip;sf:int:opt;ef:int:opt;rad:int:opt;x:int:opt;y:int:opt;"
		"q:int:opt;dir:int:opt;grow:int:opt;steady:int:opt;", swirlCreate, 0, plugin);
}
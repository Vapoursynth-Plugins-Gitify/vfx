#pragma once
#ifndef FIREWORKS_RAYS_AND_FLOWERS_V_C_MOHAN
#define FIREWORKS_RAYS_AND_FLOWERS_V_C_MOHAN
/*

Author V.C.Mohan.
Date 7 April 2021
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
template <typename finc>
void drawRay(finc* dp, int dpitch, finc color, int sx, int sy, int x, int y, int wd, int ht);

template <typename finc>
void drawFlower(finc* dp, int dpitch, finc color,
	int sx, int sy, int x, int y, int wd, int ht);

//------------------------------------------------------------------
template <typename finc>
void drawRay(finc* dp, int dpitch, finc color,
	int sx, int sy, int x, int y, int wd, int ht)
{
	finc* dpp = dp + sy * dpitch + sx;

	int xabs = x == 0 ? 1 : abs(x) / x;
	int yabs = y == 0 ? 1 : abs(y) / y;
	if (abs(x) > abs(y))
	{
		float ratio = (float)y / abs(x);
		for (int w = 0; w < abs(x); w++)
		{
			int h = (int)(w * ratio);
			int ww = w * xabs;
			// to give some thickness to the ray
			if (sx + ww > 2 && sx + ww < wd - 2 && sy + h > 2 && sy + h < ht - 2)
			{
				*(dpp + h * dpitch + ww) = color;
				*(dpp + h * dpitch + dpitch + ww) = color;
				*(dpp + h * dpitch + ww + 2) = color;
				*(dpp + h * dpitch + dpitch + ww + 2) = color;
			}
		}
	}

	else
	{
		float ratio = (float)x / abs(y);
		for (int h = 0; h < abs(y); h++)
		{
			int w = (int)(h * ratio);
			int hh = h * yabs;
			// to give some thickness to the ray
			if (sx + w > 0 && sx + w < wd && sy + hh > 0 && sy + hh < ht)
			{
				*(dpp + hh * dpitch + w) = color;
				*(dpp + hh * dpitch + dpitch + w) = color;
				*(dpp + hh * dpitch + w + 1) = color;
				*(dpp + hh * dpitch + dpitch + w + 1) = color;
			}
		}
	}

}
template <typename finc>
void drawFlower(finc* dp, int dpitch, finc color,
	int sx, int sy, int x, int y, int wd, int ht)
{
	for (int xx = -x; xx <= x; xx += x)
	{
		for (int yy = -y; yy <= y; yy += y)
		{
			drawRay(dp, dpitch, color, sx, sy, xx, yy, wd, ht);
		}
	}

}


#endif
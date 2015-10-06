////////////////////////////////////////////////////////////////
//
//      Highlight reconstruction
//
//      copyright (c) 2008-2011  Emil Martinec <ejmartin@uchicago.edu>
//
//
// code dated: June 16, 2011
//
//  hilite_recon.cc is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////

#include <cstddef>
#include <cmath>
#include "array2D.h"
#include "rawimagesource.h"
#include "rt_math.h"
#include "opthelper.h"
#include "StopWatch.h"

#define FOREACHCOLOR for (int c=0; c < ColorCount; c++)


namespace rtengine
{


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
SSEFUNCTION void RawImageSource::boxblur2(float** src, float** dst, float** temp, int H, int W, int box )
{
    //box blur image channel; box size = 2*box+1
    //horizontal blur
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int row = 0; row < H; row++) {
        int len = box + 1;
        temp[row][0] = src[row][0] / len;

        for (int j = 1; j <= box; j++) {
            temp[row][0] += src[row][j] / len;
        }

        for (int col = 1; col <= box; col++) {
            temp[row][col] = (temp[row][col - 1] * len + src[row][col + box]) / (len + 1);
            len ++;
        }

        for (int col = box + 1; col < W - box; col++) {
            temp[row][col] = temp[row][col - 1] + (src[row][col + box] - src[row][col - box - 1]) / len;
        }

        for (int col = W - box; col < W; col++) {
            temp[row][col] = (temp[row][col - 1] * len - src[row][col - box - 1]) / (len - 1);
            len --;
        }
    }

#ifdef __SSE2__
    //vertical blur
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        float len = box + 1;
        vfloat lenv = F2V( len );
        vfloat lenp1v = F2V( len + 1.0f );
        vfloat onev = F2V( 1.0f );
        vfloat tempv, temp2v;
#ifdef _OPENMP
        #pragma omp for nowait
#endif

        for (int col = 0; col < W - 7; col += 8) {
            tempv = LVFU(temp[0][col]) / lenv;
            temp2v = LVFU(temp[0][col + 4]) / lenv;

            for (int i = 1; i <= box; i++) {
                tempv = tempv + LVFU(temp[i][col]) / lenv;
                temp2v = temp2v + LVFU(temp[i][col + 4]) / lenv;
            }

            _mm_storeu_ps( &dst[0][col], tempv);
            _mm_storeu_ps( &dst[0][col + 4], temp2v);

            for (int row = 1; row <= box; row++) {
                tempv = (tempv * lenv + LVFU(temp[(row + box)][col])) / lenp1v;
                temp2v = (temp2v * lenv + LVFU(temp[(row + box)][col + 4])) / lenp1v;
                _mm_storeu_ps( &dst[row][col], tempv);
                _mm_storeu_ps( &dst[row][col + 4], temp2v);
                lenv = lenp1v;
                lenp1v = lenp1v + onev;
            }

            for (int row = box + 1; row < H - box; row++) {
                tempv = tempv + (LVFU(temp[(row + box)][col]) - LVFU(temp[(row - box - 1)][col])) / lenv;
                temp2v = temp2v + (LVFU(temp[(row + box)][col + 4]) - LVFU(temp[(row - box - 1)][col + 4])) / lenv;
                _mm_storeu_ps( &dst[row][col], tempv);
                _mm_storeu_ps( &dst[row][col + 4], temp2v);
            }

            for (int row = H - box; row < H; row++) {
                lenp1v = lenv;
                lenv = lenv - onev;
                tempv = (tempv * lenp1v - LVFU(temp[(row - box - 1)][col])) / lenv;
                temp2v = (temp2v * lenp1v - LVFU(temp[(row - box - 1)][col + 4])) / lenv;
                _mm_storeu_ps( &dst[row][col], tempv );
                _mm_storeu_ps( &dst[row][col + 4], temp2v );
            }
        }

        #pragma omp single
        {
            for (int col = W - (W % 8); col < W - 3; col += 4) {
                tempv = LVFU(temp[0][col]) / lenv;

                for (int i = 1; i <= box; i++) {
                    tempv = tempv + LVFU(temp[i][col]) / lenv;
                }

                _mm_storeu_ps( &dst[0][col], tempv);

                for (int row = 1; row <= box; row++) {
                    tempv = (tempv * lenv + LVFU(temp[(row + box)][col])) / lenp1v;
                    _mm_storeu_ps( &dst[row][col], tempv);
                    lenv = lenp1v;
                    lenp1v = lenp1v + onev;
                }

                for (int row = box + 1; row < H - box; row++) {
                    tempv = tempv + (LVFU(temp[(row + box)][col]) - LVFU(temp[(row - box - 1)][col])) / lenv;
                    _mm_storeu_ps( &dst[row][col], tempv);
                }

                for (int row = H - box; row < H; row++) {
                    lenp1v = lenv;
                    lenv = lenv - onev;
                    tempv = (tempv * lenp1v - LVFU(temp[(row - box - 1)][col])) / lenv;
                    _mm_storeu_ps( &dst[row][col], tempv );
                }
            }
        }
    }

    for (int col = W - (W % 4); col < W; col++) {
        int len = box + 1;
        dst[0][col] = temp[0][col] / len;

        for (int i = 1; i <= box; i++) {
            dst[0][col] += temp[i][col] / len;
        }

        for (int row = 1; row <= box; row++) {
            dst[row][col] = (dst[(row - 1)][col] * len + temp[(row + box)][col]) / (len + 1);
            len ++;
        }

        for (int row = box + 1; row < H - box; row++) {
            dst[row][col] = dst[(row - 1)][col] + (temp[(row + box)][col] - temp[(row - box - 1)][col]) / len;
        }

        for (int row = H - box; row < H; row++) {
            dst[row][col] = (dst[(row - 1)][col] * len - temp[(row - box - 1)][col]) / (len - 1);
            len --;
        }
    }

#else
    //vertical blur
#ifdef _OPENMP
    #pragma omp parallel for
#endif

    for (int col = 0; col < W; col++) {
        int len = box + 1;
        dst[0][col] = temp[0][col] / len;

        for (int i = 1; i <= box; i++) {
            dst[0][col] += temp[i][col] / len;
        }

        for (int row = 1; row <= box; row++) {
            dst[row][col] = (dst[(row - 1)][col] * len + temp[(row + box)][col]) / (len + 1);
            len ++;
        }

        for (int row = box + 1; row < H - box; row++) {
            dst[row][col] = dst[(row - 1)][col] + (temp[(row + box)][col] - temp[(row - box - 1)][col]) / len;
        }

        for (int row = H - box; row < H; row++) {
            dst[row][col] = (dst[(row - 1)][col] * len - temp[(row - box - 1)][col]) / (len - 1);
            len --;
        }
    }

#endif

}

void RawImageSource::boxblur_resamp(float **src, float **dst, float ** temp, int H, int W, int box, int samp )
{

#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        float tempval;
#ifdef _OPENMP
        #pragma omp for
#endif

        //box blur image channel; box size = 2*box+1
        //horizontal blur
        for (int row = 0; row < H; row++)
        {
            int len = box + 1;
            tempval = src[row][0] / len;

            for (int j = 1; j <= box; j++) {
                tempval += src[row][j] / len;
            }

            temp[row][0] = tempval;

            for (int col = 1; col <= box; col++) {
                tempval = (tempval * len + src[row][col + box]) / (len + 1);

                if(col % samp == 0) {
                    temp[row][col / samp] = tempval;
                }

                len ++;
            }

            float oneByLen = 1.f / (float)len;

            for (int col = box + 1; col < W - box; col++) {
                tempval = tempval + (src[row][col + box] - src[row][col - box - 1]) * oneByLen;

                if(col % samp == 0) {
                    temp[row][col / samp] = tempval;
                }
            }

            for (int col = W - box; col < W; col++) {
                tempval = (tempval * len - src[row][col - box - 1]) / (len - 1);

                if(col % samp == 0) {
                    temp[row][col / samp] = tempval;
                }

                len --;
            }
        }
    }

    static const int numCols = 8;   // process numCols columns at once for better L1 CPU cache usage
#ifdef _OPENMP
    #pragma omp parallel
#endif
    {
        float tempvalN[numCols] ALIGNED16;
#ifdef _OPENMP
        #pragma omp for nowait
#endif

        //vertical blur
        for (int col = 0; col < (W / samp) - (numCols - 1); col += numCols) {
            int len = box + 1;

            for(int n = 0; n < numCols; n++) {
                tempvalN[n] = temp[0][col + n] / len;
            }

            for (int i = 1; i <= box; i++) {
                for(int n = 0; n < numCols; n++) {
                    tempvalN[n] += temp[i][col + n] / len;
                }
            }

            for(int n = 0; n < numCols; n++) {
                dst[0][col + n] = tempvalN[n];
            }

            for (int row = 1; row <= box; row++) {
                for(int n = 0; n < numCols; n++) {
                    tempvalN[n] = (tempvalN[n] * len + temp[(row + box)][col + n]) / (len + 1);
                }

                if(row % samp == 0) {
                    for(int n = 0; n < numCols; n++) {
                        dst[row / samp][col + n] = tempvalN[n];
                    }
                }

                len ++;
            }

            for (int row = box + 1; row < H - box; row++) {
                for(int n = 0; n < numCols; n++) {
                    tempvalN[n] = tempvalN[n] + (temp[(row + box)][col + n] - temp[(row - box - 1)][col + n]) / len;
                }

                if(row % samp == 0) {
                    for(int n = 0; n < numCols; n++) {
                        dst[row / samp][col + n] = tempvalN[n];
                    }
                }
            }

            for (int row = H - box; row < H; row++) {
                for(int n = 0; n < numCols; n++) {
                    tempvalN[n] = (tempvalN[n] * len - temp[(row - box - 1)][col + n]) / (len - 1);
                }

                if(row % samp == 0) {
                    for(int n = 0; n < numCols; n++) {
                        dst[row / samp][col + n] = tempvalN[n];
                    }
                }

                len --;
            }
        }

        // process remaining columns
        #pragma omp single
        {
            float tempval;

            //vertical blur
            for (int col = (W / samp) - ((W / samp) % numCols); col < W / samp; col++) {
                int len = box + 1;
                tempval = temp[0][col] / len;

                for (int i = 1; i <= box; i++) {
                    tempval += temp[i][col] / len;
                }

                dst[0][col] = tempval;

                for (int row = 1; row <= box; row++) {
                    tempval = (tempval * len + temp[(row + box)][col]) / (len + 1);

                    if(row % samp == 0) {
                        dst[row / samp][col] = tempval;
                    }

                    len ++;
                }

                for (int row = box + 1; row < H - box; row++) {
                    tempval = tempval + (temp[(row + box)][col] - temp[(row - box - 1)][col]) / len;

                    if(row % samp == 0) {
                        dst[row / samp][col] = tempval;
                    }
                }

                for (int row = H - box; row < H; row++) {
                    tempval = (tempval * len - temp[(row - box - 1)][col]) / (len - 1);

                    if(row % samp == 0) {
                        dst[row / samp][col] = tempval;
                    }

                    len --;
                }
            }
        }
    }


}

void RawImageSource :: HLRecovery_inpaint (float** red, float** green, float** blue)
{
    StopWatch Stop1("HLRecovery_inpaint");
    double progress = 0.0;

    if (plistener) {
        plistener->setProgressStr ("HL reconstruction...");
        plistener->setProgress (progress);
    }

    int height = H;
    int width = W;

    static const int range = 2;
    static const int pitch = 4;

    static const int numdirs = 4;

    static const float threshpct = 0.25;
    static const float fixthreshpct = 0.7;
    static const float maxpct = 0.95;

    //%%%%%%%%%%%%%%%%%%%%
    //for blend algorithm:
    static const float blendthresh = 1.0;
    static const int ColorCount = 3;
    // Transform matrixes rgb>lab and back
    static const float trans[2][ColorCount][ColorCount] = {
        { { 1, 1, 1 }, { 1.7320508, -1.7320508, 0 }, { -1, -1, 2 } },
        { { 1, 1, 1 }, { 1, -1, 1 }, { 1, 1, -1 } }
    };
    static const float itrans[2][ColorCount][ColorCount] = {
        { { 1, 0.8660254, -0.5 }, { 1, -0.8660254, -0.5 }, { 1, 0, 1 } },
        { { 1, 1, 1 }, { 1, -1, 1 }, { 1, 1, -1 } }
    };

    float max_f[3], thresh[3];

    for (int c = 0; c < 3; c++) {
        thresh[c] = chmax[c] * threshpct;
        max_f[c] = chmax[c] * maxpct; //min(chmax[0],chmax[1],chmax[2])*maxpct;
    }

    float whitept = max(max_f[0], max_f[1], max_f[2]);
    float clippt  = min(max_f[0], max_f[1], max_f[2]);
    float medpt   = max_f[0] + max_f[1] + max_f[2] - whitept - clippt;
    float blendpt = blendthresh * clippt;

    multi_array2D<float, 3> channelblur(width, height, 0, 48);
    array2D<float> temp(width, height); // allocate temporary buffer
    // blur RGB channels
    boxblur2(red  , channelblur[0], temp, height, width, 4);

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    boxblur2(green, channelblur[1], temp, height, width, 4);

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    boxblur2(blue , channelblur[2], temp, height, width, 4);

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    // reduce channel blur to one array
    #pragma omp parallel for

    for(int i = 0; i < height; i++)
        for(int j = 0; j < width; j++) {
            channelblur[0][i][j] = fabsf(channelblur[0][i][j] - red[i][j]) + fabsf(channelblur[1][i][j] - green[i][j]) + fabsf(channelblur[2][i][j] - blue[i][j]);
        }

    for (int c = 1; c < 3; c++) {
        channelblur[c].free();    //free up some memory
    }

    multi_array2D<float, 4> hilite_full(width, height, ARRAY2D_CLEAR_DATA, 32);

    double hipass_sum = 0.f;
    int hipass_norm = 0;

    // set up which pixels are clipped or near clipping
#ifdef _OPENMP
    #pragma omp parallel for reduction(+:hipass_sum,hipass_norm) schedule(dynamic,16)
#endif

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            //if one or more channels is highlight but none are blown, add to highlight accumulator
            if ((red[i][j] > thresh[0] || green[i][j] > thresh[1] || blue[i][j] > thresh[2]) &&
                    (red[i][j] < max_f[0] && green[i][j] < max_f[1] && blue[i][j] < max_f[2])) {

                hipass_sum += channelblur[0][i][j];
                hipass_norm ++;

                hilite_full[0][i][j] = red[i][j];
                hilite_full[1][i][j] = green[i][j];
                hilite_full[2][i][j] = blue[i][j];
                hilite_full[3][i][j] = 1.f;

            }
        }
    }//end of filling highlight array

    float hipass_ave = 2.f * hipass_sum / (hipass_norm + 0.01);

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    array2D<float> hilite_full4(width, height);
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    //blur highlight data
    boxblur2(hilite_full[3], hilite_full4, temp, height, width, 1);

    temp.free(); // free temporary buffer

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16)
#endif

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (channelblur[0][i][j] > hipass_ave) {
                //too much variation
                hilite_full[0][i][j] = hilite_full[1][i][j] = hilite_full[2][i][j] = hilite_full[3][i][j] = 0.f;
                continue;
            }

            if (hilite_full4[i][j] > 0.00001 && hilite_full4[i][j] < 0.95) {
                //too near an edge, could risk using CA affected pixels, therefore omit
                hilite_full[0][i][j] = hilite_full[1][i][j] = hilite_full[2][i][j] = hilite_full[3][i][j] = 0.f;
            }
        }
    }

    channelblur[0].free();    //free up some memory
    hilite_full4.free();    //free up some memory

    int hfh = (height - (height % pitch)) / pitch;
    int hfw = (width - (width % pitch)) / pitch;

    multi_array2D<float, 4> hilite(hfw + 1, hfh + 1, ARRAY2D_CLEAR_DATA, 48);

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // blur and resample highlight data; range=size of blur, pitch=sample spacing

    array2D<float> temp2((width / pitch) + ((width % pitch) == 0 ? 0 : 1), height);

    for (int m = 0; m < 4; m++) {
        boxblur_resamp(hilite_full[m], hilite[m], temp2, height, width, range, pitch);

        if(plistener) {
            progress += 0.05;
            plistener->setProgress(progress);
        }
    }

    temp2.free();

    for (int c = 0; c < 4; c++) {
        hilite_full[c].free();    //free up some memory
    }

    multi_array2D<float, 8> hilite_dir(hfw, hfh, ARRAY2D_CLEAR_DATA, 64);
    multi_array2D<float, 4> hilite_dir0(hfh, hfw, ARRAY2D_CLEAR_DATA, 64);
    multi_array2D<float, 4> hilite_dir4(hfh, hfw, ARRAY2D_CLEAR_DATA, 64);
    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    //blur highlights
    //for (int m=0; m<4; m++)
    //  boxblur2(hilite[m],hilite[m],hfh,hfw,4);

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }


    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%


    //fill gaps in highlight map by directional extension
    //raster scan from four corners
    for (int j = 1; j < hfw - 1; j++) {
        for (int i = 2; i < hfh - 2; i++) {
            //from left
            if (hilite[3][i][j] > 0.01) {
                hilite_dir0[3][j][i] = 1.f;
            } else {
                hilite_dir0[3][j][i] = (hilite_dir0[0 + 3][j - 1][i - 2] + hilite_dir0[0 + 3][j - 1][i - 1] + hilite_dir0[0 + 3][j - 1][i] + hilite_dir0[0 + 3][j - 1][i + 1] + hilite_dir0[0 + 3][j - 1][i + 2]) == 0.f ? 0.f : 0.1f;
            }
        }

        if(hilite[3][2][j] <= 0.01) {
            hilite_dir[0 + 3][0][j]  = hilite_dir0[3][j][2];
        }

        if(hilite[3][3][j] <= 0.01) {
            hilite_dir[0 + 3][1][j]  = hilite_dir0[3][j][3];
        }

        if(hilite[3][hfh - 3][j] <= 0.01) {
            hilite_dir[4 + 3][hfh - 1][j] = hilite_dir0[3][j][hfh - 3];
        }

        if(hilite[3][hfh - 4][j] <= 0.01) {
            hilite_dir[4 + 3][hfh - 2][j] = hilite_dir0[3][j][hfh - 4];
        }
    }

    for (int i = 2; i < hfh - 2; i++) {
        if(hilite[3][i][hfw - 2] <= 0.01) {
            hilite_dir4[3][hfw - 1][i] = hilite_dir0[3][hfw - 2][i];
        }
    }



    #pragma omp parallel for

    for (int c = 0; c < 3; c++) {
        for (int j = 1; j < hfw - 1; j++) {
            for (int i = 2; i < hfh - 2; i++) {
                //from left
                if (hilite[3][i][j] > 0.01f) {
                    hilite_dir0[c][j][i] = hilite[c][i][j] / hilite[3][i][j];
                } else {
                    hilite_dir0[c][j][i] = 0.1f * ((hilite_dir0[0 + c][j - 1][i - 2] + hilite_dir0[0 + c][j - 1][i - 1] + hilite_dir0[0 + c][j - 1][i] + hilite_dir0[0 + c][j - 1][i + 1] + hilite_dir0[0 + c][j - 1][i + 2]) /
                                                   (hilite_dir0[0 + 3][j - 1][i - 2] + hilite_dir0[0 + 3][j - 1][i - 1] + hilite_dir0[0 + 3][j - 1][i] + hilite_dir0[0 + 3][j - 1][i + 1] + hilite_dir0[0 + 3][j - 1][i + 2] + 0.00001f));
                }
            }

            if(hilite[3][2][j] <= 0.01f) {
                hilite_dir[0 + c][0][j]  = hilite_dir0[c][j][2];
            }

            if(hilite[3][3][j] <= 0.01f) {
                hilite_dir[0 + c][1][j]  = hilite_dir0[c][j][3];
            }

            if(hilite[3][hfh - 3][j] <= 0.01f) {
                hilite_dir[4 + c][hfh - 1][j] = hilite_dir0[c][j][hfh - 3];
            }

            if(hilite[3][hfh - 4][j] <= 0.01f) {
                hilite_dir[4 + c][hfh - 2][j] = hilite_dir0[c][j][hfh - 4];
            }
        }

        for (int i = 2; i < hfh - 2; i++) {
            if(hilite[3][i][hfw - 2] <= 0.01f) {
                hilite_dir4[c][hfw - 1][i] = hilite_dir0[c][hfw - 2][i];
            }
        }
    }

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    for (int j = hfw - 2; j > 0; j--) {
        for (int i = 2; i < hfh - 2; i++) {
            //from right
            if (hilite[3][i][j] > 0.01) {
                hilite_dir4[3][j][i] = 1.f;
            } else {
                hilite_dir4[3][j][i] = (hilite_dir4[3][(j + 1)][(i - 2)] + hilite_dir4[3][(j + 1)][(i - 1)] + hilite_dir4[3][(j + 1)][(i)] + hilite_dir4[3][(j + 1)][(i + 1)] + hilite_dir4[3][(j + 1)][(i + 2)]) == 0.f ? 0.f : 0.1f;
            }
        }

        if(hilite[3][2][j] <= 0.01) {
            hilite_dir[0 + 3][0][j] += hilite_dir4[3][j][2];
        }

        if(hilite[3][hfh - 3][j] <= 0.01) {
            hilite_dir[4 + 3][hfh - 1][j] += hilite_dir4[3][j][hfh - 3];
        }
    }

    for (int i = 2; i < hfh - 2; i++) {
        if(hilite[3][i][0] <= 0.01) {
            hilite_dir[0 + 3][i - 2][0] += hilite_dir4[3][0][i];
            hilite_dir[4 + 3][i + 2][0] += hilite_dir4[3][0][i];
        }

        if(hilite[3][i][1] <= 0.01) {
            hilite_dir[0 + 3][i - 2][1] += hilite_dir4[3][1][i];
            hilite_dir[4 + 3][i + 2][1] += hilite_dir4[3][1][i];
        }

        if(hilite[3][i][hfw - 2] <= 0.01) {
            hilite_dir[0 + 3][i - 2][hfw - 2] += hilite_dir4[3][hfw - 2][i];
            hilite_dir[4 + 3][i + 2][hfw - 2] += hilite_dir4[3][hfw - 2][i];
        }
    }

    #pragma omp parallel for

    for (int c = 0; c < 3; c++) {
        for (int j = hfw - 2; j > 0; j--) {
            for (int i = 2; i < hfh - 2; i++) {
                //from right
                if (hilite[3][i][j] > 0.01) {
                    hilite_dir4[c][j][i] = hilite[c][i][j] / hilite[3][i][j];
                } else {
                    hilite_dir4[c][j][i] = 0.1 * ((hilite_dir4[c][(j + 1)][(i - 2)] + hilite_dir4[c][(j + 1)][(i - 1)] + hilite_dir4[c][(j + 1)][(i)] + hilite_dir4[c][(j + 1)][(i + 1)] + hilite_dir4[c][(j + 1)][(i + 2)]) /
                                                  (hilite_dir4[3][(j + 1)][(i - 2)] + hilite_dir4[3][(j + 1)][(i - 1)] + hilite_dir4[3][(j + 1)][(i)] + hilite_dir4[3][(j + 1)][(i + 1)] + hilite_dir4[3][(j + 1)][(i + 2)] + 0.00001));
                }
            }

            if(hilite[3][2][j] <= 0.01) {
                hilite_dir[0 + c][0][j] += hilite_dir4[c][j][2];
            }

            if(hilite[3][hfh - 3][j] <= 0.01) {
                hilite_dir[4 + c][hfh - 1][j] += hilite_dir4[c][j][hfh - 3];
            }
        }

        for (int i = 2; i < hfh - 2; i++) {
            if(hilite[3][i][0] <= 0.01) {
                hilite_dir[0 + c][i - 2][0] += hilite_dir4[c][0][i];
                hilite_dir[4 + c][i + 2][0] += hilite_dir4[c][0][i];
            }

            if(hilite[3][i][1] <= 0.01) {
                hilite_dir[0 + c][i - 2][1] += hilite_dir4[c][1][i];
                hilite_dir[4 + c][i + 2][1] += hilite_dir4[c][1][i];
            }

            if(hilite[3][i][hfw - 2] <= 0.01) {
                hilite_dir[0 + c][i - 2][hfw - 2] += hilite_dir4[c][hfw - 2][i];
                hilite_dir[4 + c][i + 2][hfw - 2] += hilite_dir4[c][hfw - 2][i];
            }
        }


    }

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }


    for (int i = 1; i < hfh - 1; i++)
        for (int j = 2; j < hfw - 2; j++) {
            //from top
            if (hilite[3][i][j] > 0.01) {
                hilite_dir[0 + 3][i][j] = 1.f;
            } else {
                hilite_dir[0 + 3][i][j] = (hilite_dir[0 + 3][i - 1][j - 2] + hilite_dir[0 + 3][i - 1][j - 1] + hilite_dir[0 + 3][i - 1][j] + hilite_dir[0 + 3][i - 1][j + 1] + hilite_dir[0 + 3][i - 1][j + 2]) == 0.f ? 0.f : 0.1f;
            }
        }

    for (int j = 2; j < hfw - 2; j++) {
        if(hilite[3][hfh - 2][j] <= 0.01) {
            hilite_dir[4 + 3][hfh - 1][j] += hilite_dir[0 + 3][hfh - 2][j];
        }
    }

    #pragma omp parallel for

    for (int c = 0; c < 3; c++) {
        for (int i = 1; i < hfh - 1; i++) {
            for (int j = 2; j < hfw - 2; j++) {
                //from top
                if (hilite[3][i][j] > 0.01) {
                    hilite_dir[0 + c][i][j] = hilite[c][i][j] / hilite[3][i][j];
                } else {
                    hilite_dir[0 + c][i][j] = 0.1 * ((hilite_dir[0 + c][i - 1][j - 2] + hilite_dir[0 + c][i - 1][j - 1] + hilite_dir[0 + c][i - 1][j] + hilite_dir[0 + c][i - 1][j + 1] + hilite_dir[0 + c][i - 1][j + 2]) /
                                                     (hilite_dir[0 + 3][i - 1][j - 2] + hilite_dir[0 + 3][i - 1][j - 1] + hilite_dir[0 + 3][i - 1][j] + hilite_dir[0 + 3][i - 1][j + 1] + hilite_dir[0 + 3][i - 1][j + 2] + 0.00001));
                }
            }
        }

        for (int j = 2; j < hfw - 2; j++) {
            if(hilite[3][hfh - 2][j] <= 0.01) {
                hilite_dir[4 + c][hfh - 1][j] += hilite_dir[0 + c][hfh - 2][j];
            }
        }

    }

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    for (int i = hfh - 2; i > 0; i--)
        for (int j = 2; j < hfw - 2; j++) {
            //from bottom
            if (hilite[3][i][j] > 0.01) {
                hilite_dir[4 + 3][i][j] = 1.f;
            } else {
                hilite_dir[4 + 3][i][j] = (hilite_dir[4 + 3][(i + 1)][(j - 2)] + hilite_dir[4 + 3][(i + 1)][(j - 1)] + hilite_dir[4 + 3][(i + 1)][(j)] + hilite_dir[4 + 3][(i + 1)][(j + 1)] + hilite_dir[4 + 3][(i + 1)][(j + 2)]) == 0.f ? 0.f : 0.1f;
            }

        }

    #pragma omp parallel for

    for (int c = 0; c < 4; c++) {
        for (int i = hfh - 2; i > 0; i--) {
            for (int j = 2; j < hfw - 2; j++) {
                //from bottom
                if (hilite[3][i][j] > 0.01) {
                    hilite_dir[4 + c][i][j] = hilite[c][i][j] / hilite[3][i][j];
                } else {
                    hilite_dir[4 + c][i][j] = 0.1 * ((hilite_dir[4 + c][(i + 1)][(j - 2)] + hilite_dir[4 + c][(i + 1)][(j - 1)] + hilite_dir[4 + c][(i + 1)][(j)] + hilite_dir[4 + c][(i + 1)][(j + 1)] + hilite_dir[4 + c][(i + 1)][(j + 2)]) /
                                                     (hilite_dir[4 + 3][(i + 1)][(j - 2)] + hilite_dir[4 + 3][(i + 1)][(j - 1)] + hilite_dir[4 + 3][(i + 1)][(j)] + hilite_dir[4 + 3][(i + 1)][(j + 1)] + hilite_dir[4 + 3][(i + 1)][(j + 2)] + 0.00001));
                }

            }
        }
    }

    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    //fill in edges
    for (int dir = 0; dir < 2; dir++) {
        for (int i = 1; i < hfh - 1; i++)
            for (int c = 0; c < 4; c++) {
                hilite_dir[dir * 4 + c][i][0] = hilite_dir[dir * 4 + c][i][1];
                hilite_dir[dir * 4 + c][i][hfw - 1] = hilite_dir[dir * 4 + c][i][hfw - 2];
            }

        for (int j = 1; j < hfw - 1; j++)
            for (int c = 0; c < 4; c++) {
                hilite_dir[dir * 4 + c][0][j] = hilite_dir[dir * 4 + c][1][j];
                hilite_dir[dir * 4 + c][hfh - 1][j] = hilite_dir[dir * 4 + c][hfh - 2][j];
            }

        for (int c = 0; c < 4; c++) {
            hilite_dir[dir * 4 + c][0][0] = hilite_dir[dir * 4 + c][1][0] = hilite_dir[dir * 4 + c][0][1] = hilite_dir[dir * 4 + c][1][1] = hilite_dir[dir * 4 + c][2][2];
            hilite_dir[dir * 4 + c][0][hfw - 1] = hilite_dir[dir * 4 + c][1][hfw - 1] = hilite_dir[dir * 4 + c][0][hfw - 2] = hilite_dir[dir * 4 + c][1][hfw - 2] = hilite_dir[dir * 4 + c][2][hfw - 3];
            hilite_dir[dir * 4 + c][hfh - 1][0] = hilite_dir[dir * 4 + c][hfh - 2][0] = hilite_dir[dir * 4 + c][hfh - 1][1] = hilite_dir[dir * 4 + c][hfh - 2][1] = hilite_dir[dir * 4 + c][hfh - 3][2];
            hilite_dir[dir * 4 + c][hfh - 1][hfw - 1] = hilite_dir[dir * 4 + c][hfh - 2][hfw - 1] = hilite_dir[dir * 4 + c][hfh - 1][hfw - 2] = hilite_dir[dir * 4 + c][hfh - 2][hfw - 2] = hilite_dir[dir * 4 + c][hfh - 3][hfw - 3];
        }
    }

    for (int i = 1; i < hfh - 1; i++)
        for (int c = 0; c < 4; c++) {
            hilite_dir0[c][0][i] = hilite_dir0[c][1][i];
            hilite_dir0[c][hfw - 1][i] = hilite_dir0[c][hfw - 2][i];
        }

    for (int j = 1; j < hfw - 1; j++)
        for (int c = 0; c < 4; c++) {
            hilite_dir0[c][j][0] = hilite_dir0[c][j][1];
            hilite_dir0[c][j][hfh - 1] = hilite_dir0[c][j][hfh - 2];
        }

    for (int c = 0; c < 4; c++) {
        hilite_dir0[c][0][0] = hilite_dir0[c][0][1] = hilite_dir0[c][1][0] = hilite_dir0[c][1][1] = hilite_dir0[c][2][2];
        hilite_dir0[c][hfw - 1][0] = hilite_dir0[c][hfw - 1][1] = hilite_dir0[c][hfw - 2][0] = hilite_dir0[c][hfw - 2][1] = hilite_dir0[c][hfw - 3][2];
        hilite_dir0[c][0][hfh - 1] = hilite_dir0[c][0][hfh - 2] = hilite_dir0[c][1][hfh - 1] = hilite_dir0[c][1][hfh - 2] = hilite_dir0[c][2][hfh - 3];
        hilite_dir0[c][hfw - 1][hfh - 1] = hilite_dir0[c][hfw - 1][hfh - 2] = hilite_dir0[c][hfw - 2][hfh - 1] = hilite_dir0[c][hfw - 2][hfh - 2] = hilite_dir0[c][hfw - 3][hfh - 3];
    }

    for (int i = 1; i < hfh - 1; i++)
        for (int c = 0; c < 4; c++) {
            hilite_dir4[c][0][i] = hilite_dir4[c][1][i];
            hilite_dir4[c][hfw - 1][i] = hilite_dir4[c][hfw - 2][i];
        }

    for (int j = 1; j < hfw - 1; j++)
        for (int c = 0; c < 4; c++) {
            hilite_dir4[c][j][0] = hilite_dir4[c][j][1];
            hilite_dir4[c][j][hfh - 1] = hilite_dir4[c][j][hfh - 2];
        }

    for (int c = 0; c < 4; c++) {
        hilite_dir4[c][0][0] = hilite_dir4[c][0][1] = hilite_dir4[c][1][0] = hilite_dir4[c][1][1] = hilite_dir4[c][2][2];
        hilite_dir4[c][hfw - 1][0] = hilite_dir4[c][hfw - 1][1] = hilite_dir4[c][hfw - 2][0] = hilite_dir4[c][hfw - 2][1] = hilite_dir4[c][hfw - 3][2];
        hilite_dir4[c][0][hfh - 1] = hilite_dir4[c][0][hfh - 2] = hilite_dir4[c][1][hfh - 1] = hilite_dir4[c][1][hfh - 2] = hilite_dir4[c][2][hfh - 3];
        hilite_dir4[c][hfw - 1][hfh - 1] = hilite_dir4[c][hfw - 1][hfh - 2] = hilite_dir4[c][hfw - 2][hfh - 1] = hilite_dir4[c][hfw - 2][hfh - 2] = hilite_dir4[c][hfw - 3][hfh - 3];
    }



    if(plistener) {
        progress += 0.05;
        plistener->setProgress(progress);
    }

    //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // now reconstruct clipped channels using color ratios

    for(int c = 0; c < 4; c++) {
        hilite[c].free();
    }

    LUTf invfn(0x10000);

    for (int i = 0; i < 0x10000; i++) {
        invfn[i] = 1.0 / (1.0 + i);
    }

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic,16)
#endif

    for (int i = 0; i < height; i++) {
        int i1 = min((i - (i % pitch)) / pitch, hfh - 1);

        for (int j = 0; j < width; j++) {
            int j1 = min((j - (j % pitch)) / pitch, hfw - 1);

            float pixel[3] = {red[i][j], green[i][j], blue[i][j]};

            if (pixel[0] < max_f[0] && pixel[1] < max_f[1] && pixel[2] < max_f[2]) {
                continue;    //pixel not clipped
            }

            //%%%%%%%%%%%%%%%%%%%%%%%
            //estimate recovered values using modified HLRecovery_blend algorithm

            float rgb[ColorCount], rgb_blend[ColorCount] = {}, cam[2][ColorCount], lab[2][ColorCount], sum[2], chratio;

            // Copy input pixel to rgb so it's easier to access in loops
            rgb[0] = pixel[0];
            rgb[1] = pixel[1];
            rgb[2] = pixel[2];

            // Initialize cam with raw input [0] and potentially clipped input [1]
            FOREACHCOLOR {
                cam[0][c] = rgb[c];
                cam[1][c] = min(cam[0][c], clippt);
            }

            // Calculate the lightness correction ratio (chratio)
            for (int i2 = 0; i2 < 2; i2++) {
                FOREACHCOLOR {
                    lab[i2][c] = 0;

                    for (int j = 0; j < ColorCount; j++)
                    {
                        lab[i2][c] += trans[ColorCount - 3][c][j] * cam[i2][j];
                    }
                }

                sum[i2] = 0;

                for (int c = 1; c < ColorCount; c++) {
                    sum[i2] += SQR(lab[i2][c]);
                }
            }

            if(sum[0] == 0.f) {     // avoid division by zero
                sum[0] = 0.0001f;
            }

            chratio = sqrt(sum[1] / sum[0]);


            // Apply ratio to lightness in lab space
            for (int c = 1; c < ColorCount; c++) {
                lab[0][c] *= chratio;
            }

            // Transform back from lab to RGB
            FOREACHCOLOR {
                cam[0][c] = 0;

                for (int j = 0; j < ColorCount; j++)
                {
                    cam[0][c] += itrans[ColorCount - 3][c][j] * lab[0][j];
                }
            }
            FOREACHCOLOR rgb[c] = cam[0][c] / ColorCount;

            // Copy converted pixel back
            float rfrac = min(1.0f, max(1.0f, max_f[0] / medpt) * (pixel[0] - blendpt) / (hlmax[0] - blendpt));
            float gfrac = min(1.0f, max(1.0f, max_f[1] / medpt) * (pixel[1] - blendpt) / (hlmax[1] - blendpt));
            float bfrac = min(1.0f, max(1.0f, max_f[2] / medpt) * (pixel[2] - blendpt) / (hlmax[2] - blendpt));

            if (pixel[0] > blendpt) {
                rgb_blend[0] = rfrac * rgb[0] + (1 - rfrac) * pixel[0];
            }

            if (pixel[1] > blendpt) {
                rgb_blend[1] = gfrac * rgb[1] + (1 - gfrac) * pixel[1];
            }

            if (pixel[2] > blendpt) {
                rgb_blend[2] = bfrac * rgb[2] + (1 - bfrac) * pixel[2];
            }

            //end of HLRecovery_blend estimation
            //%%%%%%%%%%%%%%%%%%%%%%%

            //float pixref[3]={min(Yclip,hfsize[0][i1][j1]),min(Yclip,hfsize[1][i1][j1]),min(Yclip,hfsize[2][i1][j1])};

            //there are clipped highlights
            //first, determine weighted average of unclipped extensions (weighting is by 'hue' proximity)
            float dirwt, factor;
            float totwt = 0; //0.0003;
            float clipfix[3] = {0, 0, 0}; //={totwt*rgb_blend[0],totwt*rgb_blend[1],totwt*rgb_blend[2]};

            float Yhi = 0.001 + (hilite_dir0[0][j1][i1] + hilite_dir0[1][j1][i1] + hilite_dir0[2][j1][i1]);
            float Y   = 0.001 + (rgb_blend[0] + rgb_blend[1] + rgb_blend[2]);

            if (hilite_dir0[0][j1][i1] + hilite_dir0[1][j1][i1] + hilite_dir0[2][j1][i1] > 0.5) {
                dirwt = invfn[65535 * (SQR(rgb_blend[0] / Y - hilite_dir0[0][j1][i1] / Yhi) +
                                       SQR(rgb_blend[1] / Y - hilite_dir0[1][j1][i1] / Yhi) +
                                       SQR(rgb_blend[2] / Y - hilite_dir0[2][j1][i1] / Yhi))];
                totwt += dirwt;
                clipfix[0] += dirwt * hilite_dir0[0][j1][i1] / (hilite_dir0[3][j1][i1] + 0.00001);
                clipfix[1] += dirwt * hilite_dir0[1][j1][i1] / (hilite_dir0[3][j1][i1] + 0.00001);
                clipfix[2] += dirwt * hilite_dir0[2][j1][i1] / (hilite_dir0[3][j1][i1] + 0.00001);
            }

            for (int dir = 0; dir < 2; dir++) {
                float Yhi = 0.001 + (hilite_dir[dir * 4 + 0][i1][j1] + hilite_dir[dir * 4 + 1][i1][j1] + hilite_dir[dir * 4 + 2][i1][j1]);
                float Y   = 0.001 + (rgb_blend[0] + rgb_blend[1] + rgb_blend[2]);

                if (hilite_dir[dir * 4 + 0][i1][j1] + hilite_dir[dir * 4 + 1][i1][j1] + hilite_dir[dir * 4 + 2][i1][j1] > 0.5) {
                    dirwt = invfn[65535 * (SQR(rgb_blend[0] / Y - hilite_dir[dir * 4 + 0][i1][j1] / Yhi) +
                                           SQR(rgb_blend[1] / Y - hilite_dir[dir * 4 + 1][i1][j1] / Yhi) +
                                           SQR(rgb_blend[2] / Y - hilite_dir[dir * 4 + 2][i1][j1] / Yhi))];
                    totwt += dirwt;
                    clipfix[0] += dirwt * hilite_dir[dir * 4 + 0][i1][j1] / (hilite_dir[dir * 4 + 3][i1][j1] + 0.00001);
                    clipfix[1] += dirwt * hilite_dir[dir * 4 + 1][i1][j1] / (hilite_dir[dir * 4 + 3][i1][j1] + 0.00001);
                    clipfix[2] += dirwt * hilite_dir[dir * 4 + 2][i1][j1] / (hilite_dir[dir * 4 + 3][i1][j1] + 0.00001);
                }
            }

            Yhi = 0.001 + (hilite_dir4[0][j1][i1] + hilite_dir4[1][j1][i1] + hilite_dir4[2][j1][i1]);
            Y   = 0.001 + (rgb_blend[0] + rgb_blend[1] + rgb_blend[2]);

            if (hilite_dir4[0][j1][i1] + hilite_dir4[1][j1][i1] + hilite_dir4[2][j1][i1] > 0.5) {
                dirwt = invfn[65535 * (SQR(rgb_blend[0] / Y - hilite_dir4[0][j1][i1] / Yhi) +
                                       SQR(rgb_blend[1] / Y - hilite_dir4[1][j1][i1] / Yhi) +
                                       SQR(rgb_blend[2] / Y - hilite_dir4[2][j1][i1] / Yhi))];
                totwt += dirwt;
                clipfix[0] += dirwt * hilite_dir4[0][j1][i1] / (hilite_dir4[3][j1][i1] + 0.00001);
                clipfix[1] += dirwt * hilite_dir4[1][j1][i1] / (hilite_dir4[3][j1][i1] + 0.00001);
                clipfix[2] += dirwt * hilite_dir4[2][j1][i1] / (hilite_dir4[3][j1][i1] + 0.00001);
            }

            if(totwt == 0.f) {
                continue;
            }

            clipfix[0] /= totwt;
            clipfix[1] /= totwt;
            clipfix[2] /= totwt;
            //sumwt += totwt;
            //counts ++;

            //now correct clipped channels
            if (pixel[0] > max_f[0] && pixel[1] > max_f[1] && pixel[2] > max_f[2]) {
                //all channels clipped
                float Y = (0.299 * clipfix[0] + 0.587 * clipfix[1] + 0.114 * clipfix[2]);

                factor = whitept / Y;
                red[i][j]   = clipfix[0] * factor;
                green[i][j] = clipfix[1] * factor;
                blue[i][j]  = clipfix[2] * factor;
            } else {//some channels clipped
                const float eps = 0.0001f;
                int notclipped[3] = {pixel[0] < max_f[0] ? 1 : 0, pixel[1] < max_f[1] ? 1 : 0, pixel[2] < max_f[2] ? 1 : 0};

                if (notclipped[0] == 0) { //red clipped
                    red[i][j]  = max(red[i][j], (clipfix[0] * ((notclipped[1] * pixel[1] + notclipped[2] * pixel[2]) /
                                                 (notclipped[1] * clipfix[1] + notclipped[2] * clipfix[2] + eps))));
                }

                if (notclipped[1] == 0) { //green clipped
                    green[i][j] = max(green[i][j], (clipfix[1] * ((notclipped[2] * pixel[2] + notclipped[0] * pixel[0]) /
                                                    (notclipped[2] * clipfix[2] + notclipped[0] * clipfix[0] + eps))));
                }

                if (notclipped[2] == 0) { //blue clipped
                    blue[i][j]  = max(blue[i][j], (clipfix[2] * ((notclipped[0] * pixel[0] + notclipped[1] * pixel[1]) /
                                                   (notclipped[0] * clipfix[0] + notclipped[1] * clipfix[1] + eps))));
                }
            }

            Y = (0.299 * red[i][j] + 0.587 * green[i][j] + 0.114 * blue[i][j]);

            if (Y > whitept) {
                factor = whitept / Y;

                red[i][j]   *= factor;
                green[i][j] *= factor;
                blue[i][j]  *= factor;
            }
        }
    }

    if(plistener) {
        plistener->setProgress(1.00);
    }

}// end of HLReconstruction


}


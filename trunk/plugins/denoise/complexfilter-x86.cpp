/*
* Copyright (C) 2009 Klaus Post
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "complexfilter.h"
#include <math.h>
#include "fftwindow.h"


#if defined (__i386__) || defined (__x86_64__)

#if defined (__x86_64__)
void DeGridComplexFilter::processSharpenOnlySSE3(ComplexBlock* block) {
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
  }
  int size = bw*bh;
  if ((size & 7) == 0) {   // TODO: Benchmark on non-vm platform - slower under VMWARE
    asm volatile
    (
    "movaps 16(%1),%%xmm14\n"          // Load gridfraction into xmm6
    "loop_sharpenonly_sse3_big:\n"
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps 32(%2), %%xmm8\n"           // in r0i0 r1i1
    "movaps 48(%2), %%xmm9\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3
    "movaps 32(%3), %%xmm12\n"           // grid r0i0 r1i1
    "movaps 48(%3), %%xmm13\n"         // grid r2i2 r3i3

    "mulps %%xmm14, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm14, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "mulps %%xmm14, %%xmm12\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm14, %%xmm13\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory
    "movaps %%xmm5, %%xmm3\n"
    "movaps %%xmm12, %%xmm10\n"         // maintain gridcorrection in memory
    "movaps %%xmm13, %%xmm11\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    -
    "subps %%xmm12, %%xmm8\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm13, %%xmm9\n"          // re2 im2 re3 im3    -
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"
    "movaps %%xmm8, %%xmm12\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm9, %%xmm13\n"

    "movaps (%1), %%xmm6\n"         // Move 1e-15 into xmm6
    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "mulps %%xmm12, %%xmm12\n"          //r0i0 r1i1 squared
    "mulps %%xmm13, %%xmm13\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "haddps %%xmm13, %%xmm12\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "addps %%xmm6, %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)
    "addps %%xmm6, %%xmm12\n"            // add 1e-15 (xmm4: psd for all 4 pixels)
    "movaps 48(%1), %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ;
    "movaps 32(%1), %%xmm6\n"         // Move sigmaSquaredSharpenMin into xmm6
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "movaps %%xmm12, %%xmm13\n"       // Copy psd into xmm5
    "movaps %%xmm7, %%xmm15\n"       // Copy sigmaSquaredSharpenMax
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "addps %%xmm7, %%xmm12\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "mulps %%xmm13, %%xmm15\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps %%xmm6, %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free
    "addps %%xmm6, %%xmm13\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free

    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "mulps %%xmm12, %%xmm13\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "movaps (%4), %%xmm6\n"         // load wsharpen[0->4]
    "rcpps %%xmm13, %%xmm13\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "movaps 16(%4), %%xmm4\n"       // Load wsharpen
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "mulps %%xmm13, %%xmm15\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "movaps 64(%1), %%xmm5\n"       // Load "1.0"
    "rsqrtps %%xmm7, %%xmm7\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rsqrtps %%xmm15, %%xmm15\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "rcpps %%xmm15, %%xmm15\n"        // sqrt (..)
    "mulps %%xmm6, %%xmm7\n"        // multiply wsharpen
    "mulps %%xmm4, %%xmm15\n"        // multiply wsharpen
    "addps %%xmm5, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "addps %%xmm5, %%xmm15\n"        // + 1.0 xmm7 = sfact
    "movaps %%xmm7, %%xmm5\n"
    "movaps %%xmm15, %%xmm13\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5
    "unpcklps %%xmm15, %%xmm15\n"     // unpack low to xmm7
    "unpckhps %%xmm13, %%xmm13\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "mulps %%xmm15, %%xmm8\n"        // re+im *= sfact
    "mulps %%xmm13, %%xmm9\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "addps %%xmm10, %%xmm8\n"        // add gridcorrection
    "addps %%xmm11, %%xmm9\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "movaps %%xmm8, 32(%2)\n"         // Store
    "movaps %%xmm9, 48(%2)\n"       // Store
    "sub $8, %0\n"                  // size -=8
    "add $64, %2\n"                 // outcur+=64
    "add $64, %3\n"                 // gridsample+=64
    "add $32, %4\n"                 // wsharpen+=32
    "cmp $0, %0\n"
    "jg loop_sharpenonly_sse3_big\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
    );
  } else {
    asm volatile
    (
    "movaps (%1), %%xmm11\n"
    "movaps 16(%1), %%xmm12\n"
    "movaps 32(%1), %%xmm13\n"
    "movaps 48(%1), %%xmm14\n"
    "movaps 64(%1), %%xmm15\n"
    "loop_sharpenonly_sse3:\n"
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm12, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm12, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    -
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "addps %%xmm11, %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)
    "movaps %%xmm14, %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ;
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps %%xmm13, %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free

    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "movaps (%4), %%xmm6\n"         // load wsharpen[0->4]
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "rsqrtps %%xmm7, %%xmm7\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "mulps %%xmm6, %%xmm7\n"        // multiply wsharpen
    "addps %%xmm15, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_sharpenonly_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
    );
  }
}

#else  // 32 bits
void DeGridComplexFilter::processSharpenOnlySSE3(ComplexBlock* block) {
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_sharpenonly_sse3:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "movaps 32(%1), %%xmm6\n"         // Move sigmaSquaredSharpenMin into xmm6
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)
    "movaps 48(%1), %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps %%xmm6, %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free
    
    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "movaps (%4), %%xmm6\n"         // load wsharpen[0->4]
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "movaps 64(%1), %%xmm5\n"       // Load "1.0"
    "rsqrtps %%xmm7, %%xmm7\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "mulps %%xmm6, %%xmm7\n"        // multiply wsharpen 
    "addps %%xmm5, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_sharpenonly_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
  );
}
#endif
void DeGridComplexFilter::processSharpenOnlySSE(ComplexBlock* block) {
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_sharpenonly_sse:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared

    "movaps %%xmm4, %%xmm7\n"
    "shufps $136, %%xmm5, %%xmm4\n"      // xmm7 r0r1 r2r3  [10 00 10 00 = 136]  
    "shufps $221, %%xmm5, %%xmm7\n"      // xmm6 i0i1 i2i3  [11 01 11 01 = 221] 
    "movaps 32(%1), %%xmm6\n"         // Move sigmaSquaredSharpenMin into xmm6
    "addps %%xmm7, %%xmm4\n"
    "movaps 48(%1), %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7
    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps %%xmm6, %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free
    
    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "movaps (%4), %%xmm6\n"         // load wsharpen[0->4]
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "movaps 64(%1), %%xmm5\n"       // Load "1.0"
    "rsqrtps %%xmm7, %%xmm7\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "mulps %%xmm6, %%xmm7\n"        // multiply wsharpen 
    "addps %%xmm5, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_sharpenonly_sse\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
  );
}
#if defined (__x86_64__)
void ComplexWienerFilterDeGrid::processSharpen_SSE3( ComplexBlock* block )
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
    temp[i+20] = sigmaSquaredNoiseNormed; // 80
    temp[i+24] = lowlimit;                // 96
  }
  int size = bw*bh;
  asm volatile
  (
    "movaps (%1), %%xmm15\n"          // 1e-15f
    "movaps 16(%1),%%xmm14\n"         // Load gridfraction into xmm14
    "movaps 32(%1), %%xmm10\n"        //xmm10 sigmaSquaredSharpenMin
    "movaps 48(%1), %%xmm11\n"        // Move sigmaSquaredSharpenMax into xmm11
    "movaps 64(%1), %%xmm9\n"         // Load "1.0"
    "movaps 80(%1), %%xmm13\n"        //sigmaSquaredNoiseNormed in xmm13
    "movaps 96(%1), %%xmm12\n"        // xmm12 = lowlimit
    "loop_wienerdegridsharpen_sse3:\n"
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm14, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm14, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    -
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "addps %%xmm15, %%xmm4\n"         // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm13, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm12, %%xmm6\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ;
    "movaps %%xmm11, %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm11, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps %%xmm10, %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free

    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "rsqrtps %%xmm7, %%xmm7\n"       // 1 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt(...)
    "mulps (%4), %%xmm7\n"        // multiply wsharpen
    "addps %%xmm9, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "mulps %%xmm6, %%xmm7\n"        // *= Wienerfactor
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_wienerdegridsharpen_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
  );
}

#else  // 32 bits

void ComplexWienerFilterDeGrid::processSharpen_SSE3( ComplexBlock* block ) 
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
    temp[i+20] = sigmaSquaredNoiseNormed; // 80
    temp[i+24] = lowlimit;                // 96
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_wienerdegridsharpen_sse3:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free
    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps 80(%1), %%xmm5\n"           //sigmaSquaredNoiseNormed in xmm5
    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm5, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free   
    "movaps 96(%1), %%xmm5\n"           // xmm5 = lowlimit
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm5, %%xmm6\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 
    "movaps 48(%1), %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps 32(%1), %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free
    
    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "movaps 64(%1), %%xmm5\n"       // Load "1.0"
    "rsqrtps %%xmm7, %%xmm7\n"      // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "mulps (%4), %%xmm7\n"        // multiply wsharpen 
    "addps %%xmm5, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "mulps %%xmm6, %%xmm7\n"        // *= Wienerfactor
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_wienerdegridsharpen_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
  );
}
#endif

void ComplexWienerFilterDeGrid::processSharpen_SSE( ComplexBlock* block ) 
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.
  float *wsharpen = sharpenWindow->getLine(0);

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredSharpenMin;   // 32
    temp[i+12] = sigmaSquaredSharpenMax;  // 48
    temp[i+16] = 1.0f;                    // 64
    temp[i+20] = sigmaSquaredNoiseNormed; // 72
    temp[i+24] = lowlimit;                // 96
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_wienerdegridsharpen_sse:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "movaps %%xmm4, %%xmm7\n"
    "shufps $136, %%xmm5, %%xmm4\n"      // xmm7 r0r1 r2r3  [10 00 10 00 = 136]  
    "shufps $221, %%xmm5, %%xmm7\n"      // xmm6 i0i1 i2i3  [11 01 11 01 = 221] 
    "addps %%xmm7, %%xmm4\n"


    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps 80(%1), %%xmm5\n"           //sigmaSquaredNoiseNormed in xmm5
    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm5, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free   
    "movaps 96(%1), %%xmm5\n"           // xmm5 = lowlimit
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm5, %%xmm6\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    //      float sfact = (1 + wsharpen[x]*sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) )) ; 
    "movaps 48(%1), %%xmm7\n"         // Move sigmaSquaredSharpenMax into xmm7
    "movaps %%xmm4, %%xmm5\n"       // Copy psd into xmm5
    "addps %%xmm7, %%xmm4\n"        // xmm4 = psd + sigmaSquaredSharpenMax
    "mulps %%xmm5, %%xmm7\n"        // xmm7 =  psd*sigmaSquaredSharpenMax
    "addps 32(%1), %%xmm5\n"        //xmm5 = psd + sigmaSquaredSharpenMin //xmm6 free
    
    "mulps %%xmm4, %%xmm5\n"        // (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) xmm4 free
    "rcpps %%xmm5, %%xmm5\n"        // 1 / (psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax) (stall)
    "mulps %%xmm5, %%xmm7\n"        // psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax)) - xmm5 free
    "movaps 64(%1), %%xmm5\n"       // Load "1.0"
    "rsqrtps %%xmm7, %%xmm7\n"       // 1.0 / sqrt( psd*sigmaSquaredSharpenMax/((psd + sigmaSquaredSharpenMin)*(psd + sigmaSquaredSharpenMax))
    "rcpps %%xmm7, %%xmm7\n"        // sqrt (..)
    "mulps (%4), %%xmm7\n"        // multiply wsharpen 
    "addps %%xmm5, %%xmm7\n"        // + 1.0 xmm7 = sfact
    "mulps %%xmm6, %%xmm7\n"        // *= Wienerfactor
    "movaps %%xmm7, %%xmm5\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "add $16, %4\n"                 // wsharpen+=16
    "cmp $0, %0\n"
    "jg loop_wienerdegridsharpen_sse\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample), "r"(wsharpen)
    : /*  %0           %1          %2              %3               %4          */
  );

}

void ComplexWienerFilterDeGrid::processNoSharpen_SSE( ComplexBlock* block ) 
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredNoiseNormed;  // 32
    temp[i+12] = lowlimit;                // 48
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_wienerdegridnosharpen_sse:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "movaps %%xmm4, %%xmm7\n"
    "shufps $136, %%xmm5, %%xmm4\n"      // xmm7 r0r1 r2r3  [10 00 10 00 = 136]  
    "shufps $221, %%xmm5, %%xmm7\n"      // xmm6 i0i1 i2i3  [11 01 11 01 = 221] 
    "addps %%xmm7, %%xmm4\n"

    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps 32(%1), %%xmm5\n"           //sigmaSquaredNoiseNormed in xmm5
    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm5, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free   
    "movaps 48(%1), %%xmm5\n"           // xmm5 = lowlimit
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm6, %%xmm5\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    "movaps %%xmm5, %%xmm7\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "cmp $0, %0\n"
    "jg loop_wienerdegridnosharpen_sse\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample)
    : /*  %0           %1          %2              %3          */
  );

}
#if defined (__x86_64__)
void ComplexWienerFilterDeGrid::processNoSharpen_SSE3( ComplexBlock* block )
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredNoiseNormed;  // 32
    temp[i+12] = lowlimit;                // 48
  }
  int size = bw*bh;
  if ((size & 7) == 0) {  //TODO: Bench me to see if I'm faster
    asm volatile
    (
    "movaps (%1), %%xmm14\n"           // xmm14: 1e-15
    "movaps 16(%1), %%xmm15\n"         // Load gridfraction into xmm15
    "loop_wienerdegridnosharpen_sse3_big:\n"
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps 32(%2), %%xmm8\n"         // in r4i4 r5i5
    "movaps 48(%2), %%xmm9\n"         //in r6i6 r7i7
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3
    "movaps 32(%3), %%xmm10\n"         // grid r4i4 r5i5
    "movaps 48(%3), %%xmm11\n"         // grid r6i6 r7i7

    "mulps %%xmm15, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm15, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "mulps %%xmm15, %%xmm10\n"          //grid r4*gf i4*gf r5*gf i5*gf
    "mulps %%xmm15, %%xmm11\n"          //grid r6*gf i6*gf r7*gf i7*gf
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory
    "movaps %%xmm5, %%xmm3\n"
    "movaps %%xmm10, %%xmm12\n"         // maintain gridcorrection in memory
    "movaps %%xmm11, %%xmm13\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    -
    "subps %%xmm10, %%xmm8\n"          // re4 im4 re5 im5  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm11, %%xmm9\n"          // re6 im6 re7 im7    -
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"
    "movaps %%xmm8, %%xmm10\n"         // copy re4+im4
    "movaps %%xmm9, %%xmm11\n"

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "mulps %%xmm10, %%xmm10\n"          //r4i4 r5i5 squared
    "mulps %%xmm11, %%xmm11\n"          //r6i6 r7i7 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 (all squared) (SSE3!) - xmm 5 free
    "haddps %%xmm11, %%xmm10\n"       //r4+i4 r5+i5 r6+i6 r7+i7 (all squared) (SSE3!) - xmm 11 free

    "addps %%xmm14, %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)
    "addps %%xmm14, %%xmm10\n"           // add 1e-15 (xmm10: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps %%xmm4, %%xmm5\n"           // Copy psd into xmm5
    "movaps %%xmm10, %%xmm11\n"           // Copy psd into xmm11

    "rcpps %%xmm4, %%xmm4\n"            //  xmm4: (1 / psd)
    "movaps 32(%1), %%xmm7\n"
    "rcpps %%xmm10, %%xmm10\n"          //  xmm10: (1 / psd)
    "subps %%xmm7, %%xmm5\n"           // xmm5 (psd) - xmm5 (ssnn)
    "subps %%xmm7, %%xmm11\n"          // xmm11 (psd) - xmm5 (ssnn)
    "movaps 48(%1), %%xmm7\n"
    "mulps %%xmm4, %%xmm5\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "mulps %%xmm10, %%xmm11\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm7, %%xmm5\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)
    "maxps %%xmm7, %%xmm11\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    "movaps %%xmm5, %%xmm7\n"
    "movaps %%xmm11, %%xmm10\n"
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm11, %%xmm11\n"     // unpack high to xmm11
    "unpcklps %%xmm10, %%xmm10\n"     // unpack low to xmm10

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "mulps %%xmm10, %%xmm8\n"        // re+im *= sfact
    "mulps %%xmm11, %%xmm9\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "addps %%xmm12, %%xmm8\n"        // add gridcorrection
    "addps %%xmm13, %%xmm9\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "movaps %%xmm8, 32(%2)\n"         // Store
    "movaps %%xmm9, 48(%2)\n"       // Store
    "sub $8, %0\n"                  // size -=8
    "add $64, %2\n"                 // outcur+=64
    "add $64, %3\n"                 // gridsample+=64
    "cmp $0, %0\n"
    "jg loop_wienerdegridnosharpen_sse3_big\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample)
    : /*  %0           %1          %2              %3          */
    );
  } else {
    asm volatile
    (
    "movaps (%1), %%xmm14\n"           // xmm14: 1e-15
    "movaps 16(%1), %%xmm15\n"         // Load gridfraction into xmm15
    "movaps 32(%1), %%xmm13\n"         //sigmaSquaredNoiseNormed in xmm13
    "movaps 48(%1), %%xmm12\n"         // xmm12 = lowlimit
    "loop_wienerdegridnosharpen_sse3:\n"
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm15, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1)
    "mulps %%xmm15, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    -
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free

    "addps %%xmm14, %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm13, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm12, %%xmm6\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    "movaps %%xmm6, %%xmm7\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm6, %%xmm6\n"     // unpack high to xmm6

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm6, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "cmp $0, %0\n"
    "jg loop_wienerdegridnosharpen_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample)
    : /*  %0           %1          %2              %3          */
    );
  }
}

#else // 32 bits
void ComplexWienerFilterDeGrid::processNoSharpen_SSE3( ComplexBlock* block ) 
{
  fftwf_complex* outcur = block->complex;
  fftwf_complex* gridsample = grid->complex;
  float gridfraction = degrid*outcur[0][0]/gridsample[0][0];
  float* temp = block->temp->data;  // Get aligned temp area, at least 256 bytes, only used by this thread.

  for (int i = 0; i < 4; i++) {
    temp[i+0] = 1e-15f;                   // 0
    temp[i+4] = gridfraction;             // 16
    temp[i+8] = sigmaSquaredNoiseNormed;  // 32
    temp[i+12] = lowlimit;                // 48
  }
  int size = bw*bh;
  asm volatile 
  ( 
    "loop_wienerdegridnosharpen_sse3:\n"
    "movaps 16(%1),%%xmm6\n"          // Load gridfraction into xmm6
    "movaps (%2), %%xmm0\n"           // in r0i0 r1i1
    "movaps 16(%2), %%xmm1\n"         //in r2i2 r3i3
    "movaps (%3), %%xmm4\n"           // grid r0i0 r1i1
    "movaps 16(%3), %%xmm5\n"         // grid r2i2 r3i3

    "mulps %%xmm6, %%xmm4\n"          //grid r0*gf i0*gf r1*gf i1*gf (xmm4: gridcorrection0 + 1) 
    "mulps %%xmm6, %%xmm5\n"          //grid r2*gf i2*gf r3*gf i3*gf  (gridfraction*gridsample[x])
    "movaps %%xmm4, %%xmm2\n"         // maintain gridcorrection in memory 
    "movaps %%xmm5, %%xmm3\n"
    "subps %%xmm4, %%xmm0\n"          // re0 im0 re1 im1  (re = outcur[x][0] - gridcorrection0;, etc) (xmm0 - xmm4)
    "subps %%xmm5, %%xmm1\n"          // re2 im2 re3 im3    - 
    "movaps %%xmm0, %%xmm4\n"         // copy re0+im0 ... into xmm4 and 5, xmm0 & 1 retained
    "movaps %%xmm1, %%xmm5\n"          

    "mulps %%xmm4, %%xmm4\n"          //r0i0 r1i1 squared
    "mulps %%xmm5, %%xmm5\n"          //r2i2 r3i3 squared
    "haddps %%xmm5, %%xmm4\n"         //r0+i0 r1+i1 r2+i2 r3+i3 r4+i4 (all squared) (SSE3!) - xmm 5 free

    "addps (%1), %%xmm4\n"            // add 1e-15 (xmm4: psd for all 4 pixels)

    //WienerFactor = MAX((psd - sigmaSquaredNoiseNormed)/psd, lowlimit); // limited Wiener filter

    "movaps 32(%1), %%xmm5\n"           //sigmaSquaredNoiseNormed in xmm5
    "movaps %%xmm4, %%xmm6\n"           // Copy psd into xmm6
    "rcpps %%xmm4, %%xmm7\n"            //  xmm7: (1 / psd)
    "subps %%xmm5, %%xmm6\n"            // xmm6 (psd) - xmm5 (ssnn) xmm5 free   
    "movaps 48(%1), %%xmm5\n"           // xmm5 = lowlimit
    "mulps %%xmm7, %%xmm6\n"            // xmm6 = (psd - sigmaSquaredNoiseNormed)/psd
    "maxps %%xmm6, %%xmm5\n"            // xmm6 = Wienerfactor = MAX(xmm6, lowlimit)

    "movaps %%xmm5, %%xmm7\n"
    "unpcklps %%xmm7, %%xmm7\n"     // unpack low to xmm7
    "unpckhps %%xmm5, %%xmm5\n"     // unpack high to xmm5

    "mulps %%xmm7, %%xmm0\n"        // re+im *= sfact
    "mulps %%xmm5, %%xmm1\n"        // re+im *= sfact
    "addps %%xmm2, %%xmm0\n"        // add gridcorrection
    "addps %%xmm3, %%xmm1\n"        // add gridcorrection
    "movaps %%xmm0, (%2)\n"         // Store
    "movaps %%xmm1, 16(%2)\n"       // Store
    "sub $4, %0\n"                  // size -=4
    "add $32, %2\n"                 // outcur+=32
    "add $32, %3\n"                 // gridsample+=32
    "cmp $0, %0\n"
    "jg loop_wienerdegridnosharpen_sse3\n"
    : /* no output registers */
    : "r" (size), "r" (temp),  "r" (outcur), "r" (gridsample)
    : /*  %0           %1          %2              %3          */
  );
}
#endif

#endif // defined (__i386__) || defined (__x86_64__)


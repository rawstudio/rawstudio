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
#include "floatplanarimage.h"

#if defined (__i386__) || defined (__x86_64__)

void FloatPlanarImage::unpackInterleavedYUV_SSE( const ImgConvertJob* j )
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data;
  temp[0] = (0.299f* WB_R_CORR); temp[4] = 0.587f; temp[8] = (0.114f * WB_B_CORR); temp[3] = 0.0f;
  temp[1] = (-0.169f* WB_R_CORR); temp[5] = -0.331f; temp[9] = (0.499 * WB_B_CORR); temp[7] = 0.0f;
  temp[2] = (0.499f* WB_R_CORR); temp[6] = -0.418f; temp[10] =(-0.0813f * WB_B_CORR); temp[11] = 0.0f;

  float* xfer =  (float*)fftwf_malloc(3*sizeof(float));

  asm volatile
  (
    "movaps (%0), %%xmm5\n"     // R values
    "movaps 16(%0), %%xmm6\n"   // G values
    "movaps 32(%0), %%xmm7\n"   // B values
    : // no output registers
    : "r" (temp)
    : //  %0
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);

    for (int x=0; x<image->w; x++) {
      xfer[0] = shortToFloat[(*pix)];     // r
      xfer[1] = shortToFloat[(*(pix+1))]; // g
      xfer[2] = shortToFloat[(*(pix+2))]; // b
      asm volatile
      (
        "movss (%0), %%xmm0\n"        // Move r  into xmm0 (load 1 to avoid StoreLoadForward pentalty)
        "movss 4(%0), %%xmm1\n"       // Move g into xmm1
        "movss 8(%0), %%xmm2\n"       // Move r into xmm2
        "shufps $0, %%xmm0, %%xmm0\n" // Splat r
        "shufps $0, %%xmm1, %%xmm1\n" // Splat g
        "shufps $0, %%xmm2, %%xmm2\n" // Splat b
        "mulps %%xmm5, %%xmm0\n"      // Multiply R
        "mulps %%xmm6, %%xmm1\n"      // Multiply G
        "mulps %%xmm7, %%xmm2\n"      // Multiply B
        "addps %%xmm0, %%xmm1\n"      // Add first
        "addps %%xmm1, %%xmm2\n"      // Add second
        "shufps $85, %%xmm2, %%xmm0\n" // Move Cb into xmm0 lower  (85 = 01010101)
        "movss %%xmm2, (%1)\n"        // Store Y
        "movhlps %%xmm2, %%xmm1\n"      // Move Cr into xmm1 low
        "movss %%xmm0, (%2)\n"        // Store Cb
        "movss %%xmm1, (%3)\n"        // Store Cr
        : // no output registers
        : "r" (&xfer[0]), "r" (Y), "r" (Cb),  "r" (Cr)
        : //  %0         %1       %2         %3
     );
      Y++; Cb++; Cr++;
      pix += image->pixelsize;
    }
  }
  asm volatile ( "emms\n" );
  fftwf_free(xfer);

}
#endif // defined (__i386__) || defined (__x86_64__)

#if defined (__x86_64__)

void FloatPlanarImage::packInterleavedYUV_SSE2_64( const ImgConvertJob* j)
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data;
  for (int i = 0; i < 4; i++) {
    temp[i] = 1.402f;       // Cr to r
    temp[i+4] = -0.714f;    // Cr to g
    temp[i+8] = -0.344f;    // Cb to g
    temp[i+12] = 1.772f;    // Cb to b
    temp[i+16] = (1.0f/WB_R_CORR);   // Red correction
    temp[i+20] = (1.0f/WB_B_CORR);    // Blue correction
    temp[i+24] = 65535.0f;    // Saturation
  }

  asm volatile
  (
    "movaps (%0), %%xmm10\n"     // Cr to r
    "movaps 16(%0), %%xmm11\n"   // Cr to g
    "movaps 32(%0), %%xmm12\n"   // Cb to g
    "movaps 48(%0), %%xmm13\n"   // Cb to b
    "movaps 64(%0), %%xmm14\n"   // Red Correction
    "movaps 80(%0), %%xmm15\n"   // Blue Correction
    "movaps 96(%0), %%xmm9\n"   // Saturation point
    "pxor %%xmm8, %%xmm8\n"     // Zero
    : // no output registers
    : "r" (temp)
    : //  %0
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    gushort* out = GET_PIXEL(image,0,y);
    guint n = (image->w+3)>>2;
    asm volatile
    (
      "loopback_YUV_SSE2_64:"
      "movaps (%2), %%xmm1\n"         // xmm1: Cb (4 pixels)
      "movaps (%3), %%xmm2\n"         // xmm2: Cr
      "movaps (%1), %%xmm0\n"         // xmm0: Y
      "movaps %%xmm1, %%xmm3\n"       // xmm3: Cb
      "movaps %%xmm2, %%xmm4\n"       // xmm4: Cr
      "mulps %%xmm12, %%xmm1\n"       // xmm1: Cb for green
      "mulps %%xmm11, %%xmm2\n"       // xmm2: Cr for green
      "addps %%xmm0, %%xmm1\n"        // xmm1: Add Y for green
      "mulps %%xmm13, %%xmm3\n"       // xmm3: Cb for blue
      "mulps %%xmm10, %%xmm4\n"       // xmm4: Cr for red
      "addps %%xmm2, %%xmm1\n"        // Green ready in xmm1
      "addps %%xmm0, %%xmm3\n"        // Add Y to blue
      "addps %%xmm0, %%xmm4\n"        // Add Y to red - xmm 0 free
      "mulps %%xmm1, %%xmm1\n"        // Square green
      "mulps %%xmm15, %%xmm3\n"        // Multiply blue correction - maybe not needed later
      "mulps %%xmm14, %%xmm4\n"        // Multiply red correction - maybe not needed later
      "minps %%xmm9, %%xmm1\n"        // Saturate green - not needed in SSE4
      "mulps %%xmm3, %%xmm3\n"        // Square blue
      "mulps %%xmm4, %%xmm4\n"        // Square red
      "cvtps2dq %%xmm1, %%xmm1\n"     // Convert green to dwords
      "minps %%xmm9, %%xmm3\n"        // Saturate blue - not needed in SSE4
      "minps %%xmm9, %%xmm4\n"        // Saturate red - not needed in SSE4
      "cvtps2dq %%xmm3, %%xmm3\n"     // Convert blue to dwords
      "cvtps2dq %%xmm4, %%xmm4\n"     // Convert red to dwords
      // TODO: Do SSE4 convertion to avoid having to manually saturate components using PACKUSDW
      "movdqa %%xmm1, %%xmm0\n"       // Copy green into xmm0
      "movdqa %%xmm3, %%xmm2\n"       // Copy blue into xmm2
      "movdqa %%xmm4, %%xmm5\n"       // Copy red into xmm5
      "pcmpgtd %%xmm8, %%xmm1\n"      // if (xmm1 > 0) xmm1 = ones - green
      "pcmpgtd %%xmm8, %%xmm3\n"      // same for blue
      "pcmpgtd %%xmm8, %%xmm4\n"      // same for red
      "pand %%xmm0, %%xmm1\n"         // Green in xmm1
      "pand %%xmm5, %%xmm4\n"         // Red in xmm4
      "pslld $16, %%xmm1\n"           // Shift up green
      "pand %%xmm2, %%xmm3\n"         // Blue in xmm3
      "por %%xmm1, %%xmm4\n"          // Interleave red & green
      "movdqa %%xmm4, %%xmm0\n"       // Copy red &green into xmm0
      "punpckldq %%xmm3, %%xmm4\n"    // Interleave lower blue into reg&green in xmm4 Now 00b1 g1r1 00b0 g0r0
      "punpckhdq %%xmm3, %%xmm0\n"    // Interleave higher blue into reg&green in xmm0 Now 00b3 g3r3 00b2 g2r2
      "movdqa %%xmm4, (%0)\n"       // Store low pixels
      "movdqa %%xmm0, 16(%0)\n"       // Store high pixels
      "add $32, %0\n"
      "add $16, %1\n"
      "add $16, %2\n"
      "add $16, %3\n"
      "dec %4\n"
      "jnz loopback_YUV_SSE2_64\n"
      "emms\n"
      : // no output registers
      : "r" (out), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(n)
      : //  %0         %1       %2         %3       %4
     );
  }
}
#endif

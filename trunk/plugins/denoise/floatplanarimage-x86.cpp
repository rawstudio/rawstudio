/*
 * * Copyright (C) 2006-2010 Anders Brander <anders@brander.dk>, 
 * * Anders Kvist <akv@lnxbx.dk> and Klaus Post <klauspost@gmail.com>
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

namespace RawStudio {
namespace FFTFilter {

#if defined (__i386__) || defined (__x86_64__)

#if defined (__x86_64__)

// Only 64 bits, and only if pixelsize is 4
void FloatPlanarImage::unpackInterleavedYUV_SSE2( const ImgConvertJob* j )
{  
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data;
  temp[0] = redCorrection; temp[1] = 1.0f; temp[2] = blueCorrection; temp[3] = 0.0f;
  for (int i = 0; i < 4; i++) {
    temp[i+4] = (0.299);   //r->Y
    temp[i+8] = (0.587);   //g->Y
    temp[i+12] = (0.114);   //b->Y

    temp[i+16] = (-0.169);  //r->Cb
    temp[i+20] = (-0.331);  //g->Cb
    temp[i+24] = (0.499);   //b->Cb

    temp[i+28] = (0.499);   //r->Cr
    temp[i+32] = (-0.418);  //g->Cr
    temp[i+36] = (-0.0813); //b->Cr
  }

  asm volatile
  (
    "movaps 0(%0), %%xmm15\n"     // Red, green, bluecorrection
    : // no output registers
    : "r" (temp)
    : //  %0
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    const gushort* pix = GET_PIXEL(image,0,y);
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    gint w = (3+image->w) >>2;
    asm volatile
    (
        "unpack_next_pixel:\n"
        "movaps (%0), %%xmm0\n"         // Load xx,b1,g1,r1,xx,b0,g0,r0
        "movaps 16(%0), %%xmm2\n"       // Load xx,b3,g3,r3,xx,b2,g2,r2
        "prefetchnta 64(%0)\n"         // Prefetch next
        "pxor %%xmm5,%%xmm5\n"
        "movaps %%xmm0, %%xmm1\n"
        "movaps %%xmm2, %%xmm3\n"

        "punpcklwd %%xmm5,%%xmm0\n"     //00xx 00b0 00g0 00r0
        "punpckhwd %%xmm5,%%xmm1\n"     //00xx 00b1 00g1 00r1
        "punpcklwd %%xmm5,%%xmm2\n"     //00xx 00b2 00g2 00r2
        "punpckhwd %%xmm5,%%xmm3\n"     //00xx 00b3 00g3 00r3

        "cvtdq2ps %%xmm0, %%xmm0\n"     // doubleword to float
        "cvtdq2ps %%xmm1, %%xmm1\n"
        "cvtdq2ps %%xmm2, %%xmm2\n"     // doubleword to float
        "cvtdq2ps %%xmm3, %%xmm3\n"

        "mulps %%xmm15, %%xmm0\n"       // Multiply by redcorrection/bluecorrection
        "mulps %%xmm15, %%xmm1\n"       // Multiply by redcorrection/bluecorrection
        "mulps %%xmm15, %%xmm2\n"       // Multiply by redcorrection/bluecorrection
        "mulps %%xmm15, %%xmm3\n"       // Multiply by redcorrection/bluecorrection

        "rsqrtps %%xmm0, %%xmm0\n"      // 1 / sqrt()
        "rsqrtps %%xmm1, %%xmm1\n"
        "rsqrtps %%xmm2, %%xmm2\n"
        "rsqrtps %%xmm3, %%xmm3\n"

        "rcpps %%xmm0, %%xmm0\n"        // sqrt
        "rcpps %%xmm1, %%xmm1\n"        // sqrt
        "rcpps %%xmm2, %%xmm2\n"        // sqrt
        "rcpps %%xmm3, %%xmm3\n"        // sqrt

        "movaps %%xmm0, %%xmm5\n"
        "movaps %%xmm2, %%xmm7\n"
        "unpcklps %%xmm1, %%xmm0\n"     //g1 g0 r1 r0
        "unpcklps %%xmm3, %%xmm2\n"     //g3 g2 r3 r2

        "movaps %%xmm0, %%xmm4\n"       //g1 g0 r1 r0
        "movlhps %%xmm2, %%xmm0\n"      //r3 r2 r1 r0
        "movhlps %%xmm4, %%xmm2\n"      //g3 g2 g1 g0

        "unpckhps %%xmm1, %%xmm5\n"     //xx xx b1 b0
        "unpckhps %%xmm3, %%xmm7\n"     //xx xx b3 b2
        "movlhps %%xmm7, %%xmm5\n"      //b3 b2 b1 b0

        "movaps %%xmm2, %%xmm1\n"     // Green in xmm1
        "movaps %%xmm2, %%xmm4\n"     // Green (copy) in xmm4
        "movaps %%xmm5, %%xmm2\n"     // Blue in xmm2
        "movaps %%xmm0, %%xmm3\n"     // Red (copy) in xmm3

        "mulps 16(%5), %%xmm3\n"     // R->Y
        "mulps 32(%5), %%xmm4\n"     // G->Y
        "mulps 48(%5), %%xmm5\n"     // B->Y

        "movaps %%xmm0, %%xmm6\n"     // Red (copy) in xmm6
        "movaps %%xmm1, %%xmm7\n"     // Green (copy) in xmm7
        "movaps %%xmm2, %%xmm8\n"     // Blue (copy) in xmm8

        "mulps 64(%5), %%xmm0\n"     // R->Cb
        "mulps 80(%5), %%xmm1\n"     // G->Cb
        "mulps 96(%5), %%xmm2\n"     // B->Cb

        "addps %%xmm4, %%xmm3\n"     // Add Y
        "addps %%xmm1, %%xmm0\n"     // Add Cb

        "mulps 112(%5), %%xmm6\n"     // R->Cr
        "mulps 128(%5), %%xmm7\n"     // G->Cr
        "mulps 144(%5), %%xmm8\n"     // B->Cr

        "addps %%xmm5, %%xmm3\n"     // Add Y (finished)
        "addps %%xmm2, %%xmm0\n"     // Add Cb (finished)
        "addps %%xmm7, %%xmm6\n"     // Add Cr
        "addps %%xmm8, %%xmm6\n"     // Add Cr (finished)

        "movntdq %%xmm3, (%1)\n"      // Store Y
        "movntdq %%xmm0, (%2)\n"      // Store Cb
        "movntdq %%xmm6, (%3)\n"      // Store Cr

        "add $32, %0\n"
        "add $16, %1\n"
        "add $16, %2\n"
        "add $16, %3\n"
        "dec %4\n"
        "jnz unpack_next_pixel\n"
        : // no output registers
        : "r" (pix), "r" (Y), "r" (Cb),  "r" (Cr),  "r" (w), "r" (temp)
         // %0         %1       %2         %3           %4    %5  
        : "%rax", "%rbx", "%rcx"
     );
  }
  asm volatile ( "emms\nsfence\n" );

}
#endif // defined (__x86_64__)

#if defined (__x86_64__)

void FloatPlanarImage::packInterleavedYUV_SSE2( const ImgConvertJob* j)
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data;
  for (int i = 0; i < 4; i++) {
    temp[i] = 1.402f;       // Cr to r
    temp[i+4] = -0.714f;    // Cr to g
    temp[i+8] = -0.344f;    // Cb to g
    temp[i+12] = 1.772f;    // Cb to b
    temp[i+16] = (1.0f/redCorrection);   // Red correction
    temp[i+20] = (1.0f/blueCorrection);    // Blue correction
    *((gint*)&temp[i+24]) = 32768;        // Subtract
    *((guint*)&temp[i+28]) = 0x80008000;    // xor sign shift
  }

  asm volatile
  (
    "movaps (%0), %%xmm10\n"     // Cr to r
    "movaps 16(%0), %%xmm11\n"   // Cr to g
    "movaps 32(%0), %%xmm12\n"   // Cb to g
    "movaps 48(%0), %%xmm13\n"   // Cb to b
    "movaps 64(%0), %%xmm14\n"   // Red Correction
    "movaps 80(%0), %%xmm15\n"   // Blue Correction
    "movaps 96(%0), %%xmm9\n"   // 0x00008000
    "pxor %%xmm8, %%xmm8\n"     // Zero
    "movaps 112(%0), %%xmm7\n"   // word 0x8000
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
      "cvtps2dq %%xmm1, %%xmm1\n"     // Convert green to dwords
      "mulps %%xmm3, %%xmm3\n"        // Square blue
      "mulps %%xmm4, %%xmm4\n"        // Square red
      "mulps %%xmm15, %%xmm3\n"       // Multiply blue correction - maybe not needed later
      "mulps %%xmm14, %%xmm4\n"       // Multiply red correction - maybe not needed later
      "psubd %%xmm9, %%xmm1\n"        // g = g - 32768  ( to avoid saturation)
      "cvtps2dq %%xmm3, %%xmm3\n"     // Convert blue to dwords
      "packssdw %%xmm1,%%xmm1\n"      // g3g2 g1g0 g3g2 g1g0
      "cvtps2dq %%xmm4, %%xmm4\n"     // Convert red to dwords
      "pxor %%xmm7, %%xmm1\n"         // Shift sign
      "psubd %%xmm9, %%xmm3\n"        // b = b - 32768  ( to avoid saturation)
      "psubd %%xmm9, %%xmm4\n"        // r = r - 32768  ( to avoid saturation)
      "packssdw %%xmm3,%%xmm3\n"      // b3b2 b1b0 b3b2 b1b0
      "packssdw %%xmm4,%%xmm4\n"      // g3g2 g1g0 r3r2 r1r0
      "pxor %%xmm7, %%xmm3\n"         // Shift sign (b)
      "pxor %%xmm7, %%xmm4\n"         // Shift sign (r)
      "punpcklwd %%xmm1, %%xmm4\n"    // g3r3 g2r2 g1r1 g0r0
      "punpcklwd %%xmm8, %%xmm3\n"    // 00b3 00b2 00b1 00b0
      "movdqa %%xmm4, %%xmm0\n"       // Copy r&g
      "punpckldq %%xmm3, %%xmm4\n"    // Interleave lower blue into reg&green in xmm4 Now 00b1 g1r1 00b0 g0r0
      "punpckhdq %%xmm3, %%xmm0\n"    // Interleave higher blue into reg&green in xmm0 Now 00b3 g3r3 00b2 g2r2
      "movntdq %%xmm4, (%0)\n"         // Store low pixels
      "movntdq %%xmm0, 16(%0)\n"       // Store high pixels
      "add $32, %0\n"
      "add $16, %1\n"
      "add $16, %2\n"
      "add $16, %3\n"
      "dec %4\n"
      "jnz loopback_YUV_SSE2_64\n"
      : // no output registers
      : "r" (out), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(n)
      : //  %0         %1       %2         %3       %4
     );
  }
  asm volatile ( "emms\nsfence\n" );
}

void FloatPlanarImage::packInterleavedYUV_SSE4( const ImgConvertJob* j)
{
  RS_IMAGE16* image = j->rs;
  float* temp = p[0]->data;
  for (int i = 0; i < 4; i++) {
    temp[i] = 1.402f;       // Cr to r
    temp[i+4] = -0.714f;    // Cr to g
    temp[i+8] = -0.344f;    // Cb to g
    temp[i+12] = 1.772f;    // Cb to b
    temp[i+16] = (1.0f/redCorrection);   // Red correction
    temp[i+20] = (1.0f/blueCorrection);    // Blue correction
  }

  asm volatile
  (
    "movaps (%0), %%xmm10\n"     // Cr to r
    "movaps 16(%0), %%xmm11\n"   // Cr to g
    "movaps 32(%0), %%xmm12\n"   // Cb to g
    "movaps 48(%0), %%xmm13\n"   // Cb to b
    "movaps 64(%0), %%xmm14\n"   // Red Correction
    "movaps 80(%0), %%xmm15\n"   // Blue Correction
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
      "loopback_YUV_SSE4_64:"
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
      "mulps %%xmm3, %%xmm3\n"        // Square blue
      "mulps %%xmm4, %%xmm4\n"        // Square red
      "cvtps2dq %%xmm1, %%xmm1\n"     // Convert green to dwords
      "mulps %%xmm15, %%xmm3\n"        // Multiply blue correction - maybe not needed later
      "mulps %%xmm14, %%xmm4\n"        // Multiply red correction - maybe not needed later
      "cvtps2dq %%xmm4, %%xmm4\n"     // Convert red to dwords
      "cvtps2dq %%xmm3, %%xmm3\n"     // Convert blue to dwords
      "packusdw %%xmm1, %%xmm1\n"     // green g3g2 g1g0 g3g2 g1g0
      "packusdw %%xmm3, %%xmm3\n"     // blue
      "packusdw %%xmm4, %%xmm4\n"     // red
      "pxor %%xmm0,%%xmm0\n"          // Not really needed, but almost a no-op, so we play nice
      "punpcklwd %%xmm1,%%xmm4\n"   // red + green interleaved g3r3 g2r2 g1r1 g0r0
      "punpcklwd %%xmm0,%%xmm3\n"   // blue zero interleaved 00b3 00b2 00b1 00b0
      "movdqa %%xmm4, %%xmm1\n"     // Copy r+g
      "punpckldq %%xmm3,%%xmm4\n"   // interleave r+g and blue low
      "punpckhdq %%xmm3,%%xmm1\n"   // interleave r+g and blue high

      "movntdq %%xmm4, (%0)\n"       // Store low pixels
      "movntdq %%xmm1, 16(%0)\n"       // Store high pixels
      "add $32, %0\n"
      "add $16, %1\n"
      "add $16, %2\n"
      "add $16, %3\n"
      "dec %4\n"
      "jnz loopback_YUV_SSE4_64\n"
      : // no output registers
      : "r" (out), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(n)
      : //  %0         %1       %2         %3       %4
     );
  }
  asm volatile ( "emms\nsfence\n" );
}

#else  // 32 bits

void FloatPlanarImage::packInterleavedYUV_SSE2( const ImgConvertJob* j)
{
  RS_IMAGE16* image = j->rs;
  float temp[32] __attribute__ ((aligned (16)));
  for (int i = 0; i < 4; i++) {
    temp[i] = 1.402f;       // Cr to r
    temp[i+4] = -0.714f;    // Cr to g
    temp[i+8] = -0.344f;    // Cb to g
    temp[i+12] = 1.772f;    // Cb to b
    temp[i+16] = (1.0f/redCorrection);   // Red correction
    temp[i+20] = (1.0f/blueCorrection);    // Blue correction
    *((gint*)&temp[i+24]) = 32768;        // Subtract
    *((guint*)&temp[i+28]) = 0x80008000;    // xor sign shift
  }
  int* itemp = (int*)(&temp[28]);

  asm volatile
  (
    "movaps 96(%0), %%xmm7\n"   // Subtract
    "movaps 112(%0), %%xmm5\n"   // Xor sign
    "pxor %%xmm6, %%xmm6\n"     // Zero
    : // no output registers
    : "r" (temp)
    : //  %0
  );
  for (int y = j->start_y; y < j->end_y; y++ ) {
    gfloat *Y = p[0]->getAt(ox, y+oy);
    gfloat *Cb = p[1]->getAt(ox, y+oy);
    gfloat *Cr = p[2]->getAt(ox, y+oy);
    gushort* out = GET_PIXEL(image,0,y);
    itemp[0] = (image->w+3)>>2;
    asm volatile
    (
      "loopback_YUV_SSE2_32:"
      "movaps (%2), %%xmm1\n"         // xmm1: Cb (4 pixels)
      "movaps (%3), %%xmm2\n"         // xmm2: Cr
      "movaps (%1), %%xmm0\n"         // xmm0: Y
      "movaps %%xmm1, %%xmm3\n"       // xmm3: Cb
      "movaps %%xmm2, %%xmm4\n"       // xmm4: Cr
      "mulps 32(%4), %%xmm1\n"       // xmm1: Cb for green
      "mulps 16(%4), %%xmm2\n"       // xmm2: Cr for green
      "addps %%xmm0, %%xmm1\n"        // xmm1: Add Y for green
      "mulps 48(%4), %%xmm3\n"        // xmm3: Cb for blue
      "mulps (%4), %%xmm4\n"          // xmm4: Cr for red
      "addps %%xmm2, %%xmm1\n"        // Green ready in xmm1
      "addps %%xmm0, %%xmm3\n"        // Add Y to blue
      "addps %%xmm0, %%xmm4\n"        // Add Y to red - xmm 0 free
      "mulps %%xmm1, %%xmm1\n"        // Square green
      "mulps %%xmm3, %%xmm3\n"        // Square blue
      "mulps %%xmm4, %%xmm4\n"        // Square red
      "cvtps2dq %%xmm1, %%xmm1\n"     // Convert green to dwords
      "mulps 80(%4), %%xmm3\n"        // Multiply blue correction - maybe not needed later
      "mulps 64(%4), %%xmm4\n"        // Multiply red correction - maybe not needed later
      "psubd %%xmm7, %%xmm1\n"        // g = g - 32768  ( to avoid saturation)
      "cvtps2dq %%xmm3, %%xmm3\n"     // Convert blue to dwords
      "packssdw %%xmm1,%%xmm1\n"      // g3g2 g1g0 g3g2 g1g0
      "cvtps2dq %%xmm4, %%xmm4\n"     // Convert red to dwords
      "pxor %%xmm5, %%xmm1\n"         // Shift sign
      "psubd %%xmm7, %%xmm3\n"        // b = b - 32768  ( to avoid saturation)
      "psubd %%xmm7, %%xmm4\n"        // r = r - 32768  ( to avoid saturation)
      "packssdw %%xmm3,%%xmm3\n"      // b3b2 b1b0 b3b2 b1b0
      "packssdw %%xmm4,%%xmm4\n"      // g3g2 g1g0 r3r2 r1r0
      "pxor %%xmm5, %%xmm3\n"         // Shift sign (b)
      "pxor %%xmm5, %%xmm4\n"         // Shift sign (r)
      "punpcklwd %%xmm1, %%xmm4\n"    // g3r3 g2r2 g1r1 g0r0
      "punpcklwd %%xmm6, %%xmm3\n"    // 00b3 00b2 00b1 00b0
      "movdqa %%xmm4, %%xmm0\n"       // Copy r&g
      "punpckldq %%xmm3, %%xmm4\n"    // Interleave lower blue into reg&green in xmm4 Now 00b1 g1r1 00b0 g0r0
      "punpckhdq %%xmm3, %%xmm0\n"    // Interleave higher blue into reg&green in xmm0 Now 00b3 g3r3 00b2 g2r2

      "movdqa %%xmm4, (%0)\n"       // Store low pixels
      "movdqa %%xmm0, 16(%0)\n"       // Store high pixels
      "add $32, %0\n"
      "add $16, %1\n"
      "add $16, %2\n"
      "add $16, %3\n"
      "decl 112(%4)\n"
      "jnz loopback_YUV_SSE2_32\n"
      "emms\n"
      : // no output registers
      : "r" (out), "r" (Y), "r" (Cb),  "r" (Cr),  "r"(temp)
      : //  %0         %1       %2         %3       %4
     );
  }
}

#endif

#endif // defined (__i386__) || defined (__x86_64__)

}}// namespace RawStudio::FFTFilter

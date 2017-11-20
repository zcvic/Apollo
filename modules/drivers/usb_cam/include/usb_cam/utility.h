/*********************************************************************
 *
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, Robert Bosch LLC.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Robert Bosch nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *********************************************************************/
#ifndef USB_CAM_INCLUDE_USB_CAM_UTILITY_H_
#define USB_CAM_INCLUDE_USB_CAM_UTILITY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include <immintrin.h>
#include <x86intrin.h>

#include <ros/ros.h>

namespace usb_cam {

const unsigned char uchar_clipping_table[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // -128 - -121
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // -120 - -113
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // -112 - -105
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // -104 -  -97
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -96 -  -89
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -88 -  -81
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -80 -  -73
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -72 -  -65
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -64 -  -57
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -56 -  -49
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -48 -  -41
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -40 -  -33
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -32 -  -25
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -24 -  -17
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //  -16 -   -9
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, //   -8 -   -1
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
    137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182,
    183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205,
    206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228,
    229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251,
    252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, // 256-263
    255, 255, 255, 255, 255, 255, 255, 255, // 264-271
    255, 255, 255, 255, 255, 255, 255, 255, // 272-279
    255, 255, 255, 255, 255, 255, 255, 255, // 280-287
    255, 255, 255, 255, 255, 255, 255, 255, // 288-295
    255, 255, 255, 255, 255, 255, 255, 255, // 296-303
    255, 255, 255, 255, 255, 255, 255, 255, // 304-311
    255, 255, 255, 255, 255, 255, 255, 255, // 312-319
    255, 255, 255, 255, 255, 255, 255, 255, // 320-327
    255, 255, 255, 255, 255, 255, 255, 255, // 328-335
    255, 255, 255, 255, 255, 255, 255, 255, // 336-343
    255, 255, 255, 255, 255, 255, 255, 255, // 344-351
    255, 255, 255, 255, 255, 255, 255, 255, // 352-359
    255, 255, 255, 255, 255, 255, 255, 255, // 360-367
    255, 255, 255, 255, 255, 255, 255, 255, // 368-375
    255, 255, 255, 255, 255, 255, 255, 255, // 376-383
    };

const int clipping_table_offset = 128;

void errno_exit(const char * s);
int xioctl(int fd, int request, void * arg);
unsigned char CLIPVALUE(int val);
void YUV2RGB(const unsigned char y, const unsigned char u, const unsigned char v, unsigned char* r,
                    unsigned char* g, unsigned char* b);
void uyvy2rgb(char *YUV, char *RGB, int NumPixels);
void mono102mono8(char *RAW, char *MONO, int NumPixels);
void yuyv2rgb(char *YUV, char *RGB, int NumPixels);
void yuyv2rgb_gpu(char *YUV, char *RGB, int NumPixels);
void yuyv2rgb_thread(char *YUV, char *RGB, int NumPixels);
void yuyv2rgb_avx(unsigned char *YUV, unsigned char *RGB, int NumPixels);
void rgb242rgb(char *YUV, char *RGB, int NumPixels);

int cuda_test();

#define SIMD_INLINE inline __attribute__ ((always_inline))

void print_m256(const __m256i a);
void print_m256_i32(const __m256i a);
void print_m256_i16(const __m256i a);

template <class T> SIMD_INLINE char GetChar(T value, size_t index)
{
	return ((char*)&value)[index];
}

#define SIMD_CHAR_AS_LONGLONG(a) (((long long)a) & 0xFF)

#define SIMD_SHORT_AS_LONGLONG(a) (((long long)a) & 0xFFFF)

#define SIMD_INT_AS_LONGLONG(a) (((long long)a) & 0xFFFFFFFF)

#define SIMD_LL_SET1_EPI8(a) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(a) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 16) | (SIMD_CHAR_AS_LONGLONG(a) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 32) | (SIMD_CHAR_AS_LONGLONG(a) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 48) | (SIMD_CHAR_AS_LONGLONG(a) << 56)

#define SIMD_LL_SET2_EPI8(a, b) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 16) | (SIMD_CHAR_AS_LONGLONG(b) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 32) | (SIMD_CHAR_AS_LONGLONG(b) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 48) | (SIMD_CHAR_AS_LONGLONG(b) << 56)

#define SIMD_LL_SETR_EPI8(a, b, c, d, e, f, g, h) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(c) << 16) | (SIMD_CHAR_AS_LONGLONG(d) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(e) << 32) | (SIMD_CHAR_AS_LONGLONG(f) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(g) << 48) | (SIMD_CHAR_AS_LONGLONG(h) << 56)

#define SIMD_LL_SET1_EPI16(a) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(a) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(a) << 32) | (SIMD_SHORT_AS_LONGLONG(a) << 48)

#define SIMD_LL_SET2_EPI16(a, b) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(a) << 32) | (SIMD_SHORT_AS_LONGLONG(b) << 48)

#define SIMD_LL_SETR_EPI16(a, b, c, d) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(c) << 32) | (SIMD_SHORT_AS_LONGLONG(d) << 48)

#define SIMD_LL_SET1_EPI32(a) \
    SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(a) << 32)

#define SIMD_LL_SET2_EPI32(a, b) \
    SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(b) << 32)

#define SIMD_MM256_SET1_EPI8(a) \
    {SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), \
    SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a)}

#define SIMD_MM256_SET2_EPI8(a0, a1) \
    {SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), \
    SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1)}

#define SIMD_MM256_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
    {SIMD_LL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7), SIMD_LL_SETR_EPI8(a8, a9, aa, ab, ac, ad, ae, af), \
    SIMD_LL_SETR_EPI8(b0, b1, b2, b3, b4, b5, b6, b7), SIMD_LL_SETR_EPI8(b8, b9, ba, bb, bc, bd, be, bf)}

#define SIMD_MM256_SET1_EPI16(a) \
    {SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), \
    SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a)}

#define SIMD_MM256_SET2_EPI16(a0, a1) \
    {SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), \
    SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1)}

#define SIMD_MM256_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_LL_SETR_EPI16(a0, a1, a2, a3), SIMD_LL_SETR_EPI16(a4, a5, a6, a7), \
    SIMD_LL_SETR_EPI16(a8, a9, aa, ab), SIMD_LL_SETR_EPI16(ac, ad, ae, af)}

#define SIMD_MM256_SET1_EPI32(a) \
    {SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), \
    SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a)}

#define SIMD_MM256_SET2_EPI32(a0, a1) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), \
    SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1)}

#define SIMD_MM256_SETR_EPI32(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a2, a3), \
    SIMD_LL_SET2_EPI32(a4, a5), SIMD_LL_SET2_EPI32(a6, a7)}


const size_t A = sizeof(__m256i);
const size_t DA = 2 * A;
const size_t QA = 4 * A;
const size_t OA = 8 * A;
const size_t HA = A / 2;

const __m256i K_ZERO = SIMD_MM256_SET1_EPI8(0);
const __m256i K_INV_ZERO = SIMD_MM256_SET1_EPI8(0xFF);

const __m256i K8_01 = SIMD_MM256_SET1_EPI8(0x01);
const __m256i K8_02 = SIMD_MM256_SET1_EPI8(0x02);
const __m256i K8_04 = SIMD_MM256_SET1_EPI8(0x04);
const __m256i K8_08 = SIMD_MM256_SET1_EPI8(0x08);
const __m256i K8_10 = SIMD_MM256_SET1_EPI8(0x10);
const __m256i K8_20 = SIMD_MM256_SET1_EPI8(0x20);
const __m256i K8_40 = SIMD_MM256_SET1_EPI8(0x40);
const __m256i K8_80 = SIMD_MM256_SET1_EPI8(0x80);

const __m256i K8_01_FF = SIMD_MM256_SET2_EPI8(0x01, 0xFF);

const __m256i K16_0001 = SIMD_MM256_SET1_EPI16(0x0001);
const __m256i K16_0002 = SIMD_MM256_SET1_EPI16(0x0002);
const __m256i K16_0003 = SIMD_MM256_SET1_EPI16(0x0003);
const __m256i K16_0004 = SIMD_MM256_SET1_EPI16(0x0004);
const __m256i K16_0005 = SIMD_MM256_SET1_EPI16(0x0005);
const __m256i K16_0006 = SIMD_MM256_SET1_EPI16(0x0006);
const __m256i K16_0008 = SIMD_MM256_SET1_EPI16(0x0008);
const __m256i K16_0010 = SIMD_MM256_SET1_EPI16(0x0010);
const __m256i K16_0018 = SIMD_MM256_SET1_EPI16(0x0018);
const __m256i K16_0020 = SIMD_MM256_SET1_EPI16(0x0020);
const __m256i K16_0080 = SIMD_MM256_SET1_EPI16(0x0080);
const __m256i K16_00FF = SIMD_MM256_SET1_EPI16(0x00FF);
const __m256i K16_FF00 = SIMD_MM256_SET1_EPI16(0xFF00);

const __m256i K32_00000001 = SIMD_MM256_SET1_EPI32(0x00000001);
const __m256i K32_00000002 = SIMD_MM256_SET1_EPI32(0x00000002);
const __m256i K32_00000004 = SIMD_MM256_SET1_EPI32(0x00000004);
const __m256i K32_00000008 = SIMD_MM256_SET1_EPI32(0x00000008);
const __m256i K32_000000FF = SIMD_MM256_SET1_EPI32(0x000000FF);
const __m256i K32_0000FFFF = SIMD_MM256_SET1_EPI32(0x0000FFFF);
const __m256i K32_00010000 = SIMD_MM256_SET1_EPI32(0x00010000);
const __m256i K32_01000000 = SIMD_MM256_SET1_EPI32(0x01000000);
const __m256i K32_FFFFFF00 = SIMD_MM256_SET1_EPI32(0xFFFFFF00);



const __m256i K8_SHUFFLE_BGR0_TO_BLUE = SIMD_MM256_SETR_EPI8(
	0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR1_TO_BLUE = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD,
	0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR2_TO_BLUE = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD);

const __m256i Y_SHUFFLE0 = SIMD_MM256_SETR_EPI8(
	0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, -1, -1, -1, -1, -1, -1, -1, -1,
	0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i Y_SHUFFLE1 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe,
	-1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe );

const __m256i U_SHUFFLE0 = SIMD_MM256_SETR_EPI8(
	0x1, 0x5, 0x9, 0xd, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0x1, 0x5, 0x9, 0xd, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i U_SHUFFLE1 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd,  -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd,  -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i U_SHUFFLE2 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd, -1, -1, -1, -1);

const __m256i U_SHUFFLE3 = SIMD_MM256_SETR_EPI8(
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x5, 0x9, 0xd );
const __m256i U_SHUFFLE4 = SIMD_MM256_SETR_EPI8(
	0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x5, 0x0, 0x0, 0x0,
	0x2, 0x0, 0x0, 0x0, 0x6, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0, 0x0);


const __m256i V_SHUFFLE0 = SIMD_MM256_SETR_EPI8(
	0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i V_SHUFFLE1 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i V_SHUFFLE2 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf, -1, -1, -1, -1 );

const __m256i V_SHUFFLE3 = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x3, 0x7, 0xb, 0xf );

const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR0 = SIMD_MM256_SETR_EPI8(
	0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5,
	-1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1);
const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR1 = SIMD_MM256_SETR_EPI8(
	-1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1,
	0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD);
const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR2 = SIMD_MM256_SETR_EPI8(
	-1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1,
	-1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1);

const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR0 = SIMD_MM256_SETR_EPI8(
	-1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1,
	0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA);
const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR1 = SIMD_MM256_SETR_EPI8(
	-1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1,
	-1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1);
const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR2 = SIMD_MM256_SETR_EPI8(
	0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA,
	-1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1);

const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR0 = SIMD_MM256_SETR_EPI8(
	-1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1,
	-1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1);
const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR1 = SIMD_MM256_SETR_EPI8(
	0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7,
	-1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1);
const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR2 = SIMD_MM256_SETR_EPI8(
	-1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1,
	0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF);


const __m256i K8_SHUFFLE_BGR0_TO_GREEN = SIMD_MM256_SETR_EPI8(
	0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR1_TO_GREEN = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE,
	0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR2_TO_GREEN = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE);

const __m256i K8_SHUFFLE_BGR0_TO_RED = SIMD_MM256_SETR_EPI8(
	0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR1_TO_RED = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF,
	0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
const __m256i K8_SHUFFLE_BGR2_TO_RED = SIMD_MM256_SETR_EPI8(
	-1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF);

const __m256i K16_Y_ADJUST = SIMD_MM256_SET1_EPI16(0);
const __m256i K16_UV_ADJUST = SIMD_MM256_SET1_EPI16(128);
const int Y_ADJUST = 0;
const int UV_ADJUST = 128;
const int YUV_TO_BGR_AVERAGING_SHIFT = 13;
const int YUV_TO_BGR_ROUND_TERM = 0;// 1 << (YUV_TO_BGR_AVERAGING_SHIFT);
//const int Y_TO_RGB_WEIGHT = int(((1 << YUV_TO_BGR_AVERAGING_SHIFT)) + 0.5);
//const int U_TO_BLUE_WEIGHT = int(2.041*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
//const int U_TO_GREEN_WEIGHT = -int(0.3455*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
//const int V_TO_GREEN_WEIGHT = -int(0.7169*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
//const int V_TO_RED_WEIGHT = int(1.4065*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
const int Y_TO_RGB_WEIGHT = int(((1 << YUV_TO_BGR_AVERAGING_SHIFT)) );
const int U_TO_BLUE_WEIGHT = int(2.041*(1 << YUV_TO_BGR_AVERAGING_SHIFT) );
const int U_TO_GREEN_WEIGHT = -int(0.3455*(1 << YUV_TO_BGR_AVERAGING_SHIFT) );
const int V_TO_GREEN_WEIGHT = -int(0.7169*(1 << YUV_TO_BGR_AVERAGING_SHIFT));
const int V_TO_RED_WEIGHT = int(1.4065*(1 << YUV_TO_BGR_AVERAGING_SHIFT));
//const int V_TO_RED_WEIGHT = int(1*(1 << YUV_TO_BGR_AVERAGING_SHIFT));

const __m256i K16_YRGB_RT = SIMD_MM256_SET2_EPI16(Y_TO_RGB_WEIGHT, YUV_TO_BGR_ROUND_TERM);
const __m256i K16_VR_0 = SIMD_MM256_SET2_EPI16(V_TO_RED_WEIGHT, 0);
const __m256i K16_UG_VG = SIMD_MM256_SET2_EPI16(U_TO_GREEN_WEIGHT, V_TO_GREEN_WEIGHT);
const __m256i K16_UB_0 = SIMD_MM256_SET2_EPI16(U_TO_BLUE_WEIGHT, 0);




template <bool align> SIMD_INLINE __m256i Load(const __m256i * p);

template <> SIMD_INLINE __m256i Load<false>(const __m256i * p)
{
	return _mm256_loadu_si256(p);
}

template <> SIMD_INLINE __m256i Load<true>(const __m256i * p)
{
	return _mm256_load_si256(p);
}


SIMD_INLINE void * AlignLo(const void * ptr, size_t align)
{
	return (void *)(((size_t)ptr) & ~(align - 1));
}

SIMD_INLINE bool Aligned(const void * ptr, size_t align = sizeof(__m256))
{
	return ptr == AlignLo(ptr, align);
}


template <bool align> SIMD_INLINE void Store(__m256i * p, __m256i a);

template <> SIMD_INLINE void Store<false>(__m256i * p, __m256i a)
{
	_mm256_storeu_si256(p, a);
}

template <> SIMD_INLINE void Store<true>(__m256i * p, __m256i a)
{
	_mm256_store_si256(p, a);
}




SIMD_INLINE __m256i SaturateI16ToU8(__m256i value)
{
	return _mm256_min_epi16(K16_00FF, _mm256_max_epi16(value, K_ZERO));
}

SIMD_INLINE __m256i AdjustY16(__m256i y16)
{
	return _mm256_subs_epi16(y16, K16_Y_ADJUST);
}

SIMD_INLINE __m256i AdjustUV16(__m256i uv16)
{
	return _mm256_subs_epi16(uv16, K16_UV_ADJUST);
}

SIMD_INLINE __m256i AdjustedYuvToRed32(__m256i y16_1, __m256i v16_0)
{
	
	//print_m256_i16(y16_1);
	//print_m256_i16(v16_0);
	//print_m256_i32(_mm256_madd_epi16(y16_1, K16_YRGB_RT));
	//print_m256_i32(_mm256_madd_epi16(v16_0, K16_VR_0));
	return _mm256_srai_epi32(_mm256_add_epi32(_mm256_madd_epi16(y16_1, K16_YRGB_RT),
		_mm256_madd_epi16(v16_0, K16_VR_0)), YUV_TO_BGR_AVERAGING_SHIFT);
}

SIMD_INLINE __m256i AdjustedYuvToRed16(__m256i y16, __m256i v16)
{
	//print_m256_i32(AdjustedYuvToRed32(_mm256_unpacklo_epi16(y16, K16_0001), _mm256_unpacklo_epi16(v16, K_ZERO)));
	//print_m256_i16(_mm256_unpacklo_epi16(y16, K16_0001));
	return SaturateI16ToU8(_mm256_packs_epi32(
		AdjustedYuvToRed32(_mm256_unpacklo_epi16(y16, K16_0001), _mm256_unpacklo_epi16(v16, K_ZERO)),
		AdjustedYuvToRed32(_mm256_unpackhi_epi16(y16, K16_0001), _mm256_unpackhi_epi16(v16, K_ZERO))));
}

SIMD_INLINE __m256i AdjustedYuvToGreen32(__m256i y16_1, __m256i u16_v16)
{
	return _mm256_srai_epi32(_mm256_add_epi32(_mm256_madd_epi16(y16_1, K16_YRGB_RT),
		_mm256_madd_epi16(u16_v16, K16_UG_VG)), YUV_TO_BGR_AVERAGING_SHIFT);
}

SIMD_INLINE __m256i AdjustedYuvToGreen16(__m256i y16, __m256i u16, __m256i v16)
{
	return SaturateI16ToU8(_mm256_packs_epi32(
		AdjustedYuvToGreen32(_mm256_unpacklo_epi16(y16, K16_0001), _mm256_unpacklo_epi16(u16, v16)),
		AdjustedYuvToGreen32(_mm256_unpackhi_epi16(y16, K16_0001), _mm256_unpackhi_epi16(u16, v16))));
}

SIMD_INLINE __m256i AdjustedYuvToBlue32(__m256i y16_1, __m256i u16_0)
{
	return _mm256_srai_epi32(_mm256_add_epi32(_mm256_madd_epi16(y16_1, K16_YRGB_RT),
		_mm256_madd_epi16(u16_0, K16_UB_0)), YUV_TO_BGR_AVERAGING_SHIFT);
}

SIMD_INLINE __m256i AdjustedYuvToBlue16(__m256i y16, __m256i u16)
{
	return SaturateI16ToU8(_mm256_packs_epi32(
		AdjustedYuvToBlue32(_mm256_unpacklo_epi16(y16, K16_0001), _mm256_unpacklo_epi16(u16, K_ZERO)),
		AdjustedYuvToBlue32(_mm256_unpackhi_epi16(y16, K16_0001), _mm256_unpackhi_epi16(u16, K_ZERO))));
}

SIMD_INLINE __m256i YuvToRed(__m256i y, __m256i v)
{

	__m256i lo = AdjustedYuvToRed16(
		_mm256_unpacklo_epi8(y, K_ZERO),
		AdjustUV16(_mm256_unpacklo_epi8(v, K_ZERO)));
	__m256i hi = AdjustedYuvToRed16(
		(_mm256_unpackhi_epi8(y, K_ZERO)),
		AdjustUV16(_mm256_unpackhi_epi8(v, K_ZERO)));

	//print_m256_i16(lo);
	//print_m256_i16(hi);
	return _mm256_packus_epi16(lo, hi);
}

SIMD_INLINE __m256i YuvToGreen(__m256i y, __m256i u, __m256i v)
{
	__m256i lo = AdjustedYuvToGreen16(
		(_mm256_unpacklo_epi8(y, K_ZERO)),
		AdjustUV16(_mm256_unpacklo_epi8(u, K_ZERO)),
		AdjustUV16(_mm256_unpacklo_epi8(v, K_ZERO)));
	__m256i hi = AdjustedYuvToGreen16(
		(_mm256_unpackhi_epi8(y, K_ZERO)),
		AdjustUV16(_mm256_unpackhi_epi8(u, K_ZERO)),
		AdjustUV16(_mm256_unpackhi_epi8(v, K_ZERO)));
	return _mm256_packus_epi16(lo, hi);
}

SIMD_INLINE __m256i YuvToBlue(__m256i y, __m256i u)
{
	__m256i lo = AdjustedYuvToBlue16(
		(_mm256_unpacklo_epi8(y, K_ZERO)),
		AdjustUV16(_mm256_unpacklo_epi8(u, K_ZERO)));
	__m256i hi = AdjustedYuvToBlue16(
		(_mm256_unpackhi_epi8(y, K_ZERO)),
		AdjustUV16(_mm256_unpackhi_epi8(u, K_ZERO)));
	return _mm256_packus_epi16(lo, hi);
}


template <int index> __m256i InterleaveBgr(__m256i blue, __m256i green, __m256i red);

template<> SIMD_INLINE __m256i InterleaveBgr<0>(__m256i blue, __m256i green, __m256i red)
{
	return
		_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(blue, 0x44), K8_SHUFFLE_PERMUTED_BLUE_TO_BGR0),
			_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(green, 0x44), K8_SHUFFLE_PERMUTED_GREEN_TO_BGR0),
				_mm256_shuffle_epi8(_mm256_permute4x64_epi64(red, 0x44), K8_SHUFFLE_PERMUTED_RED_TO_BGR0)));
}

template<> SIMD_INLINE __m256i InterleaveBgr<1>(__m256i blue, __m256i green, __m256i red)
{
	return
		_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(blue, 0x99), K8_SHUFFLE_PERMUTED_BLUE_TO_BGR1),
			_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(green, 0x99), K8_SHUFFLE_PERMUTED_GREEN_TO_BGR1),
				_mm256_shuffle_epi8(_mm256_permute4x64_epi64(red, 0x99), K8_SHUFFLE_PERMUTED_RED_TO_BGR1)));
}

template<> SIMD_INLINE __m256i InterleaveBgr<2>(__m256i blue, __m256i green, __m256i red)
{
	return
		_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(blue, 0xEE), K8_SHUFFLE_PERMUTED_BLUE_TO_BGR2),
			_mm256_or_si256(_mm256_shuffle_epi8(_mm256_permute4x64_epi64(green, 0xEE), K8_SHUFFLE_PERMUTED_GREEN_TO_BGR2),
				_mm256_shuffle_epi8(_mm256_permute4x64_epi64(red, 0xEE), K8_SHUFFLE_PERMUTED_RED_TO_BGR2)));
}

SIMD_INLINE __m256i BgrToBlue(__m256i bgr[3])
{
	__m256i b0 = _mm256_shuffle_epi8(bgr[0], K8_SHUFFLE_BGR0_TO_BLUE);
	__m256i b2 = _mm256_shuffle_epi8(bgr[2], K8_SHUFFLE_BGR2_TO_BLUE);
	return
		_mm256_or_si256(_mm256_permute2x128_si256(b0, b2, 0x20),
			_mm256_or_si256(_mm256_shuffle_epi8(bgr[1], K8_SHUFFLE_BGR1_TO_BLUE),
				_mm256_permute2x128_si256(b0, b2, 0x31)));
}

SIMD_INLINE __m256i BgrToGreen(__m256i bgr[3])
{
	__m256i g0 = _mm256_shuffle_epi8(bgr[0], K8_SHUFFLE_BGR0_TO_GREEN);
	__m256i g2 = _mm256_shuffle_epi8(bgr[2], K8_SHUFFLE_BGR2_TO_GREEN);
	return
		_mm256_or_si256(_mm256_permute2x128_si256(g0, g2, 0x20),
			_mm256_or_si256(_mm256_shuffle_epi8(bgr[1], K8_SHUFFLE_BGR1_TO_GREEN),
				_mm256_permute2x128_si256(g0, g2, 0x31)));
}

SIMD_INLINE __m256i BgrToRed(__m256i bgr[3])
{
	__m256i r0 = _mm256_shuffle_epi8(bgr[0], K8_SHUFFLE_BGR0_TO_RED);
	__m256i r2 = _mm256_shuffle_epi8(bgr[2], K8_SHUFFLE_BGR2_TO_RED);
	return
		_mm256_or_si256(_mm256_permute2x128_si256(r0, r2, 0x20),
			_mm256_or_si256(_mm256_shuffle_epi8(bgr[1], K8_SHUFFLE_BGR1_TO_RED),
				_mm256_permute2x128_si256(r0, r2, 0x31)));
}

template <bool align> SIMD_INLINE __m256i LoadPermuted(const __m256i * p)
{
	return _mm256_permute4x64_epi64(Load<align>(p), 0xD8);
}


}

#endif /* USB_CAM_INCLUDE_USB_CAM_UTILITY_H_ */

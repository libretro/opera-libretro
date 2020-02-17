#ifndef LIBOPERA_SWI_HLE_0X5XXXX_H_INCLUDED
#define LIBOPERA_SWI_HLE_0X5XXXX_H_INCLUDED

#include "inline.h"

#include "opera_fixedpoint_math.h"

#include <stdint.h>

/* void MulVec3Mat33_F16(vec3f16 dest, vec3f16 vec, mat33f16 mat); */
static
INLINE
void
opera_swi_hle_0x50000(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  char     *ram;
  vec3f16  *dest;
  vec3f16  *vec;
  mat33f16 *mat;

  ram  = ram_;
  dest = (vec3f16*)&ram[r0_];
  vec  = (vec3f16*)&ram[r1_];
  mat  = (mat33f16*)&ram[r2_];

  MulVec3Mat33_F16(*dest,*vec,*mat);
}

/* void MulMat33Mat33_F16(mat33f16 dest, mat33f16 src1, mat33f16 src2); */
static
INLINE
void
opera_swi_hle_0x50001(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  uint8_t  *ram;
  mat33f16 *dest;
  mat33f16 *src1;
  mat33f16 *src2;

  ram  = ram_;
  dest = (mat33f16*)&ram[r0_];
  src1 = (mat33f16*)&ram[r1_];
  src2 = (mat33f16*)&ram[r2_];

  MulMat33Mat33_F16(*dest,*src1,*src2);
}

/* void MulManyVec3Mat33_F16(vec3f16 *dest, vec3f16 *src, mat33f16 mat, int32 count); */
static
INLINE
void
opera_swi_hle_0x50002(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  uint8_t  *ram;
  vec3f16  *dest;
  vec3f16  *src;
  mat33f16 *mat;
  int32_t   count;

  ram   = ram_;
  dest  = (vec3f16*)&ram[r0_];
  src   = (vec3f16*)&ram[r1_];
  mat   = (mat33f16*)&ram[r2_];
  count = (int32_t)r3_;

  MulManyVec3Mat33_F16(dest,src,*mat,count);
}

/* void MulObjectVec3Mat33_F16(void *objectlist[], ObjOffset1 *offsetstruct, int32 count); */
static
INLINE
void
opera_swi_hle_0x50003(void    *ram_,
                      uint8_t  r0_,
                      uint8_t  r1_,
                      uint8_t  r2_)
{

}

/* void MulObjectMat33_F16(void *objectlist[], ObjOffset2 *offsetstruct, mat33f16 mat, int32 count); */
static
INLINE
void
opera_swi_hle_0x50004(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{

}

/* void MulManyF16(frac16 *dest, frac16 *src1, frac16 *src2, int32 count); */
static
INLINE
void
opera_swi_hle_0x50005(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  uint8_t *ram;
  frac16  *dest;
  frac16  *src1;
  frac16  *src2;
  int32_t  count;

  ram   = ram_;
  dest  = (frac16*)&ram[r0_];
  src1  = (frac16*)&ram[r1_];
  src2  = (frac16*)&ram[r2_];
  count = (int32_t)r3_;

  MulManyF16(dest,src1,src2,count);
}

/* void MulScalerF16(frac16 *dest, frac16 *src, frac16 scaler, int32 count);  */
static
INLINE
void
opera_swi_hle_0x50006(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  char    *ram;
  frac16  *dest;
  frac16  *src;
  frac16   scaler;
  int32_t  count;

  ram    = ram_;
  dest   = (frac16*)&ram[r0_];
  src    = (frac16*)&ram[r1_];
  scaler = r2_;
  count  = (int32_t)r3_;

  MulScalerF16(dest,src,scaler,count);
}

/* void MulVec4Mat44_F16(vec4f16 dest, vec4f16 vec, mat44f16 mat);  */
static
INLINE
void
opera_swi_hle_0x50007(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  char     *ram;
  vec4f16  *dest;
  vec4f16  *vec;
  mat44f16 *mat;

  ram  = ram_;
  dest = (vec4f16*)&ram[r0_];
  vec  = (vec4f16*)&ram[r1_];
  mat  = (mat44f16*)&ram[r2_];

  MulVec4Mat44_F16(*dest,*vec,*mat);
}

/* void MulMat44Mat44_F16(mat44f16 dest, mat44f16 src1, mat44f16 src2);  */
static
INLINE
void
opera_swi_hle_0x50008(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  char     *ram;
  mat44f16 *dest;
  mat44f16 *src1;
  mat44f16 *src2;

  ram  = ram_;
  dest = (mat44f16*)&ram[r0_];
  src1 = (mat44f16*)&ram[r1_];
  src2 = (mat44f16*)&ram[r2_];

  MulMat44Mat44_F16(*dest,*src1,*src2);
}

/* void MulManyVec4Mat44_F16(vec4f16 *dest, vec4f16 *src, mat44f16 mat, int32 count);  */
static
INLINE
void
opera_swi_hle_0x50009(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  char     *ram;
  vec4f16  *dest;
  vec4f16  *src;
  mat44f16 *mat;
  int32_t   count;

  ram   = ram_;
  dest  = (vec4f16*)&ram[r0_];
  src   = (vec4f16*)&ram[r1_];
  mat   = (mat44f16*)&ram[r2_];
  count = (int32_t)r3_;

  MulManyVec4Mat44_F16(dest,src,*mat,count);
}

/* void MulObjectVec4Mat44_F16(void *objectlist[], ObjeOffset1 *offsetstruct, int32 count);  */
static
INLINE
void
opera_swi_hle_0x5000A(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{

}

/* void MulObjectMat44_F16(void *objectlist[], ObjOffset2 *offsetstruct, mat44f16 mat, int32 count);  */
static
INLINE
void
opera_swi_hle_0x5000B(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{

}

/* frac16 Dot3_F16(vec3f16 v1, vec3f16 v2); */
static
INLINE
uint32_t
opera_swi_hle_0x5000C(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_)
{
  char    *ram;
  vec3f16 *v1;
  vec3f16 *v2;

  ram = ram_;
  v1  = (vec3f16*)&ram[r0_];
  v2  = (vec3f16*)&ram[r1_];

  return (uint32_t)Dot3_F16(*v1,*v2);
}

/* frac16 Dot4_F16(vec4f16 v1, vec4f16 v2); */
static
INLINE
uint32_t
opera_swi_hle_0x5000D(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_)
{
  char    *ram;
  vec4f16 *v1;
  vec4f16 *v2;

  ram = ram_;
  v1  = (vec4f16*)&ram[r0_];
  v2  = (vec4f16*)&ram[r1_];

  return (uint32_t)Dot4_F16(*v1,*v2);
}

/* void Cross3_F16(vec3f16 dest, vec3f16 v1, vec3f16 v2); */
static
INLINE
void
opera_swi_hle_0x5000E(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  char    *ram;
  vec3f16 *dest;
  vec3f16 *v1;
  vec3f16 *v2;

  ram  = ram_;
  dest = (vec3f16*)&ram[r0_];
  v1   = (vec3f16*)&ram[r1_];
  v2   = (vec3f16*)&ram[r2_];

  Cross3_F16(*dest,*v1,*v2);
}

/* frac16 AbsVec3_F16(vec3f16 vec); */
static
INLINE
uint32_t
opera_swi_hle_0x5000F(void     *ram_,
                      uint32_t  r0_)
{
  char    *ram;
  vec3f16 *vec;

  ram = ram_;
  vec = (vec3f16*)&ram[r0_];

  return (uint32_t)AbsVec3_F16(*vec);
}

/* frac16 AbsVec4_F16(vec3f16 vec); */
static
INLINE
uint32_t
opera_swi_hle_0x50010(void     *ram_,
                      uint32_t  r0_)
{
  char    *ram;
  vec4f16 *vec;

  ram = ram_;
  vec = (vec4f16*)&ram[r0_];

  return (uint32_t)AbsVec4_F16(*vec);
}

/* void MulVec3Mat33DivZ_F16(vec3f16 dest, vec3f16 vec, mat33f16 mat, frac16 n); */
static
INLINE
void
opera_swi_hle_0x50011(void     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  char     *ram;
  vec3f16  *dest;
  vec3f16  *vec;
  mat33f16 *mat;
  frac16    n;

  ram  = ram_;
  dest = (vec3f16*)&ram[r0_];
  vec  = (vec3f16*)&ram[r1_];
  mat  = (mat33f16*)&ram[r2_];
  n    = (frac16)r3_;

  MulVec3Mat33DivZ_F16(*dest,*vec,*mat,n);
}

/* void MulManyVec3Mat33DivZ_F16(mmv3m33d *s);  */
static
INLINE
void
opera_swi_hle_0x50012(void     *ram_,
                      uint32_t  r0_)
{
  char     *ram;
  vec3f16  *dest;
  vec3f16  *src;
  mat33f16 *mat;
  frac16    n;
  uint32_t  count;

  ram = ram_;
  dest  = (vec3f16*)&ram[*(uint32_t*)&ram[r0_ + 0x00]];
  src   = (vec3f16*)&ram[*(uint32_t*)&ram[r0_ + 0x04]];
  mat   = (mat33f16*)&ram[*(uint32_t*)&ram[r0_ + 0x08]];
  n     = *(frac16*)&ram[r0_ + 0x0C];
  count = *(uint32_t*)&ram[r0_ + 0x10];

  MulManyVec3Mat33DivZ_F16(dest,src,mat,n,count);
}

#endif

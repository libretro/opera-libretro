#include "freedo_fixedpoint_math.h"

static
frac16
sqrt_frac16(frac16 x_)
{
  frac16 root;
  frac16 remHi;
  frac16 remLo;
  frac16 testDiv;
  frac16 count;

  root  = 0;
  remHi = 0;
  remLo = x_;
  count = 16;

  do
    {
      remHi = ((remHi << 16) | (remLo >> 16));
      remLo = (remLo << 16);
      testDiv = ((root << 1) + 1);
      if(remHi >= testDiv)
        {
          remHi -= testDiv;
          root++;
        }
    }
  while(count-- != 0);

  return root;
}

/* swi 0x50000 */
void
MulVec3Mat33_F16(vec3f16  dest_,
                 vec3f16  vec_,
                 mat33f16 mat_)
{
  vec3f16 tmp;

  tmp[0] = ((((int64_t)vec_[0] * (int64_t)mat_[0][0]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][0]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][0])) >> 16);
  tmp[1] = ((((int64_t)vec_[0] * (int64_t)mat_[0][1]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][1]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][1])) >> 16);
  tmp[2] = ((((int64_t)vec_[0] * (int64_t)mat_[0][2]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][2]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][2])) >> 16);

  dest_[0] = tmp[0];
  dest_[1] = tmp[1];
  dest_[2] = tmp[2];
}

/* swi 0x50001 */
void
MulMat33Mat33_F16(mat33f16 dest_,
                  mat33f16 src1_,
                  mat33f16 src2_)
{
  mat33f16 tmp;

  tmp[0][0] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][0])) >> 16);
  tmp[0][1] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][1])) >> 16);
  tmp[0][2] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][2])) >> 16);

  tmp[1][0] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][0])) >> 16);
  tmp[1][1] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][1])) >> 16);
  tmp[1][2] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][2])) >> 16);

  tmp[2][0] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][0])) >> 16);
  tmp[2][1] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][1])) >> 16);
  tmp[2][2] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][2])) >> 16);

  dest_[0][0] = tmp[0][0];
  dest_[0][1] = tmp[0][1];
  dest_[0][2] = tmp[0][2];

  dest_[1][0] = tmp[1][0];
  dest_[1][1] = tmp[1][1];
  dest_[1][2] = tmp[1][2];

  dest_[2][0] = tmp[2][0];
  dest_[2][1] = tmp[2][1];
  dest_[2][2] = tmp[2][2];
}

/* swi 0x50002 */
void
MulManyVec3Mat33_F16(vec3f16  *dest_,
                     vec3f16  *src_,
                     mat33f16  mat_,
                     int32_t   count_)
{
  vec3f16 tmp;

  for(int32_t i = 0; i < count_; i++)
    {
      tmp[0] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][0]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][0]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][0])) >> 16);
      tmp[1] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][1]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][1]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][1])) >> 16);
      tmp[2] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][2]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][2]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][2])) >> 16);

      dest_[i][0] = tmp[0];
      dest_[i][1] = tmp[1];
      dest_[i][2] = tmp[2];
    }
}

/* swi 0x50005 */
void
MulManyF16(frac16  *dest_,
           frac16  *src1_,
           frac16  *src2_,
           int32_t  count_)
{
  for(int32_t i = 0; i < count_; i++)
    {
      dest_[i] = (((int64_t)src1_[i] * (int64_t)src2_[i]) >> 16);
    }
}

/* swi 0x50006 */
void
MulScalerF16(frac16  *dest_,
             frac16  *src_,
             frac16   scaler_,
             int32_t  count_)
{
  for(int32_t i = 0; i < count_; i++)
    {
      dest_[i] = (((int64_t)src_[i] * (int64_t)scaler_) >> 16);
    }
}

/* swi 0x50007 */
void
MulVec4Mat44_F16(vec4f16  dest_,
                 vec4f16  vec_,
                 mat44f16 mat_)
{
  vec4f16 tmp;

  tmp[0] = ((((int64_t)vec_[0] * (int64_t)mat_[0][0]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][0]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][0]) +
             ((int64_t)vec_[3] * (int64_t)mat_[3][0])) >> 16);
  tmp[1] = ((((int64_t)vec_[0] * (int64_t)mat_[0][1]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][1]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][1]) +
             ((int64_t)vec_[3] * (int64_t)mat_[3][1])) >> 16);
  tmp[2] = ((((int64_t)vec_[0] * (int64_t)mat_[0][2]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][2]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][2]) +
             ((int64_t)vec_[3] * (int64_t)mat_[3][2])) >> 16);
  tmp[3] = ((((int64_t)vec_[0] * (int64_t)mat_[0][3]) +
             ((int64_t)vec_[1] * (int64_t)mat_[1][3]) +
             ((int64_t)vec_[2] * (int64_t)mat_[2][3]) +
             ((int64_t)vec_[3] * (int64_t)mat_[3][3])) >> 16);

  dest_[0] = tmp[0];
  dest_[1] = tmp[1];
  dest_[2] = tmp[2];
  dest_[3] = tmp[3];
}

/* swi 0x50008 */
void
MulMat44Mat44_F16(mat44f16 dest_,
                  mat44f16 src1_,
                  mat44f16 src2_)
{
  mat44f16 tmp;

  tmp[0][0] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][0]) +
                ((int64_t)src1_[0][3] * (int64_t)src2_[3][0])) >> 16);
  tmp[0][1] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][1]) +
                ((int64_t)src1_[0][3] * (int64_t)src2_[3][1])) >> 16);
  tmp[0][2] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][2]) +
                ((int64_t)src1_[0][3] * (int64_t)src2_[3][2])) >> 16);
  tmp[0][3] = ((((int64_t)src1_[0][0] * (int64_t)src2_[0][3]) +
                ((int64_t)src1_[0][1] * (int64_t)src2_[1][3]) +
                ((int64_t)src1_[0][2] * (int64_t)src2_[2][3]) +
                ((int64_t)src1_[0][3] * (int64_t)src2_[3][3])) >> 16);

  tmp[1][0] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][0]) +
                ((int64_t)src1_[1][3] * (int64_t)src2_[3][0])) >> 16);
  tmp[1][1] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][1]) +
                ((int64_t)src1_[1][3] * (int64_t)src2_[3][1])) >> 16);
  tmp[1][2] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][2]) +
                ((int64_t)src1_[1][3] * (int64_t)src2_[3][2])) >> 16);
  tmp[1][3] = ((((int64_t)src1_[1][0] * (int64_t)src2_[0][3]) +
                ((int64_t)src1_[1][1] * (int64_t)src2_[1][3]) +
                ((int64_t)src1_[1][2] * (int64_t)src2_[2][3]) +
                ((int64_t)src1_[1][3] * (int64_t)src2_[3][3])) >> 16);

  tmp[2][0] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][0]) +
                ((int64_t)src1_[2][3] * (int64_t)src2_[3][0])) >> 16);
  tmp[2][1] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][1]) +
                ((int64_t)src1_[2][3] * (int64_t)src2_[3][1])) >> 16);
  tmp[2][2] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][2]) +
                ((int64_t)src1_[2][3] * (int64_t)src2_[3][2])) >> 16);
  tmp[2][3] = ((((int64_t)src1_[2][0] * (int64_t)src2_[0][3]) +
                ((int64_t)src1_[2][1] * (int64_t)src2_[1][3]) +
                ((int64_t)src1_[2][2] * (int64_t)src2_[2][3]) +
                ((int64_t)src1_[2][3] * (int64_t)src2_[3][3])) >> 16);

  tmp[3][0] = ((((int64_t)src1_[3][0] * (int64_t)src2_[0][0]) +
                ((int64_t)src1_[3][1] * (int64_t)src2_[1][0]) +
                ((int64_t)src1_[3][2] * (int64_t)src2_[2][0]) +
                ((int64_t)src1_[3][3] * (int64_t)src2_[3][0])) >> 16);
  tmp[3][1] = ((((int64_t)src1_[3][0] * (int64_t)src2_[0][1]) +
                ((int64_t)src1_[3][1] * (int64_t)src2_[1][1]) +
                ((int64_t)src1_[3][2] * (int64_t)src2_[2][1]) +
                ((int64_t)src1_[3][3] * (int64_t)src2_[3][1])) >> 16);
  tmp[3][2] = ((((int64_t)src1_[3][0] * (int64_t)src2_[0][2]) +
                ((int64_t)src1_[3][1] * (int64_t)src2_[1][2]) +
                ((int64_t)src1_[3][2] * (int64_t)src2_[2][2]) +
                ((int64_t)src1_[3][3] * (int64_t)src2_[3][2])) >> 16);
  tmp[3][3] = ((((int64_t)src1_[3][0] * (int64_t)src2_[0][3]) +
                ((int64_t)src1_[3][1] * (int64_t)src2_[1][3]) +
                ((int64_t)src1_[3][2] * (int64_t)src2_[2][3]) +
                ((int64_t)src1_[3][3] * (int64_t)src2_[3][3])) >> 16);

  dest_[0][0] = tmp[0][0];
  dest_[0][1] = tmp[0][1];
  dest_[0][2] = tmp[0][2];
  dest_[0][3] = tmp[0][3];

  dest_[1][0] = tmp[1][0];
  dest_[1][1] = tmp[1][1];
  dest_[1][2] = tmp[1][2];
  dest_[1][3] = tmp[1][3];

  dest_[2][0] = tmp[2][0];
  dest_[2][1] = tmp[2][1];
  dest_[2][2] = tmp[2][2];
  dest_[2][3] = tmp[2][3];

  dest_[3][0] = tmp[3][0];
  dest_[3][1] = tmp[3][1];
  dest_[3][2] = tmp[3][2];
  dest_[3][3] = tmp[3][3];
}

/* swi 0x50009 */
void
MulManyVec4Mat44_F16(vec4f16  *dest_,
                     vec4f16  *src_,
                     mat44f16  mat_,
                     int32_t   count_)
{
  vec4f16 tmp;

  for(int32_t i = 0; i < count_; i++)
    {
      tmp[0] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][0]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][0]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][0]) +
                 ((int64_t)src_[i][3] * (int64_t)mat_[3][0])) >> 16);
      tmp[1] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][1]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][1]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][1]) +
                 ((int64_t)src_[i][3] * (int64_t)mat_[3][1])) >> 16);
      tmp[2] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][2]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][2]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][2]) +
                 ((int64_t)src_[i][3] * (int64_t)mat_[3][2])) >> 16);
      tmp[3] = ((((int64_t)src_[i][0] * (int64_t)mat_[0][3]) +
                 ((int64_t)src_[i][1] * (int64_t)mat_[1][3]) +
                 ((int64_t)src_[i][2] * (int64_t)mat_[2][3]) +
                 ((int64_t)src_[i][3] * (int64_t)mat_[3][3])) >> 16);

      dest_[i][0] = tmp[0];
      dest_[i][1] = tmp[1];
      dest_[i][2] = tmp[2];
      dest_[i][3] = tmp[3];
    }
}

/* swi 0x5000A */
void
MulObjectVec4Mat44_F16(void       *objectlist[],
                       ObjOffset1 *offsetstruct,
                       int32_t     count)
{

}

/* swi 0x5000B */
void
MulObjectMat44_F16(void       *objectlist_[],
                   ObjOffset2 *offsetstruct_,
                   mat44f16    mat_,
                   int32_t     count_)
{

}

/* swi 0x5000C */
frac16
Dot3_F16(vec3f16 v1_,
         vec3f16 v2_)
{
  frac16 rv;

  rv = ((((int64_t)v1_[0] * (int64_t)v2_[0]) +
         ((int64_t)v1_[1] * (int64_t)v2_[1]) +
         ((int64_t)v1_[2] * (int64_t)v2_[2])) >> 16);

  return rv;
}

/* swi 0x5000D */
frac16
Dot4_F16(vec4f16 v1_,
         vec4f16 v2_)
{
  frac16 rv;

  rv = ((((int64_t)v1_[0] * (int64_t)v2_[0]) +
         ((int64_t)v1_[1] * (int64_t)v2_[1]) +
         ((int64_t)v1_[2] * (int64_t)v2_[2]) +
         ((int64_t)v1_[3] * (int64_t)v2_[3])) >> 16);

  return rv;
}

/* swi 0x5000E */
void
Cross3_F16(vec3f16 dest_,
           vec3f16 v1_,
           vec3f16 v2_)
{
  vec3f16 tmp;

  tmp[0] = ((((int64_t)v1_[1] * (int64_t)v2_[2]) - ((int64_t)v1_[2] * (int64_t)v2_[1])) >> 16);
  tmp[1] = ((((int64_t)v1_[2] * (int64_t)v2_[0]) - ((int64_t)v1_[0] * (int64_t)v2_[2])) >> 16);
  tmp[2] = ((((int64_t)v1_[0] * (int64_t)v2_[1]) - ((int64_t)v1_[1] * (int64_t)v2_[0])) >> 16);

  dest_[0] = tmp[0];
  dest_[1] = tmp[1];
  dest_[2] = tmp[2];
}

/* swi 0x5000F */
frac16
AbsVec3_F16(vec3f16 vec_)
{
  frac16 rv;

  rv = ((((int64_t)vec_[0] * (int64_t)vec_[0]) +
         ((int64_t)vec_[1] * (int64_t)vec_[1]) +
         ((int64_t)vec_[2] * (int64_t)vec_[2])) >> 16);

  return sqrt_frac16(rv);
}

/* swi 0x50010 */
frac16
AbsVec4_F16(vec4f16 vec_)
{
  frac16 rv;

  rv = ((((int64_t)vec_[0] * (int64_t)vec_[0]) +
         ((int64_t)vec_[1] * (int64_t)vec_[1]) +
         ((int64_t)vec_[2] * (int64_t)vec_[2]) +
         ((int64_t)vec_[3] * (int64_t)vec_[3])) >> 16);

  return sqrt_frac16(rv);
}

/* swi 0x50011 */
void
MulVec3Mat33DivZ_F16(vec3f16  dest_,
                     vec3f16  vec_,
                     mat33f16 mat_,
                     frac16   n_)
{
  int64_t mul;

  MulVec3Mat33_F16(dest_,vec_,mat_);

  if(dest_[2] != 0)
    {
      int64_t mul;

      mul = (((int64_t)n_ << 16) / (int64_t)dest_[2]);

      dest_[0] = (((int64_t)dest_[0] * mul) >> 16);
      dest_[1] = (((int64_t)dest_[1] * mul) >> 16);
    }
}

/* swi 0x50012 */
void
MulManyVec3Mat33DivZ_F16(vec3f16  *dest_,
                         vec3f16  *src_,
                         mat33f16 *mat_,
                         frac16    n_,
                         uint32_t  count_)
{
  for(uint32_t i = 0; i < count_; i++)
    MulVec3Mat33DivZ_F16(dest_[i],src_[i],*mat_,n_);
}

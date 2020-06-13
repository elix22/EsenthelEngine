/******************************************************************************/
#include "stdafx.h"
namespace EE{
/******************************************************************************

   If Material has base_0 && base_2:
      Layout 2
      base_0: RGB, Glow
      base_2: Smooth, Reflect, Bump, Alpha
   Elif Material has base_0
      Layout 1
      base_0: RGBA
   Else
      Layout 0
      empty
   Endif

   base_1 always: NrmX, NrmY

   Detail tex always: NrmX, NrmY, Col, Smooth

   When changing the above to a different order, then look for "#MaterialTextureLayout" text in Engine/Editor to update the codes.

   https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/

   Water Reflectivity, Index of Refraction (IOR) of Water is 1.33:
      F(0)=Sqr(1.33-1)/Sqr(1.33+1)=0.020059312=~0.02 (2%)
   Reflectivity:
      Ice              0.018
      Water            0.02
      Eye              0.025
      Skin             0.028, 0.033
      Plant,Leaf,Grass 0.035
      Rock, Bark       0.039
      Glass            0.04
      Plastic          0.04 .. 0.05
      Fabric	        0.04 .. 0.056
      Sand             0.046
      Hair             0.047
      Asphalt          0.06
      Ruby             0.077271957
      Crystal          0.111111111
      Diamond          0.171968833

   Metals Linear Specular/Color:
                  R           G           B
      Iron        0.563       0.579       0.579
      Silver      0.971519    0.959915    0.915324
      Aluminium   0.913183    0.921494    0.924524
      Gold        1           0.765557    0.336057
      Copper      0.955008    0.637427    0.538163
      Chromium    0.549585    0.556114    0.554256
      Nickel      0.659777    0.608679    0.525649
      Titanium    0.541931    0.496791    0.449419
      Cobalt      0.662124    0.654864    0.633732
      Platinum    0.672411    0.637331    0.585456

/******************************************************************************/
enum MTRL_TEX_LAYOUT : Byte
{
   MTL_NONE,
   MTL_RGBA,
   MTL_RGB_GLOW$NRM$SMOOTH_REFLECT_BUMP_ALPHA,
};
#define CC4_MTRL CC4('M','T','R','L')

#define BUMP_DEFAULT_TEX 0 // 0..255, normally this should be 128, but 0 will allow to use BC5 (for Mtrl.base_2 tex if there's no Alpha) and always set Material.bump=0 when bump is not used #MaterialTextureLayout
#define BUMP_DEFAULT_PAR 0.03f

// #MaterialTextureLayout
// base_0
#define    GLOW_CHANNEL 3
// base_1
#define    NRMX_CHANNEL 0
#define    NRMY_CHANNEL 1
// base_2
#define  SMOOTH_CHANNEL 0
#define REFLECT_CHANNEL 1
#define    BUMP_CHANNEL 2
#define   ALPHA_CHANNEL 3
/******************************************************************************/
static Int Compare(C UniqueMultiMaterialKey &a, C UniqueMultiMaterialKey &b)
{
   if(a.m[0]<b.m[0])return -1; if(a.m[0]>b.m[0])return +1;
   if(a.m[1]<b.m[1])return -1; if(a.m[1]>b.m[1])return +1;
   if(a.m[2]<b.m[2])return -1; if(a.m[2]>b.m[2])return +1;
   if(a.m[3]<b.m[3])return -1; if(a.m[3]>b.m[3])return +1;
                                                return  0;
}
/******************************************************************************/
DEFINE_CACHE(Material, Materials, MaterialPtr, "Material");
Material               MaterialDefault,
                       MaterialDefaultNoCull;
const Material        *MaterialLast,
                      *MaterialLast4[4]; // can't merge with 'MaterialLast' because that sets 'Sh.Material' but this sets 'Sh.MultiMaterial'
MaterialPtr            MaterialNull;
ThreadSafeMap<UniqueMultiMaterialKey, UniqueMultiMaterialData> UniqueMultiMaterialMap(Compare);
/******************************************************************************/
Vec4 MaterialParams::colorS(               )C {return        LinearToSRGB(color_l) ;}
void MaterialParams::colorS(C Vec4 &color_s)  {return colorL(SRGBToLinear(color_s));}
/******************************************************************************/
Material::Material()
{
   color_l.set(1, 1, 1, 1);
   ambient.set(0, 0, 0);
   smooth   =0;
   reflect  =MATERIAL_REFLECT;
   glow     =0;
   normal   =0;
   bump     =0;
   det_power=0.3f;
   det_scale=4;
   tex_scale=1.0f;

   cull     =true;
   technique=MTECH_DEFAULT;

   clear();
   validate();
}
Material::~Material()
{
#if !SYNC_LOCK_SAFE // if 'SyncLock' is not safe then crash may occur when trying to lock, to prevent that, check if we have any elements (this means cache was already initialized)
   if(UniqueMultiMaterialMap.elms())
#endif
   {
      UniqueMultiMaterialMap.lock  (); REPA(UniqueMultiMaterialMap){C UniqueMultiMaterialKey &key=UniqueMultiMaterialMap.lockedKey(i); if(key.m[0]==this || key.m[1]==this || key.m[2]==this || key.m[3]==this)UniqueMultiMaterialMap.lockedRemove(i);}
      UniqueMultiMaterialMap.unlock();
   }
}
/******************************************************************************/
static Bool HasAlphaTest(MATERIAL_TECHNIQUE technique)
{
   switch(technique)
   {
      case MTECH_ALPHA_TEST            :
      case MTECH_GRASS                 :
      case MTECH_GRASS_3D              :
      case MTECH_LEAF_2D               :
      case MTECH_LEAF                  :
      case MTECH_TEST_BLEND_LIGHT      :
      case MTECH_TEST_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_LEAF :
         return true;

      default: return false;
   }
}
Bool Material::hasAlpha()C
{
   switch(technique)
   {
      case MTECH_ALPHA_TEST:
      case MTECH_GRASS:
      case MTECH_GRASS_3D:
      case MTECH_LEAF_2D:
      case MTECH_LEAF:
      case MTECH_BLEND:
      case MTECH_BLEND_LIGHT:
      case MTECH_BLEND_LIGHT_GRASS:
      case MTECH_BLEND_LIGHT_LEAF:
      case MTECH_TEST_BLEND_LIGHT:
      case MTECH_TEST_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_LEAF:
         return true;

      default: return false;
   }
}
Bool Material::hasAlphaBlend()C
{
   switch(technique)
   {
      case MTECH_BLEND:
      case MTECH_BLEND_LIGHT:
      case MTECH_BLEND_LIGHT_GRASS:
      case MTECH_BLEND_LIGHT_LEAF:
      case MTECH_TEST_BLEND_LIGHT:
      case MTECH_TEST_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_LEAF:
         return true;

      default: return false;
   }
}
Bool Material::hasAlphaBlendLight()C
{
   switch(technique)
   {
      case MTECH_BLEND_LIGHT:
      case MTECH_BLEND_LIGHT_GRASS:
      case MTECH_BLEND_LIGHT_LEAF:
      case MTECH_TEST_BLEND_LIGHT:
      case MTECH_TEST_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_LEAF:
         return true;

      default: return false;
   }
}
Bool Material::hasGrass()C
{
   switch(technique)
   {
      case MTECH_GRASS:
      case MTECH_GRASS_3D:
      case MTECH_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_GRASS:
         return true;

      default: return false;
   }
}
Bool Material::hasGrass2D()C
{
   switch(technique)
   {
      case MTECH_GRASS:
      case MTECH_BLEND_LIGHT_GRASS:
      case MTECH_TEST_BLEND_LIGHT_GRASS:
         return true;

      default: return false;
   }
}
Bool Material::hasGrass3D()C
{
   switch(technique)
   {
      case MTECH_GRASS_3D:
         return true;

      default: return false;
   }
}
Bool Material::hasLeaf()C
{
   switch(technique)
   {
      case MTECH_LEAF_2D:
      case MTECH_LEAF:
      case MTECH_BLEND_LIGHT_LEAF:
      case MTECH_TEST_BLEND_LIGHT_LEAF:
         return true;

      default: return false;
   }
}
Bool Material::hasLeaf2D()C
{
   switch(technique)
   {
      case MTECH_LEAF_2D:
         return true;

      default: return false;
   }
}
Bool Material::hasLeaf3D()C
{
   switch(technique)
   {
      case MTECH_LEAF:
      case MTECH_BLEND_LIGHT_LEAF:
      case MTECH_TEST_BLEND_LIGHT_LEAF:
         return true;

      default: return false;
   }
}
/******************************************************************************/
Bool Material::needTanBin()C
{
   // #MaterialTextureLayout
   return (base_1     && normal   >EPS_COL8         )  // normal        is in base_1
       || (base_2     && bump     >EPS_MATERIAL_BUMP)  // bump          is in base_2
       || (detail_map && det_power>EPS_COL8         ); // normal detail is in DetailMap
}
/******************************************************************************/
Material& Material::reset   () {T=MaterialDefault; return validate();}
Material& Material::validate() // #MaterialTextureLayout
{
                      if(this==MaterialLast    )MaterialLast    =null;
   REPA(MaterialLast4)if(this==MaterialLast4[i])MaterialLast4[i]=null;

  _has_alpha_test=HasAlphaTest(technique); // !! set this first, because codes below rely on this !!
  _depth_write   =!(hasAlphaBlend() && !hasAlphaTest ());
//_coverage      = (hasAlphaTest () && !hasAlphaBlend());
  _alpha_factor.set(0, 0, 0, FltToByte(T.glow));

   // set multi
   {
     _multi.color    =(LINEAR_GAMMA ? colorL() : colorS());
     _multi.glow     =glow; // 'glow_mul', 'glow_add' not needed because Glow is stored in 'base_0' (which can be either Alpha or Glow, however Alpha is never used for multi-materials, so it's always 1.0)
     _multi.tex_scale=tex_scale;
     _multi.det_scale=det_scale;

      // normal map
      if(base_1)
      {
        _multi.normal=normal;
      }else
      {
        _multi.normal=0;
      }

      // base2
      if(base_2)
      {
        _multi.base2_mul.c[SMOOTH_CHANNEL]=smooth;
        _multi.base2_add.c[SMOOTH_CHANNEL]=0;

        _multi.base2_mul.c[REFLECT_CHANNEL]=reflect;
        _multi.base2_add.c[REFLECT_CHANNEL]=0;

        _multi.bump=bump;
      }else
      {
        _multi.base2_mul=0;
        _multi.base2_add.c[ SMOOTH_CHANNEL]=smooth;
        _multi.base2_add.c[REFLECT_CHANNEL]=reflect;

        _multi.bump=0;
      }

      if(detail_map)
      {
        _multi.det_mul=  det_power;
        _multi.det_add=  det_power*-0.5f; // used for normal      : (tex-0.5)*det_power     == tex*det_power - det_power*0.5                                    == tex*det_mul + det_add
        _multi.det_inv=1-det_power      ; // used for color,smooth: Lerp(1, tex, det_power) == 1*(1-det_power) + tex*det_power == tex*det_power + (1-det_power) == tex*det_mul + det_inv
      }else
      {
        _multi.det_mul=0;
        _multi.det_add=0;
        _multi.det_inv=1;
      }
     _multi.macro=(macro_map!=null);
   }
   return T;
}
/******************************************************************************
Bool Material::convertAlphaTest(Bool blend)
{
   if(blend)
   {
      if(technique==MTECH_ALPHA_TEST){technique=MTECH_TEST_BLEND_LIGHT      ; color.w=1; validate(); return true;}
      if(technique==MTECH_GRASS     ){technique=MTECH_TEST_BLEND_LIGHT_GRASS; color.w=1; validate(); return true;}
      if(technique==MTECH_LEAF      ){technique=MTECH_TEST_BLEND_LIGHT_LEAF ; color.w=1; validate(); return true;}
   }else
   {
      if(technique==MTECH_TEST_BLEND_LIGHT      ){technique=MTECH_ALPHA_TEST; color.w=0.5f; validate(); return true;}
      if(technique==MTECH_TEST_BLEND_LIGHT_GRASS){technique=MTECH_GRASS     ; color.w=0.5f; validate(); return true;}
      if(technique==MTECH_TEST_BLEND_LIGHT_LEAF ){technique=MTECH_LEAF      ; color.w=0.5f; validate(); return true;}
   }
   return false;
}
/******************************************************************************/
void Material::setSolid()C
{
   if(MaterialLast!=this)
   {
      MaterialLast    =this;
      MaterialLast4[0]=null; // because they use the same shader images

      if(_alpha_factor.a)Renderer._has_glow=true;
      Sh.Col[0]  ->set(    base_0());
      Sh.Nrm[0]  ->set(    base_1());
      Sh.Ext[0]  ->set(    base_2());
      Sh.Det[0]  ->set(detail_map());
      Sh.Mac[0]  ->set( macro_map());
      Sh.Lum     ->set( light_map());
      Sh.Material->set<MaterialParams>(T);
   #if !LINEAR_GAMMA
      Renderer.material_color_l->set(colorS());
   #endif
   }
}
void Material::setAmbient()C
{
   if(MaterialLast!=this)
   {
      MaterialLast=this;
    //MaterialLast4[0]=null; not needed since multi materials not rendered in ambient mode

      // textures needed for alpha-test
      Sh.Col[0]  ->set(base_0   ());
      Sh.Ext[0]  ->set(base_2   ());
      Sh.Lum     ->set(light_map());
      Sh.Material->set<MaterialParams>(T); // params needed for alpha-test and ambient
   #if !LINEAR_GAMMA
      Renderer.material_color_l->set(colorS());
   #endif
   }
}
void Material::setBlend()C
{
   if(MaterialLast!=this)
   {
      MaterialLast=this;
    //MaterialLast4[0]=null; not needed since multi materials not rendered in blend mode

      D.alphaFactor(_alpha_factor); if(_alpha_factor.a)Renderer._has_glow=true;

      Sh.Col[0]  ->set(    base_0());
      Sh.Nrm[0]  ->set(    base_1());
      Sh.Ext[0]  ->set(    base_2());
      Sh.Det[0]  ->set(detail_map());
      Sh.Mac[0]  ->set( macro_map());
      Sh.Lum     ->set( light_map());
      Sh.Material->set<MaterialParams>(T);
   #if !LINEAR_GAMMA
      Renderer.material_color_l->set(colorS());
   #endif
   }
}
void Material::setBlendForce()C
{
   if(_alpha_factor.a // if has glow in material settings
   && base_2 && base_0) // and on texture channel (glow is in Base0 but only if Base2 is present #MaterialTextureLayout)
   { // then it means we need to disable it for forced blend, which operates on alpha instead of glow
      if(MaterialLast==this)MaterialLast=null;
      D.alphaFactor(TRANSPARENT);
   }else
   {
      if(MaterialLast==this)return;
         MaterialLast= this;

      D.alphaFactor(_alpha_factor); if(_alpha_factor.a)Renderer._has_glow=true;
   }

   Sh.Col[0]  ->set(    base_0());
   Sh.Nrm[0]  ->set(    base_1());
   Sh.Ext[0]  ->set(    base_2());
   Sh.Det[0]  ->set(detail_map());
   Sh.Mac[0]  ->set( macro_map());
   Sh.Lum     ->set( light_map());
   Sh.Material->set<MaterialParams>(T);
#if !LINEAR_GAMMA
   Renderer.material_color_l->set(colorS());
#endif
}
void Material::setOutline()C
{
   if(MaterialLast!=this)
   {
      MaterialLast=this;
    //MaterialLast4[0]=null; not needed since multi materials not rendered in outline mode
      // textures needed for alpha-test
      Sh.Col[0]->set(base_0());
      Sh.Ext[0]->set(base_2());
      Renderer.material_color_l->set(LINEAR_GAMMA ? colorL() : colorS()); // only Material Color is used for potential alpha-testing
   }
}
void Material::setBehind()C
{
   if(MaterialLast!=this)
   {
      MaterialLast=this;
    //MaterialLast4[0]=null; not needed since multi materials not rendered in behind mode
      // textures needed for alpha-test
      Sh.Col[0]->set(base_0());
      Sh.Ext[0]->set(base_2());
      Renderer.material_color_l->set(LINEAR_GAMMA ? colorL() : colorS()); // only Material Color is used
   }
}
void Material::setShadow()C
{
   if(hasAlphaTest() && MaterialLast!=this) // this shader needs params/textures only for alpha test (if used)
   {
      MaterialLast=this;
    //MaterialLast4[0]=null; not needed since multi materials don't have alpha test and don't need to set values in shadow mode
      // textures needed for alpha-test
      Sh.Col[0]->set(base_0());
      Sh.Ext[0]->set(base_2());
      Renderer.material_color_l->set(LINEAR_GAMMA ? colorL() : colorS()); // only Material Color is used
   }
}
void Material::setMulti(Int i)C
{
   DEBUG_RANGE_ASSERT(i, Sh.MultiMaterial);
   if(MaterialLast4[i]!=this)
   {
            MaterialLast4[i]=this;
      if(!i)MaterialLast    =null; // because they use the same shader images

      if(_alpha_factor.a)Renderer._has_glow=true;

      Sh.Col          [i]->set(  base_0  ());
      Sh.Nrm          [i]->set(  base_1  ());
      Sh.Ext          [i]->set(  base_2  ());
      Sh.Det          [i]->set(detail_map());
      Sh.Mac          [i]->set( macro_map());
      Sh.MultiMaterial[i]->set(_multi      );
   }
}
void Material::setAuto()C
{
   switch(Renderer())
   {
      case RM_PREPARE :
      case RM_SOLID   :
      case RM_SOLID_M : setSolid(); break;

      case RM_AMBIENT : setAmbient(); break;

      case RM_FUR     :
      case RM_CLOUD   :
      case RM_BLEND   :
      case RM_PALETTE :
      case RM_PALETTE1:
      case RM_OVERLAY : setBlend(); break;

      case RM_BEHIND  : setBehind(); break;

      case RM_OUTLINE : setOutline(); break;

      case RM_EARLY_Z :
      case RM_SHADOW  : setShadow(); break;
   }
}
/******************************************************************************/
void Material::_adjustParams(UInt old_base_tex, UInt new_base_tex)
{
   UInt changed=(old_base_tex^new_base_tex);
   if(changed&BT_BUMP)
   {
      if(!(new_base_tex&BT_BUMP))bump=0;else
      if(bump<=EPS_MATERIAL_BUMP)bump=BUMP_DEFAULT_PAR;
   }
   if(changed&(BT_BUMP|BT_NORMAL))
   {
      if(!(new_base_tex&BT_BUMP) && !(new_base_tex&BT_NORMAL))normal=0;else
      if(                                    normal<=EPS_COL8)normal=1;
   }

   if(changed&BT_SMOOTH)
      if(!(new_base_tex&BT_SMOOTH))smooth=0;else
      if(smooth<=EPS_COL8         )smooth=1;

   if(changed&BT_REFLECT)
      if(!(new_base_tex&BT_REFLECT)        )reflect=MATERIAL_REFLECT;else
      if(reflect<=MATERIAL_REFLECT+EPS_COL8)reflect=1;

   if(changed&BT_GLOW)
      if(!(new_base_tex&BT_GLOW))glow=0;else
      if(glow<=EPS_COL8         )glow=1;

   if(changed&BT_ALPHA)
   {
      if(new_base_tex&BT_ALPHA)
      {
         if(!hasAlphaBlend() && color_l.w>=1-EPS_COL8)color_l.w=0.5f;
         if(!hasAlpha     ()                         )technique=MTECH_ALPHA_TEST;
      }else
      {
         if(hasAlpha())technique=MTECH_DEFAULT; // disable alpha technique if alpha map is not available
      }
   }

   validate();
}
/******************************************************************************/
Bool Material::saveData(File &f, CChar *path)C
{
   f.putMulti(Byte(10), cull, technique)<<SCAST(C MaterialParams, T); // version

   // textures
   f.putStr(    base_0.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!
   f.putStr(    base_1.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!
   f.putStr(    base_2.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!
   f.putStr(detail_map.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!
   f.putStr( macro_map.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!
   f.putStr( light_map.name(path)); // !! can't use 'id' because textures are stored in "Tex/" folder, so there's no point in using 'putAsset' !!

   return f.ok();
}
Bool Material::loadData(File &f, CChar *path)
{
   MaterialParams &mp=T; Char temp[MAX_LONG_PATH]; Flt sss;
   switch(f.decUIntV())
   {
      case 10:
      {
         f.getMulti(cull, technique)>>mp;
         f.getStr(temp);     base_0.require(temp, path);
         f.getStr(temp);     base_1.require(temp, path);
         f.getStr(temp);     base_2.require(temp, path);
         f.getStr(temp); detail_map.require(temp, path);
         f.getStr(temp);  macro_map.require(temp, path);
         f.getStr(temp);  light_map.require(temp, path);
      }break;

      case 9:
      {
         f.getMulti(cull, technique)>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect; colorS(color_l);
         f.getStr(temp);     base_0.require(temp, path);
         f.getStr(temp);     base_1.require(temp, path);
                             base_2=null;
         f.getStr(temp); detail_map.require(temp, path);
         f.getStr(temp);  macro_map.require(temp, path);
         f.getStr(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f.getStr(temp);  light_map.require(temp, path);
         f.getStr(temp);
         f.getStr(temp);
      }break;

      case 8:
      {
         f.getMulti(cull, technique)>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect; colorS(color_l);
         f._getStr1(temp);     base_0.require(temp, path);
         f._getStr1(temp);     base_1.require(temp, path);
                               base_2=null;
         f._getStr1(temp); detail_map.require(temp, path);
         f._getStr1(temp);  macro_map.require(temp, path);
         f._getStr1(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f._getStr1(temp);  light_map.require(temp, path);
         f._getStr1(temp);
         f._getStr1(temp);
      }break;

      case 7:
      {
         f>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect>>cull>>technique; colorS(color_l);
         f._getStr(temp);     base_0.require(temp, path);
         f._getStr(temp);     base_1.require(temp, path);
                              base_2=null;
         f._getStr(temp); detail_map.require(temp, path);
         f._getStr(temp);  macro_map.require(temp, path);
         f._getStr(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f._getStr(temp);  light_map.require(temp, path);
         f._getStr(temp);
         f._getStr(temp);
      }break;

      case 6:
      {
         f>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect>>cull>>technique; colorS(color_l);
         f._getStr(temp);     base_0.require(temp, path);
         f._getStr(temp);     base_1.require(temp, path);
                              base_2=null;
         f._getStr(temp); detail_map.require(temp, path);
         f._getStr(temp);  macro_map.require(temp, path);
         f._getStr(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f._getStr(temp);  light_map.require(temp, path);
         f._getStr8();
      }break;

      case 5:
      {
         f>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect>>cull>>technique; colorS(color_l);
         f._getStr(temp);     base_0.require(temp, path);
         f._getStr(temp);     base_1.require(temp, path);
                              base_2=null;
         f._getStr(temp); detail_map.require(temp, path);
         f._getStr(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f._getStr(temp);  light_map.require(temp, path);
                           macro_map=null;
         f._getStr8();
      }break;

      case 4:
      {
         f>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect>>cull>>technique; colorS(color_l);
         f._getStr(temp);     base_0.require(temp, path);
         f._getStr(temp);     base_1.require(temp, path);
                              base_2=null;
         f._getStr(temp); detail_map.require(temp, path);
         f._getStr(temp); if(!Is(temp))reflect=MATERIAL_REFLECT;
         f._getStr(temp);  light_map.require(temp, path);
                           macro_map=null;
      }break;

      case 3:
      {
         f>>color_l>>ambient>>smooth>>sss>>glow>>normal>>bump>>det_scale>>det_power>>reflect>>cull>>technique; tex_scale=1; colorS(color_l);
             base_0.require(f._getStr8(), path);
             base_1.require(f._getStr8(), path);
             base_2=null;
         detail_map.require(f._getStr8(), path);
                  Str8 temp=f._getStr8(); if(!Is(temp))reflect=MATERIAL_REFLECT;
          light_map.require(f._getStr8(), path);
          macro_map=null;
      }break;

      case 2:
      {
         f.skip(1);
         f>>color_l>>smooth>>sss>>glow>>normal>>bump>>det_scale>>det_power>>reflect>>cull>>technique; ambient=0; tex_scale=1; colorS(color_l);
         if(technique==MTECH_FUR){det_power=color_l.w; color_l.w=1;}
             base_0.require(f._getStr8(), path);
             base_1.require(f._getStr8(), path);
             base_2=null;
         detail_map.require(f._getStr8(), path);
                  Str8 temp=f._getStr8(); if(!Is(temp))reflect=MATERIAL_REFLECT;
          light_map=null;
          macro_map=null;
      }break;

      case 1:
      {
         f.skip(1);
         f>>color_l>>smooth>>glow>>normal>>bump>>det_scale>>det_power>>reflect>>cull>>technique; sss=0; ambient=0; tex_scale=1; colorS(color_l);
         if(technique==MTECH_FUR){det_power=color_l.w; color_l.w=1;}
             base_0.require(f._getStr8(), path);
             base_1.require(f._getStr8(), path);
             base_2=null;
         detail_map.require(f._getStr8(), path);
                  Str8 temp=f._getStr8(); if(!Is(temp))reflect=MATERIAL_REFLECT;
          light_map=null;
          macro_map=null;
      }break;

      case 0:
      {
         f.skip(1);
         f>>color_l>>smooth>>glow>>normal>>bump>>det_scale>>det_power>>reflect>>cull; sss=0; ambient=0; tex_scale=1; colorS(color_l);
         switch(f.getByte())
         {
            default: technique=MTECH_DEFAULT   ; break;
            case 1 : technique=MTECH_ALPHA_TEST; break;
            case 4 : technique=MTECH_FUR       ; break;
            case 5 : technique=MTECH_GRASS     ; break;
         }
         if(technique==MTECH_FUR){det_power=color_l.w; color_l.w=1;}
         Char8 temp[80];
         f>>temp;     base_0.require(temp, path);
         f>>temp;     base_1.require(temp, path);
                      base_2=null;
         f>>temp; detail_map.require(temp, path);
         f>>temp; if(!Is(temp))reflect=MATERIAL_REFLECT;
                   light_map=null;
                   macro_map=null;
      }break;

      default: goto error;
   }
   if(f.ok()){validate(); return true;}
error:
   reset(); return false;
}
/******************************************************************************/
Bool Material::save(File &f, CChar *path)C
{
   f.putUInt(CC4_MTRL);
   return saveData(f, path);
}
Bool Material::load(File &f, CChar *path)
{
   if(f.getUInt()==CC4_MTRL)return loadData(f, path);
   reset(); return false;
}

Bool Material::save(C Str &name)C
{
   File f; if(f.writeTry(name)){if(save(f, _GetPath(name)) && f.flush())return true; f.del(); FDelFile(name);}
   return false;
}
Bool Material::load(C Str &name)
{
   File f; if(f.readTry(name))return load(f, _GetPath(name));
   reset(); return false;
}
/******************************************************************************/
void MaterialClear() // must be called: after changing 'Renderer.mode', after changing 'D.alphaFactor', or material parameters/textures
{
         MaterialLast  =null;
   REPAO(MaterialLast4)=null;
}
/******************************************************************************/
static Int ImgW(C ImageSource &src, C Image *img) {return (!img->is()) ? 0 : (src.size.x>0) ? src.size.x : img->w();}
static Int ImgH(C ImageSource &src, C Image *img) {return (!img->is()) ? 0 : (src.size.y>0) ? src.size.y : img->h();}

static FILTER_TYPE Filter(Int filter) {return InRange(filter, FILTER_NUM) ? FILTER_TYPE(filter) : FILTER_BEST;}

UInt CreateBaseTextures(Image &base_0, Image &base_1, Image &base_2, C ImageSource &color, C ImageSource &alpha, C ImageSource &bump, C ImageSource &normal, C ImageSource &smooth, C ImageSource &reflect, C ImageSource &glow, Bool resize_to_pow2, Bool flip_normal_y)
{
   // #MaterialTextureLayout
   UInt  ret=0;
   Image dest_0, dest_1, dest_2;
   {
      Image   color_temp; C Image *  color_src=&  color.image; if(  color_src->compressed())if(  color_src->copyTry(  color_temp, -1, -1, -1, IMAGE_R8G8B8A8_SRGB, IMAGE_SOFT, 1))  color_src=&  color_temp;else goto error;
      Image   alpha_temp; C Image *  alpha_src=&  alpha.image; if(  alpha_src->compressed())if(  alpha_src->copyTry(  alpha_temp, -1, -1, -1, IMAGE_L8A8         , IMAGE_SOFT, 1))  alpha_src=&  alpha_temp;else goto error;
      Image    bump_temp; C Image *   bump_src=&   bump.image; if(   bump_src->compressed())if(   bump_src->copyTry(   bump_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))   bump_src=&   bump_temp;else goto error;
      Image  normal_temp; C Image * normal_src=& normal.image; if( normal_src->compressed())if( normal_src->copyTry( normal_temp, -1, -1, -1, IMAGE_R8G8         , IMAGE_SOFT, 1)) normal_src=& normal_temp;else goto error;
      Image  smooth_temp; C Image * smooth_src=& smooth.image; if( smooth_src->compressed())if( smooth_src->copyTry( smooth_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1)) smooth_src=& smooth_temp;else goto error;
      Image reflect_temp; C Image *reflect_src=&reflect.image; if(reflect_src->compressed())if(reflect_src->copyTry(reflect_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))reflect_src=&reflect_temp;else goto error;
      Image    glow_temp; C Image *   glow_src=&   glow.image; if(   glow_src->compressed())if(   glow_src->copyTry(   glow_temp, -1, -1, -1, IMAGE_L8A8         , IMAGE_SOFT, 1))   glow_src=&   glow_temp;else goto error;

      // set alpha
      Bool alpha_from_col=false;
      if(  alpha_src->is()) // there's alpha map specified
      {
         if(alpha_src->typeChannels()>1 && alpha_src->typeInfo().a // if alpha has both RGB and Alpha channels, then check which one to use
         && alpha_src->lockRead())
         {
            Byte min_alpha=255, min_lum=255;
            REPD(y, alpha_src->h())
            REPD(x, alpha_src->w())
            {
               Color c=alpha_src->color(x, y);
               MIN(min_alpha, c.a    );
               MIN(min_lum  , c.lum());
            }
            alpha_src->unlock();
            if(min_alpha>=254 && min_lum<254)if(alpha_src->copyTry(alpha_temp, -1, -1, -1, IMAGE_L8, IMAGE_SOFT, 1))alpha_src=&alpha_temp;else goto error; // alpha channel is fully white -> use luminance as alpha
         }
      }else // if there's no alpha map
      if(color.image.typeInfo().a) // but there is alpha channel in color map
      {
         Byte min_alpha=255;
         alpha_src=&alpha_temp.create(color_src->w(), color_src->h(), 1, IMAGE_A8, IMAGE_SOFT, 1);
         if(color_src->lockRead())
         {
            REPD(y, color_src->h())
            REPD(x, color_src->w())
            {
               Byte a=color_src->color(x, y).a;
                      alpha_temp.pixel(x, y, a);
               MIN(min_alpha, a);
            }
            color_src->unlock();
         }
         if(min_alpha>=254)alpha_temp.del(); // alpha channel in color map is fully white
         else              alpha_from_col=true;
      }
      FILTER_TYPE alpha_filter=Filter((alpha_from_col && alpha.filter<0) ? color.filter : alpha.filter);
      VecI2       alpha_size; if(!alpha_src->is())alpha_size.zero();else alpha_size.set((alpha.size.x>0) ? alpha.size.x : (alpha_from_col && color.size.x>0) ? color.size.x : alpha_src->w(),
                                                                                        (alpha.size.y>0) ? alpha.size.y : (alpha_from_col && color.size.y>0) ? color.size.y : alpha_src->h());

      // set what textures do we have (set this before 'normal' is generated from 'bump')
      if(  color_src->is())ret|=BT_COLOR  ;
      if(  alpha_src->is())ret|=BT_ALPHA  ;
      if(   bump_src->is())ret|=BT_BUMP   ;
      if( normal_src->is())ret|=BT_NORMAL ;
      if( smooth_src->is())ret|=BT_SMOOTH ;
      if(reflect_src->is())ret|=BT_REFLECT;
      if(   glow_src->is())ret|=BT_GLOW   ;

      MTRL_TEX_LAYOUT layout=((ret&(BT_SMOOTH|BT_REFLECT|BT_BUMP|BT_GLOW)) ? MTL_RGB_GLOW$NRM$SMOOTH_REFLECT_BUMP_ALPHA : MTL_RGBA);

      // base_0
      if(layout==MTL_RGB_GLOW$NRM$SMOOTH_REFLECT_BUMP_ALPHA) // put glow in W channel
      {
         Int w=Max(1, ImgW(color, color_src), ImgW(glow, glow_src)), // Max 1 in case all images are empty, but we still need it (because shaders support base_2 only if base_0 is also present, so if we've detected this layout, it means we want base_2 and thus also need base_0)
             h=Max(1, ImgH(color, color_src), ImgH(glow, glow_src)); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( color_src->is() && (color_src->w()!=w || color_src->h()!=h))if(color_src->copyTry(color_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(color.filter), (color.clamp?IC_CLAMP:IC_WRAP)|IC_ALPHA_WEIGHT))color_src=&color_temp;else goto error;
         if(  glow_src->is() && ( glow_src->w()!=w ||  glow_src->h()!=h))if( glow_src->copyTry( glow_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter( glow.filter), ( glow.clamp?IC_CLAMP:IC_WRAP)|IC_ALPHA_WEIGHT)) glow_src=& glow_temp;else goto error;
         if(!color_src->is() ||  color_src->lockRead())
         {
            if(!glow_src->is() || glow_src->lockRead())
            {
               dest_0.createSoftTry(w, h, 1, IMAGE_R8G8B8A8_SRGB);
               REPD(y, dest_0.h())
               REPD(x, dest_0.w())
               {
                  Color c=(color_src->is() ? color_src->color(x, y) : WHITE);
                  if(glow_src->is())
                  {
                     Color glow=glow_src->color(x, y); c.a=DivRound(glow.lum()*glow.a, 255);
                  }else                                c.a=255;
                  dest_0.color(x, y, c);
               }
               glow_src->unlock();
            }
            color_src->unlock();
         }
      }else // put alpha in W channel
      {
         Int w=Max(ImgW(color, color_src), alpha_size.x),
             h=Max(ImgH(color, color_src), alpha_size.y); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( color_src->is() && (color_src->w()!=w || color_src->h()!=h))if(color_src->copyTry(color_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(color.filter), (color.clamp?IC_CLAMP:IC_WRAP)|IC_ALPHA_WEIGHT))color_src=&color_temp;else goto error;
         if( alpha_src->is() && (alpha_src->w()!=w || alpha_src->h()!=h))if(alpha_src->copyTry(alpha_temp, w, h, -1, -1, IMAGE_SOFT, 1,        alpha_filter , (alpha.clamp?IC_CLAMP:IC_WRAP)                ))alpha_src=&alpha_temp;else goto error;
         if(!color_src->is() ||  color_src->lockRead())
         {
            if(!alpha_src->is() || alpha_src->lockRead())
            {
               dest_0.createSoftTry(w, h, 1, IMAGE_R8G8B8A8_SRGB);
               Int alpha_component=(alpha_src->typeInfo().a ? 3 : 0); // use Alpha or Red in case src is R8/L8
               REPD(y, dest_0.h())
               REPD(x, dest_0.w()){Color c=(color_src->is() ? color_src->color(x, y) : WHITE); c.a=(alpha_src->is() ? alpha_src->color(x, y).c[alpha_component] : 255); dest_0.color(x, y, c);} // full alpha
               alpha_src->unlock();
            }
            color_src->unlock();
         }
      }

      // base_1 NRM !! do this first before base_2 SRBA which resizes bump !!
    C Image *bump_to_normal=null;
      if(  bump_src->is() && !normal_src->is()                                                )bump_to_normal=  bump_src;else // if bump available and normal not, then create normal from bump
      if(normal_src->is() && (normal.image.typeChannels()==1 || normal_src->monochromaticRG()))bump_to_normal=normal_src;     // if normal is provided as monochromatic, then treat it as bump and convert to normal
      if(bump_to_normal) // create normal from bump
      {
         // it's best to resize bump instead of normal
         Int w=((normal.size.x>0) ? normal.size.x : (bump_to_normal==bump_src && bump.size.x>0) ? bump.size.x : bump_to_normal->w()),
             h=((normal.size.y>0) ? normal.size.y : (bump_to_normal==bump_src && bump.size.y>0) ? bump.size.y : bump_to_normal->h()); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
       C ImageSource &src=((bump_to_normal==bump_src) ? bump : normal);
         if(bump_to_normal->w()!=w || bump_to_normal->h()!=h)if(bump_to_normal->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(src.filter), (src.clamp?IC_CLAMP:IC_WRAP)))bump_to_normal=&normal_temp;else goto error; // !! convert to 'normal_temp' instead of 'bump_temp' because we still need original bump later !!
         bump_to_normal->bumpToNormal(normal_temp, AvgF(w, h)*BUMP_TO_NORMAL_SCALE); normal_src=&normal_temp;
         flip_normal_y=false; // no need to flip since normal map generated from bump is always correct
      }
      if(normal_src->is())
      {
         Int w=ImgW(normal, normal_src),
             h=ImgH(normal, normal_src); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( normal_src->is() && (normal_src->w()!=w || normal_src->h()!=h))if(normal_src->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(normal.filter), (normal.clamp?IC_CLAMP:IC_WRAP)))normal_src=&normal_temp;else goto error;
         if(!normal_src->is() ||  normal_src->lockRead())
         {
            dest_1.createSoftTry(w, h, 1, IMAGE_R8G8_SIGN, 1);
            Vec4 c; c.zw=0;
            REPD(y, dest_1.h())
            REPD(x, dest_1.w())
            {
               c.xy=(normal_src->is() ? normal_src->colorF(x, y).xy*2-1 : Vec2Zero);
               if(flip_normal_y)CHS(c.y);
               dest_1.colorF(x, y, c);
            }
            normal_src->unlock();
         }
      }

      // base_2 SRBA
      if(layout==MTL_RGB_GLOW$NRM$SMOOTH_REFLECT_BUMP_ALPHA /*&& ret&(BT_SMOOTH|BT_REFLECT|BT_BUMP|BT_ALPHA)*/) // always create 'base2' so we can determine layout based on 'base2' presence
      {
         Int w=Max(1, Max(ImgW(smooth, smooth_src), ImgW(reflect, reflect_src), ImgW(bump, bump_src), alpha_size.x)), // Max 1 in case all images are empty, but we still need it
             h=Max(1, Max(ImgH(smooth, smooth_src), ImgH(reflect, reflect_src), ImgH(bump, bump_src), alpha_size.y)); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}

         if( smooth_src->is() && ( smooth_src->w()!=w ||  smooth_src->h()!=h))if( smooth_src->copyTry( smooth_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter( smooth.filter), ( smooth.clamp?IC_CLAMP:IC_WRAP))) smooth_src=& smooth_temp;else goto error;
         if(reflect_src->is() && (reflect_src->w()!=w || reflect_src->h()!=h))if(reflect_src->copyTry(reflect_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(reflect.filter), (reflect.clamp?IC_CLAMP:IC_WRAP)))reflect_src=&reflect_temp;else goto error;
         if(   bump_src->is() && (   bump_src->w()!=w ||    bump_src->h()!=h))if(   bump_src->copyTry(   bump_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(   bump.filter), (   bump.clamp?IC_CLAMP:IC_WRAP)))   bump_src=&   bump_temp;else goto error;
         if(  alpha_src->is() && (  alpha_src->w()!=w ||   alpha_src->h()!=h))if(  alpha_src->copyTry(  alpha_temp, w, h, -1, -1, IMAGE_SOFT, 1,          alpha_filter , (  alpha.clamp?IC_CLAMP:IC_WRAP)))  alpha_src=&  alpha_temp;else goto error;

         if(!smooth_src->is() || smooth_src->lockRead())
         {
            if(!reflect_src->is() || reflect_src->lockRead())
            {
               if(!bump_src->is() || bump_src->lockRead())
               {
                  if(!alpha_src->is() || alpha_src->lockRead())
                  {
                     dest_2.createSoftTry(w, h, 1, IMAGE_R8G8B8A8);
                     Int alpha_component=(alpha_src->typeInfo().a ? 3 : 0); // use Alpha or Red in case src is R8/L8
                     REPD(y, dest_2.h())
                     REPD(x, dest_2.w())
                     {
                        Color c;
                        c.c[ SMOOTH_CHANNEL]=( smooth_src->is() ?  smooth_src->color(x, y).lum() : 255);
                        c.c[REFLECT_CHANNEL]=(reflect_src->is() ? reflect_src->color(x, y).lum() : 255);
                        c.c[   BUMP_CHANNEL]=(   bump_src->is() ?    bump_src->color(x, y).lum() : BUMP_DEFAULT_TEX);
                        c.c[  ALPHA_CHANNEL]=(  alpha_src->is() ?   alpha_src->color(x, y).c[alpha_component] : 255);
                        dest_2.color(x, y, c);
                     }
                     alpha_src->unlock();
                  }
                  bump_src->unlock();
               }
               reflect_src->unlock();
            }
            smooth_src->unlock();
         }
      }
   }

error:
   Swap(dest_0, base_0);
   Swap(dest_1, base_1);
   Swap(dest_2, base_2);
   return ret;
}
/******************************************************************************/
void CreateDetailTexture(Image &detail, C ImageSource &color, C ImageSource &bump, C ImageSource &normal, C ImageSource &smooth, Bool resize_to_pow2, Bool flip_normal_y)
{
   Image dest;
   {
      Image  color_temp; C Image * color_src=& color.image; if( color_src->compressed())if( color_src->copyTry( color_temp, -1, -1, -1, IMAGE_R8G8B8A8_SRGB, IMAGE_SOFT, 1)) color_src=& color_temp;else goto error;
      Image   bump_temp; C Image *  bump_src=&  bump.image; if(  bump_src->compressed())if(  bump_src->copyTry(  bump_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))  bump_src=&  bump_temp;else goto error;
      Image normal_temp; C Image *normal_src=&normal.image; if(normal_src->compressed())if(normal_src->copyTry(normal_temp, -1, -1, -1, IMAGE_R8G8         , IMAGE_SOFT, 1))normal_src=&normal_temp;else goto error;
      Image smooth_temp; C Image *smooth_src=&smooth.image; if(smooth_src->compressed())if(smooth_src->copyTry(smooth_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))smooth_src=&smooth_temp;else goto error;

      Int w=Max(ImgW(color, color_src), ImgW(smooth, smooth_src)),
          h=Max(ImgH(color, color_src), ImgH(smooth, smooth_src)); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}

      // normal
    C Image *bump_to_normal=null;
      if(  bump_src->is() && !normal_src->is()                                                )bump_to_normal=  bump_src;else // if bump available and normal not, then create normal from bump
      if(normal_src->is() && (normal.image.typeChannels()==1 || normal_src->monochromaticRG()))bump_to_normal=normal_src;     // if normal is provided as monochromatic, then treat it as bump and convert to normal
      if(bump_to_normal) // create normal from bump
      {
         // it's best to resize bump instead of normal
         MAX(w, (normal.size.x>0) ? normal.size.x : (bump_to_normal==bump_src && bump.size.x>0) ? bump.size.x : bump_to_normal->w());
         MAX(h, (normal.size.y>0) ? normal.size.y : (bump_to_normal==bump_src && bump.size.y>0) ? bump.size.y : bump_to_normal->h()); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
       C ImageSource &src=((bump_to_normal==bump_src) ? bump : normal);
         if(bump_to_normal->w()!=w || bump_to_normal->h()!=h)if(bump_to_normal->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(src.filter), (src.clamp?IC_CLAMP:IC_WRAP)))bump_to_normal=&normal_temp;else goto error; // !! convert to 'normal_temp' instead of 'bump_temp' because we still need original bump later !!
         bump_to_normal->bumpToNormal(normal_temp, AvgF(w, h)*BUMP_TO_NORMAL_SCALE); normal_src=&normal_temp;
         flip_normal_y=false; // no need to flip since normal map generated from bump is always correct
      }else
      if(normal_src->is())
      {
         MAX(w, ImgW(normal, normal_src));
         MAX(h, ImgH(normal, normal_src)); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
      }

      if( color_src->is() && ( color_src->w()!=w ||  color_src->h()!=h))if( color_src->copyTry( color_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter( color.filter), ( color.clamp?IC_CLAMP:IC_WRAP)|IC_ALPHA_WEIGHT)) color_src=& color_temp;else goto error;
      if(  bump_src->is() && (  bump_src->w()!=w ||   bump_src->h()!=h))if(  bump_src->copyTry(  bump_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(  bump.filter), (  bump.clamp?IC_CLAMP:IC_WRAP)                ))  bump_src=&  bump_temp;else goto error;
      if(normal_src->is() && (normal_src->w()!=w || normal_src->h()!=h))if(normal_src->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(normal.filter), (normal.clamp?IC_CLAMP:IC_WRAP)                ))normal_src=&normal_temp;else goto error;
      if(smooth_src->is() && (smooth_src->w()!=w || smooth_src->h()!=h))if(smooth_src->copyTry(smooth_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(smooth.filter), (smooth.clamp?IC_CLAMP:IC_WRAP)                ))smooth_src=&smooth_temp;else goto error;

      dest.createSoftTry(w, h, 1, IMAGE_R8G8B8A8);

      if(!color_src->is() || color_src->lockRead())
      {
         if(!bump_src->is() || bump_src->lockRead())
         {
            if(!normal_src->is() || normal_src->lockRead())
            {
               if(!smooth_src->is() || smooth_src->lockRead())
               {
                  REPD(y, dest.h())
                  REPD(x, dest.w())
                  {
                     Byte  color =( color_src->is() ?  color_src->color(x, y).lum() : 255);
                     Byte  bump  =(  bump_src->is() ?   bump_src->color(x, y).lum() : 128);
                     Color normal=(normal_src->is() ? normal_src->color(x, y)       : Color(128, 128, 255, 0)); if(flip_normal_y)normal.g=255-normal.g;
                     Byte  smooth=(smooth_src->is() ? smooth_src->color(x, y).lum() : 255);
                     dest.color(x, y, Color(normal.r, normal.g, color, smooth)); // #MaterialTextureLayout
                  }
                  smooth_src->unlock();
               }
               normal_src->unlock();
            }
            bump_src->unlock();
         }
         color_src->unlock();
      }
   }

error:
   Swap(dest, detail);
}
/******************************************************************************/
UInt CreateWaterBaseTextures(Image &base_0, Image &base_1, Image &base_2, C ImageSource &color, C ImageSource &alpha, C ImageSource &bump, C ImageSource &normal, C ImageSource &smooth, C ImageSource &reflect, C ImageSource &glow, Bool resize_to_pow2, Bool flip_normal_y)
{
   // #WaterMaterialTextureLayout
   UInt  ret=0;
   Image dest_0, dest_1, dest_2;
   {
      Image   color_temp; C Image *  color_src=&  color.image; if(  color_src->compressed())if(  color_src->copyTry(  color_temp, -1, -1, -1, IMAGE_R8G8B8A8_SRGB, IMAGE_SOFT, 1))  color_src=&  color_temp;else goto error;
    //Image   alpha_temp; C Image *  alpha_src=&  alpha.image; if(  alpha_src->compressed())if(  alpha_src->copyTry(  alpha_temp, -1, -1, -1, IMAGE_L8A8         , IMAGE_SOFT, 1))  alpha_src=&  alpha_temp;else goto error;
      Image    bump_temp; C Image *   bump_src=&   bump.image; if(   bump_src->compressed())if(   bump_src->copyTry(   bump_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))   bump_src=&   bump_temp;else goto error;
      Image  normal_temp; C Image * normal_src=& normal.image; if( normal_src->compressed())if( normal_src->copyTry( normal_temp, -1, -1, -1, IMAGE_R8G8         , IMAGE_SOFT, 1)) normal_src=& normal_temp;else goto error;
    //Image  smooth_temp; C Image * smooth_src=& smooth.image; if( smooth_src->compressed())if( smooth_src->copyTry( smooth_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1)) smooth_src=& smooth_temp;else goto error;
    //Image reflect_temp; C Image *reflect_src=&reflect.image; if(reflect_src->compressed())if(reflect_src->copyTry(reflect_temp, -1, -1, -1, IMAGE_L8           , IMAGE_SOFT, 1))reflect_src=&reflect_temp;else goto error;
    //Image    glow_temp; C Image *   glow_src=&   glow.image; if(   glow_src->compressed())if(   glow_src->copyTry(   glow_temp, -1, -1, -1, IMAGE_L8A8         , IMAGE_SOFT, 1))   glow_src=&   glow_temp;else goto error;

      // set what textures do we have (set this before 'normal' is generated from 'bump')
      if(  color_src->is())ret|=BT_COLOR  ;
    //if(  alpha_src->is())ret|=BT_ALPHA  ;
      if(   bump_src->is())ret|=BT_BUMP   ;
      if( normal_src->is())ret|=BT_NORMAL ;
    //if( smooth_src->is())ret|=BT_SMOOTH ;
    //if(reflect_src->is())ret|=BT_REFLECT;
    //if(   glow_src->is())ret|=BT_GLOW   ;

      // base_0
      {
         Int w=ImgW(color, color_src),
             h=ImgH(color, color_src); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( color_src->is() && (color_src->w()!=w || color_src->h()!=h))if(color_src->copyTry(color_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(color.filter), (color.clamp?IC_CLAMP:IC_WRAP)|IC_ALPHA_WEIGHT))color_src=&color_temp;else goto error;
         if(!color_src->is() ||  color_src->lockRead())
         {
            dest_0.createSoftTry(w, h, 1, IMAGE_R8G8B8_SRGB);
            REPD(y, dest_0.h())
            REPD(x, dest_0.w())
            {
               Color c=(color_src->is() ? color_src->color(x, y) : WHITE);
               c.a=255; // force full alpha
               dest_0.color(x, y, c);
            }
            color_src->unlock();
         }
      }

      // base_1 NRM !! do this first before base_2 Bump which resizes bump !!
    C Image *bump_to_normal=null;
      if(  bump_src->is() && !normal_src->is()                                                )bump_to_normal=  bump_src;else // if bump available and normal not, then create normal from bump
      if(normal_src->is() && (normal.image.typeChannels()==1 || normal_src->monochromaticRG()))bump_to_normal=normal_src;     // if normal is provided as monochromatic, then treat it as bump and convert to normal
      if(bump_to_normal) // create normal from bump
      {
         // it's best to resize bump instead of normal
         Int w=((normal.size.x>0) ? normal.size.x : (bump_to_normal==bump_src && bump.size.x>0) ? bump.size.x : bump_to_normal->w()),
             h=((normal.size.y>0) ? normal.size.y : (bump_to_normal==bump_src && bump.size.y>0) ? bump.size.y : bump_to_normal->h()); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
       C ImageSource &src=((bump_to_normal==bump_src) ? bump : normal);
         if(bump_to_normal->w()!=w || bump_to_normal->h()!=h)if(bump_to_normal->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(src.filter), (src.clamp?IC_CLAMP:IC_WRAP)))bump_to_normal=&normal_temp;else goto error; // !! convert to 'normal_temp' instead of 'bump_temp' because we still need original bump later !!
         bump_to_normal->bumpToNormal(normal_temp, AvgF(w, h)*BUMP_TO_NORMAL_SCALE); normal_src=&normal_temp;
         flip_normal_y=false; // no need to flip since normal map generated from bump is always correct
      }
      if(normal_src->is())
      {
         Int w=ImgW(normal, normal_src),
             h=ImgH(normal, normal_src); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( normal_src->is() && (normal_src->w()!=w || normal_src->h()!=h))if(normal_src->copyTry(normal_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(normal.filter), (normal.clamp?IC_CLAMP:IC_WRAP)))normal_src=&normal_temp;else goto error;
         if(!normal_src->is() ||  normal_src->lockRead())
         {
            dest_1.createSoftTry(w, h, 1, IMAGE_R8G8_SIGN, 1);
            Vec4 c; c.zw=0;
            REPD(y, dest_1.h())
            REPD(x, dest_1.w())
            {
               c.xy=(normal_src->is() ? normal_src->colorF(x, y).xy*2-1 : Vec2Zero);
               if(flip_normal_y)CHS(c.y);
               dest_1.colorF(x, y, c);
            }
            normal_src->unlock();
         }
      }

      // base_2 BUMP
      if(bump_src->is())
      {
         Int w=ImgW(bump, bump_src),
             h=ImgH(bump, bump_src); if(resize_to_pow2){w=NearestPow2(w); h=NearestPow2(h);}
         if( bump_src->is() && (bump_src->w()!=w || bump_src->h()!=h))if(bump_src->copyTry(bump_temp, w, h, -1, -1, IMAGE_SOFT, 1, Filter(bump.filter), (bump.clamp?IC_CLAMP:IC_WRAP)))bump_src=&bump_temp;else goto error;
         if(!bump_src->is() ||  bump_src->lockRead())
         {
            dest_2.createSoftTry(w, h, 1, IMAGE_R8_SIGN);
            Vec4 c=0;
            REPD(y, dest_2.h())
            REPD(x, dest_2.w())
            {
               c.x=(bump_src->is() ? bump_src->colorF(x, y).xyz.max()*2-1 : 0);
               dest_2.colorF(x, y, c);
            }
            bump_src->unlock();
         }
      }
   }

error:
   Swap(dest_0, base_0);
   Swap(dest_1, base_1);
   Swap(dest_2, base_2);
   return ret;
}
/******************************************************************************/
Bool CreateBumpFromColor(Image &bump, C Image &color, Flt min_blur_range, Flt max_blur_range)
{
   Image color_temp; C Image *color_src=&color; if(color_src->compressed())if(color_src->copyTry(color_temp, -1, -1, -1, ImageTypeUncompressed(color_src->type()), IMAGE_SOFT, 1))color_src=&color_temp;else goto error;
   {
      Image bump_temp; if(bump_temp.createSoftTry(color.w(), color.h(), 1, IMAGE_F32)) // operate on temporary in case "&bump==&color", create as high precision to get good quality for blur/normalize
      {
         if(color_src->lockRead())
         {
            REPD(y, bump_temp.h())
            REPD(x, bump_temp.w())bump_temp.pixF(x, y)=color_src->colorF(x, y).xyz.max();
            color_src->unlock();

            if(min_blur_range<0)min_blur_range=0; // auto
            if(max_blur_range<0)max_blur_range=3; // auto
            Bool  first=true;
            Flt   power=1;
            Image bump_step;
            for(Flt blur=max_blur_range; ; ) // start with max range, because it's the most important, so we want it to be precise, and then we will go with half steps down
            {
               bump_temp.blur(first ? bump : bump_step, blur, false); // always set the first blur into 'bump' to set it as base, or in case we finish after one step
               if(!first)
               {
                  REPD(y, bump.h())
                  REPD(x, bump.w())bump.pixF(x, y)+=bump_step.pixF(x, y)*power;
               }
               if(blur<=min_blur_range)break;
               first =false;
               blur *=0.5f; if(blur<1)blur=0; // if we reach below 1 blur, then go straight to 0 to avoid doing 0.5, 0.25, 0.125, ..
               power*=0.5f;
            }
            bump.normalize();
            return true;
         }
      }
   }
error:
   bump.del(); return false;
}
/******************************************************************************/
static inline Flt LightSpecular(C Vec &normal, C Vec &light_dir, C Vec &eye_dir, Flt power=64)
{
#if 1 // blinn
   return Pow(Sat(Dot(normal, !(light_dir+eye_dir))), power);
#else // phong
   Vec reflection=!(normal*(2*Dot(normal, light_dir)) - light_dir);
   return Pow(Sat(Dot(reflection, eye_dir)), power);
#endif
}
Bool MergeBaseTextures(Image &base_0, C Material &material, Int image_type, Int max_image_size, C Vec *light_dir, Flt light_power, Flt spec_mul, FILTER_TYPE filter)
{
   // #MaterialTextureLayout

   // dimensions
   VecI2 size=0;
   if(material.base_0)size=Max(size, material.base_0->size());
   if(material.base_1)size=Max(size, material.base_1->size());
   if(material.base_2)size=Max(size, material.base_2->size());
   if(max_image_size>0)
   {
      MIN(size.x, max_image_size);
      MIN(size.y, max_image_size);
   }

   Image color; // operate on temp variable in case 'base_0' argument is set to other images used in this func
   if(size.any()) // this implies we have at least one texture
   {
      if(material.base_0)
      {
         if(!material.base_0->copyTry(color, size.x, size.y, 1, IMAGE_R8G8B8A8_SRGB, IMAGE_SOFT, 1, filter, IC_WRAP))return false; // create new color map, use IMAGE_R8G8B8A8_SRGB to always include Alpha
      }else
      {
         if(!color.createSoftTry(size.x, size.y, 1, IMAGE_R8G8B8A8_SRGB))return false;
         REPD(y, color.h())
         REPD(x, color.w())color.color(x, y, WHITE);
      }

      MAX(light_power, 0);
           spec_mul*=material.smooth*light_power/255.0f;
      Flt  glow_mul =material.glow  *(2*1.75f), // *2 because shaders use this multiplier, *1.75 because shaders iterate over few pixels and take the max out of them (this is just approximation)
           glow_blur=0.07f;
      Bool has_normal=(light_dir       && material.base_1 && material.normal*light_power>0.01f),
           has_spec  =(light_dir       && material.base_2 && material.smooth*light_power>0.01f),
           has_glow  =(material.base_2 && material.base_0 && material.glow              >0.01f); // glow is stored in base0 but only if base2 is present

      Image normal; // 'base_1' resized to 'color' resolution
      if(has_normal      && material.base_1)if(!material.base_1->copyTry(normal, color.w(), color.h(), 1, ImageTypeUncompressed(material.base_1->type()), IMAGE_SOFT, 1, filter, IC_WRAP))return false;

      Image b2; // 'base_2' resized to 'color' resolution
      if((1 || has_spec) && material.base_2)if(!material.base_2->copyTry(b2    , color.w(), color.h(), 1, ImageTypeUncompressed(material.base_2->type()), IMAGE_SOFT, 1, filter, IC_WRAP))return false; // copy always because it has alpha

      // setup glow (before baking normals and overwriting 'color' alpha channel)
      Image glow; if(has_glow && glow.createSoftTry(color.w(), color.h(), 1, IMAGE_F32_3)) // use Vec because we're storing glow with multiplier
      {
         REPD(y, color.h())
         REPD(x, color.w())
         {
            Vec4 c=color.colorF(x, y); // RGB Glow
            c.xyz*=c.c[GLOW_CHANNEL]*glow_mul;
            glow.colorF(x, y, c);
         }
         glow.blur(glow.size3()*glow_blur, false);
      }

      // setup alpha
      Bool has_alpha; // if has any alpha in the texture channel
      if(material.base_2 // if have to replace alpha
      || image_type<=0)  // or set 'has_alpha'
      {
         has_alpha=false;
         REPD(y, color.h())
         REPD(x, color.w())
         {
            Color c=color.color(x, y);
            if(material.base_2)c.a=(b2.is() ? b2.color(x, y).a : 255); // alpha is in base2
            if(c.a<254)has_alpha=true;
            color.color(x, y, c);
         }
      }

      // bake normal map
      if(has_normal || has_spec)
      {
         Flt light=Sat(light_dir->z)*light_power, // light intensity at flat normal without ambient
           ambient=1-light; // setup ambient so light intensity at flat normal is 1
         REPD(y, color.h())
         REPD(x, color.w())
         {
            // I'm assuming that the texture plane is XY plane with Z=0, and facing us (towards -Z) just like browsing image in some viewer
            Color col=color.color(x, y);
            Vec n;
            n.xy=normal.colorF(x, y).xy*material.normal; CHS(n.y);
            n.z=-CalcZ(n.xy);
            n.normalize();

            if(has_normal)
            {
               Flt d=Sat(-Dot(n, *light_dir)), l=ambient + light_power*d;
               col=ColorBrightness(col, l);
            }
            if(has_spec)if(Byte s=b2.color(x, y).c[SMOOTH_CHANNEL])
            {
               Flt spec=LightSpecular(-n, *light_dir, Vec(0, 0, 1))*spec_mul;
               Color cs=ColorBrightness(s*spec); cs.a=0;
               col=ColorAdd(col, cs);
            }
            color.color(x, y, col);
         }
      }

      // apply glow map (after baking normal)
      if(glow.is())
         REPD(y, color.h())
         REPD(x, color.w())
      {
         Color c=color.color(x, y),
               g=glow .color(x, y); g.a=0;
         color.color(x, y, ColorAdd(c, g));
      }

      // image type
      if(image_type<=0)
      {
         if(material.base_0)
         {
            image_type=material.base_0->type();
            if(has_alpha)image_type=ImageTypeIncludeAlpha(IMAGE_TYPE(image_type)); // convert image type to one with    alpha channel
            else         image_type=ImageTypeExcludeAlpha(IMAGE_TYPE(image_type)); // convert image type to one without alpha channel
         }else           image_type=(has_alpha ? IMAGE_BC7_SRGB : IMAGE_BC1_SRGB);
      }
      if(image_type==IMAGE_PVRTC1_2 || image_type==IMAGE_PVRTC1_4 || image_type==IMAGE_PVRTC1_2_SRGB || image_type==IMAGE_PVRTC1_4_SRGB)size=NearestPow2(size.avgI()); // PVRTC1 must be square and pow2

      // final copy
    C ImagePtr &base=(material.base_0 ? material.base_0 : material.base_1 ? material.base_1 : material.base_2);
      if(!color.copyTry(color, size.x, size.y, 1, image_type, base->mode(), (base->mipMaps()>1) ? 0 : 1, filter, IC_WRAP))return false;
   }

   Swap(base_0, color);
   return true;
}
/******************************************************************************/
static Bool CanBeRemoved(C Material &material) {return material.canBeRemoved();} // Renderer Instancing doesn't use incRef/decRef for more performance, so we need to do additional checking for materials if they can be removed from cache, by checking if they're not assigned to any instance

void ShutMaterial() {Materials.del();}
void InitMaterial()
{
   MaterialDefault.cull=true;
   MaterialDefault.validate();

   MaterialDefaultNoCull=MaterialDefault;
   MaterialDefaultNoCull.cull=false;

   Materials.canBeRemoved(CanBeRemoved);
}
/******************************************************************************/
}
/******************************************************************************/

#ifndef ANIME4KCPP_CORE_OPENCL_ANIME4K09_KERNEL_HPP
#define ANIME4KCPP_CORE_OPENCL_ANIME4K09_KERNEL_HPP

#ifdef ENABLE_OPENCL

#ifdef BUILT_IN_KERNEL

namespace Anime4KCPP::OpenCL
{
    static constexpr const char* Anime4KCPPKernelSourceString =
        R"(#define MAX3(a,b,c) fmax(fmax(a,b),c)
#define MIN3(a,b,c) fmin(fmin(a,b),c)
#define RANGE 12.56637061436f
__constant sampler_t samplerN=CLK_NORMALIZED_COORDS_FALSE|CLK_ADDRESS_CLAMP_TO_EDGE|CLK_FILTER_NEAREST;
__constant sampler_t samplerL=CLK_NORMALIZED_COORDS_TRUE|CLK_ADDRESS_CLAMP_TO_EDGE|CLK_FILTER_LINEAR;
inline static void getLightest(float4*mc,float4*a,float4*b,float4*c,float strength){
(*mc)=mad((native_divide((*a)+(*b)+(*c),3.0f)-(*mc)),strength,(*mc));}
inline static void getAVerage(float4*mc,float4*a,float4*b,float4*c,float strength){
(*mc).xyz=mad((native_divide((*a).xyz+(*b).xyz+(*c).xyz,3.0f)-(*mc).xyz),strength,(*mc).xyz);
(*mc).w=0.299f*(*mc).z+0.587f*(*mc).y+0.114f*(*mc).x;}
inline static float Lanczos4(float x){
if(x==0.0f) return 1.0f;
x*=M_PI_F;
if(x>=-RANGE&&x<RANGE) return native_divide(4.0f*native_sin(x)*native_sin(x*0.25f),x*x);
else return 0.0f;}
__kernel void getGrayLanczos4(__read_only image2d_t srcImg,__write_only image2d_t dstImg,float nWidth,float nHeight){
const int x=get_global_id(0),y=get_global_id(1);
if(x>=get_image_width(dstImg)||y>=get_image_height(dstImg)) return;
const int2 coord=(int2)(x,y);
const float2 scale=(float2)(nWidth,nHeight);
const float2 xy=((convert_float2(coord)+0.5f)*scale)-0.5f;
const float2 fxy=floor(xy);
float4 mc=(0.0f);
#pragma unroll 8
for(float sx=fxy.x-3.0f; sx<=fxy.x+4.0f; sx+=1.0f){
float coeffX=Lanczos4(xy.x-sx);
mc+=
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y-3.0f))*coeffX*Lanczos4(xy.y-fxy.y+3.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y-2.0f))*coeffX*Lanczos4(xy.y-fxy.y+2.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y-1.0f))*coeffX*Lanczos4(xy.y-fxy.y+1.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y-0.0f))*coeffX*Lanczos4(xy.y-fxy.y+0.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y+1.0f))*coeffX*Lanczos4(xy.y-fxy.y-1.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y+2.0f))*coeffX*Lanczos4(xy.y-fxy.y-2.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y+3.0f))*coeffX*Lanczos4(xy.y-fxy.y-3.0f)+
read_imagef(srcImg,samplerN,(float2)(sx,fxy.y+4.0f))*coeffX*Lanczos4(xy.y-fxy.y-4.0f);}
mc.w=0.299f*mc.z+0.587f*mc.y+0.114f*mc.x;
write_imagef(dstImg,coord,mc);}
__kernel void getGray(__read_only image2d_t srcImg,__write_only image2d_t dstImg,float nWidth,float nHeight){
const int x=get_global_id(0),y=get_global_id(1);
if(x>=get_image_width(dstImg)||y>=get_image_height(dstImg)) return;
const int2 coord=(int2)(x,y);
float4 mc=read_imagef(srcImg,samplerL,(convert_float2(coord)+0.5f)*(float2)(nWidth,nHeight));
mc.w=0.299f*mc.z+0.587f*mc.y+0.114f*mc.x;
write_imagef(dstImg,coord,mc);}
__kernel void pushColor(__read_only image2d_t srcImg,__write_only image2d_t dstImg,float strength){
const int x=get_global_id(0),y=get_global_id(1);
if(x>=get_image_width(srcImg)||y>=get_image_height(srcImg)) return;
int2 coord=(int2)(x,y);
float4 tl=read_imagef(srcImg,samplerN,(int2)(x-1,y-1));
float4 tc=read_imagef(srcImg,samplerN,(int2)(x,y-1));
float4 tr=read_imagef(srcImg,samplerN,(int2)(x+1,y-1));
float4 ml=read_imagef(srcImg,samplerN,(int2)(x-1,y));
float4 mc=read_imagef(srcImg,samplerN,coord);
float4 mr=read_imagef(srcImg,samplerN,(int2)(x+1,y));
float4 bl=read_imagef(srcImg,samplerN,(int2)(x-1,y+1));
float4 bc=read_imagef(srcImg,samplerN,(int2)(x,y+1));
float4 br=read_imagef(srcImg,samplerN,(int2)(x+1,y+1));
float maxD,minL;
maxD=MAX3(bl.w,bc.w,br.w);
minL=MIN3(tl.w,tc.w,tr.w);
if(minL>mc.w&&mc.w>maxD) getLightest(&mc,&tl,&tc,&tr,strength);
else{ maxD=MAX3(tl.w,tc.w,tr.w);
minL=MIN3(bl.w,bc.w,br.w);
if(minL>mc.w&&mc.w>maxD) getLightest(&mc,&bl,&bc,&br,strength);}
maxD=MAX3(ml.w,mc.w,bc.w);
minL=MIN3(tc.w,tr.w,mr.w);
if(minL>maxD) getLightest(&mc,&tc,&tr,&mr,strength);
else{ maxD=MAX3(tc.w,mc.w,mr.w);
minL=MIN3(ml.w,bl.w,bc.w);
if(minL>maxD) getLightest(&mc,&ml,&bl,&bc,strength);}
maxD=MAX3(tl.w,ml.w,bl.w);
minL=MIN3(tr.w,mr.w,br.w);
if(minL>mc.w&&mc.w>maxD) getLightest(&mc,&tr,&mr,&br,strength);
else{ maxD=MAX3(tr.w,mr.w,br.w);
minL=MIN3(tl.w,ml.w,bl.w);
if(minL>mc.w&&mc.w>maxD) getLightest(&mc,&tl,&ml,&bl,strength);}
maxD=MAX3(tc.w,mc.w,ml.w);
minL=MIN3(mr.w,br.w,bc.w);
if(minL>maxD) getLightest(&mc,&mr,&br,&bc,strength);
else{ maxD=MAX3(bc.w,mc.w,mr.w);
minL=MIN3(ml.w,tl.w,tc.w);
if(minL>maxD) getLightest(&mc,&ml,&tl,&tc,strength);}
write_imagef(dstImg,coord,mc);}
__kernel void getGradient(__read_only image2d_t srcImg,__write_only image2d_t dstImg){
const int x=get_global_id(0),y=get_global_id(1);
if(x>=get_image_width(srcImg)||y>=get_image_height(srcImg)) return;
int2 coord=(int2)(x,y);
float4 tl=read_imagef(srcImg,samplerN,(int2)(x-1,y-1));
float4 tc=read_imagef(srcImg,samplerN,(int2)(x,y-1));
float4 tr=read_imagef(srcImg,samplerN,(int2)(x+1,y-1));
float4 ml=read_imagef(srcImg,samplerN,(int2)(x-1,y));
float4 mc=read_imagef(srcImg,samplerN,coord);
float4 mr=read_imagef(srcImg,samplerN,(int2)(x+1,y));
float4 bl=read_imagef(srcImg,samplerN,(int2)(x-1,y+1));
float4 bc=read_imagef(srcImg,samplerN,(int2)(x,y+1));
float4 br=read_imagef(srcImg,samplerN,(int2)(x+1,y+1));
const float gradX=tr.w+mr.w+mr.w+br.w-tl.w-ml.w-ml.w-bl.w;
const float gradY=tl.w+tc.w+tc.w+tr.w-bl.w-bc.w-bc.w-br.w;
const float grad=clamp(native_sqrt(gradX*gradX+gradY*gradY),0.0f,1.0f);
mc.w=1.0f-grad;
write_imagef(dstImg,coord,mc);}
__kernel void pushGradient(__read_only image2d_t srcImg,__write_only image2d_t dstImg,float strength){
const int x=get_global_id(0),y=get_global_id(1);
if(x>=get_image_width(srcImg)||y>=get_image_height(srcImg)) return;
int2 coord=(int2)(x,y);
float4 tl=read_imagef(srcImg,samplerN,(int2)(x-1,y-1));
float4 tc=read_imagef(srcImg,samplerN,(int2)(x,y-1));
float4 tr=read_imagef(srcImg,samplerN,(int2)(x+1,y-1));
float4 ml=read_imagef(srcImg,samplerN,(int2)(x-1,y));
float4 mc=read_imagef(srcImg,samplerN,coord);
float4 mr=read_imagef(srcImg,samplerN,(int2)(x+1,y));
float4 bl=read_imagef(srcImg,samplerN,(int2)(x-1,y+1));
float4 bc=read_imagef(srcImg,samplerN,(int2)(x,y+1));
float4 br=read_imagef(srcImg,samplerN,(int2)(x+1,y+1));
float maxD,minL;
maxD=MAX3(bl.w,bc.w,br.w);
minL=MIN3(tl.w,tc.w,tr.w);
if(minL>mc.w&&mc.w>maxD){ getAVerage(&mc,&tl,&tc,&tr,strength);
write_imagef(dstImg,coord,mc);
return;} 
maxD=MAX3(tl.w,tc.w,tr.w);
minL=MIN3(bl.w,bc.w,br.w);
if(minL>mc.w&&mc.w>maxD){ getAVerage(&mc,&bl,&bc,&br,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(ml.w,mc.w,bc.w);
minL=MIN3(tc.w,tr.w,mr.w);
if(minL>maxD){ getAVerage(&mc,&tc,&tr,&mr,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(tc.w,mc.w,mr.w);
minL=MIN3(ml.w,bl.w,bc.w);
if(minL>maxD){ getAVerage(&mc,&ml,&bl,&bc,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(tl.w,ml.w,bl.w);
minL=MIN3(tr.w,mr.w,br.w);
if(minL>mc.w&&mc.w>maxD){ getAVerage(&mc,&tr,&mr,&br,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(tr.w,mr.w,br.w);
minL=MIN3(tl.w,ml.w,bl.w);
if(minL>mc.w&&mc.w>maxD){ getAVerage(&mc,&tl,&ml,&bl,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(tc.w,mc.w,ml.w);
minL=MIN3(mr.w,br.w,bc.w);
if(minL>maxD){ getAVerage(&mc,&mr,&br,&bc,strength);
write_imagef(dstImg,coord,mc);
return;}
maxD=MAX3(bc.w,mc.w,mr.w);
minL=MIN3(ml.w,tl.w,tc.w);
if(minL>maxD){ getAVerage(&mc,&ml,&tl,&tc,strength);
write_imagef(dstImg,coord,mc);
return;}
mc.w=0.299f*mc.z+0.587f*mc.y+0.114f*mc.x;
write_imagef(dstImg,coord,mc);})";
}

#endif // BUILT_IN_KERNEL

#endif // ENABLE_OPENCL

#endif // !ANIME4KCPP_CORE_OPENCL_ANIME4K09_KERNEL_HPP

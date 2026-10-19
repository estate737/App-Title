// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 2048
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#include "stb_truetype.h"
#include <dirent.h>
#include <sys/stat.h>

namespace app_title {
static unsigned char* g_font=nullptr;
static stbtt_fontinfo g_info{};
static bool font_ready(const char* path) {
    if(g_font)return true;
    FILE* f=fopen(path,"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    const long size=ftell(f);
    if(size<=0||size>16*1024*1024){fclose(f);return false;}
    rewind(f);auto* bytes=static_cast<unsigned char*>(malloc(static_cast<std::size_t>(size)));
    const bool ok=bytes && fread(bytes,1,static_cast<std::size_t>(size),f)==static_cast<std::size_t>(size);
    fclose(f);
    if(!ok||!stbtt_InitFont(&g_info,bytes,0)){free(bytes);return false;}
    g_font=bytes;return true;
}
static void blend(unsigned char* p,unsigned r,unsigned g,unsigned b,unsigned a) {
    const unsigned out_a=a+(p[3]*(255-a)+127)/255;
    const unsigned colors[]={r,g,b};
    for(int c=0;c<3;++c) {
        const unsigned premul=colors[c]*a+(p[c]*p[3]*(255-a)+127)/255;
        p[c]=out_a?static_cast<unsigned char>((premul+out_a/2)/out_a):0;
    }
    p[3]=static_cast<unsigned char>(out_a);
}
bool render_icon(const char* source,const char* output,const char* id,const Config& cfg,const char* font) {
    if(!valid_id(id,std::strlen(id))||!font_ready(font))return false;
    int w=0,h=0,n=0;
    if(!stbi_info(source,&w,&h,&n)||w<32||h<32||w>2048||h>2048)return false;
    auto* img=stbi_load(source,&w,&h,&n,4);if(!img)return false;
    const float height=h*0.105f;
    float scale=stbtt_ScaleForPixelHeight(&g_info,height);
    int advance=0;
    for(const char* s=id;*s;++s){int a,b;stbtt_GetCodepointHMetrics(&g_info,*s,&a,&b);advance+=a;}
    if(advance*scale>w*0.90f)scale=w*0.90f/advance;
    int ascent,descent,gap;stbtt_GetFontVMetrics(&g_info,&ascent,&descent,&gap);
    const int text_h=static_cast<int>((ascent-descent)*scale+0.5f);
    const int padding=h/40+1;
    const int strip_h=text_h+padding*2;
    const int strip_y=cfg.top?0:h-strip_h;
    const unsigned lum=(((cfg.color>>16)&255)*299+((cfg.color>>8)&255)*587+(cfg.color&255)*114)/1000;
    const unsigned background=lum<100?255:0;
    for(int y=strip_y;y<strip_y+strip_h && y<h;++y)
        for(int x=0;x<w;++x)blend(img+(y*w+x)*4,background,background,background,170);
    int x=static_cast<int>((w-advance*scale)/2);
    const int baseline=strip_y+padding+static_cast<int>(ascent*scale);
    for(const char* s=id;*s;++s) {
        int a,b,bw,bh,xoff,yoff;stbtt_GetCodepointHMetrics(&g_info,*s,&a,&b);
        auto* bm=stbtt_GetCodepointBitmap(&g_info,scale,scale,*s,&bw,&bh,&xoff,&yoff);
        if(bm) {
            for(int by=0;by<bh;++by)for(int bx=0;bx<bw;++bx) {
                const int px=x+xoff+bx,py=baseline+yoff+by;
                if(px>=0&&px<w&&py>=0&&py<h)
                    blend(img+(py*w+px)*4,(cfg.color>>16)&255,(cfg.color>>8)&255,cfg.color&255,bm[by*bw+bx]);
            }
            stbtt_FreeBitmap(bm,nullptr);
        }
        x+=static_cast<int>(a*scale+0.5f);
    }
    int bytes=0;auto* png=stbi_write_png_to_mem(img,w*4,w,h,4,&bytes);
    const bool ok=png && bytes>0 && atomic_file(output,png,static_cast<std::size_t>(bytes));
    free(png);stbi_image_free(img);return ok;
}
void scan_icons(const Config& c,Stats& stats) {
    const char* roots[]={"/user/appmeta","/system_data/priv/mms/appmeta"};
    const char* fonts[]={"/preinst/common/font/SST-Medium.otf","/preinst/common/font/SST-Roman.otf"};
    const char* font=fonts[0];struct stat fs{};
    if(stat(font,&fs)!=0)font=fonts[1];
    log_fmt("icons.font.begin path=%s",font);
    if(!font_ready(font)) {
        stats.icons=0;stats.failed=1;
        log_line("icons.font.end ready=0; cannot prepare annotated PNGs");
        return;
    }
    log_line("icons.font.end ready=1");
    unsigned count=0,fail=0;
    for(const char* root:roots) {
        log_fmt("icons.scan.begin root=%s",root);
        DIR* d=opendir(root);if(!d){log_fmt("icons.scan.unavailable root=%s errno=%d",root,errno);continue;}
        while(auto* e=readdir(d)) {
            if(!valid_id(e->d_name,std::strlen(e->d_name)))continue;
            char source[512],output[512];
            std::snprintf(source,sizeof(source),"%s/%s/icon0.png",root,e->d_name);
            std::snprintf(output,sizeof(output),"%s/%s.png",kCache,e->d_name);
            struct stat st{};if(lstat(source,&st)!=0||!S_ISREG(st.st_mode))continue;
            // Always derive from the original file; the controller has no read hooks.
            if(render_icon(source,output,e->d_name,c,font))++count;
            else {++fail;if(fail<=10)log_fmt("Icon failed: %s",source);}
        }
        closedir(d);
        log_fmt("icons.scan.end root=%s generated=%u failed=%u",root,count,fail);
    }
    stats.icons=count;stats.failed=fail;
    log_fmt("Generated icons=%u failed=%u position=%s color=%06x",count,fail,c.top?"top":"bottom",c.color);
}
}

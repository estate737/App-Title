// SPDX-License-Identifier: GPL-3.0-or-later
#include "home_style.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
namespace {
void hash_test(const std::string& input,const char* expected) {
    std::uint8_t digest[20];app_title::sha1_digest(input.data(),input.size(),digest);
    char text[41]{};for(unsigned i=0;i<20;++i)std::snprintf(text+i*2,3,"%02x",digest[i]);
    assert(!std::strcmp(text,expected));
}
std::uint32_t get32(const unsigned char* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
void put32(unsigned char* p,std::uint32_t v){std::memcpy(p,&v,4);}
}
int main(int argc,char** argv) {
    using namespace app_title;
    hash_test("","da39a3ee5e6b4b0d3255bfef95601890afd80709");
    hash_test("abc","a9993e364706816aba3e25717850c26c9cd0d89d");
    hash_test("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    hash_test(std::string(1000000,'a'),"34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    Config config{};assert(!config.top&&config.color==0xffffff);
    std::uint8_t code[183]{},small[10];std::memset(small,0x5a,sizeof(small));
    assert(build_title_style(code,sizeof(code),config)==181);
    assert(!build_title_style(small,sizeof(small),config));for(auto v:small)assert(v==0x5a);
    config.top=true;assert(build_title_style(code,sizeof(code),config)==180);
    config.color=0x1000000;assert(!build_title_style(code,sizeof(code),config));config={};
    unsigned char malformed[512]{};assert(patch_home_style(malformed,sizeof(malformed),config)==HomeStyleResult::ignored);
    std::memcpy(malformed,"RNPSHEDR",8);put32(malformed+8,2);put32(malformed+0x1c,64);put32(malformed+0x20,448);put32(malformed+0x24,448);put32(malformed+0x28,512);
    std::memcpy(malformed+64,"\xc6\x1f\xbc\x03\xc1\x03\x19\x1f",8);
    constexpr char marker[]="isHomeTitleIdDisplayEnabled";
    std::memcpy(malformed+128,marker,sizeof(marker)-1);
    const std::vector<unsigned char> before(malformed,malformed+sizeof(malformed));
    assert(patch_home_style(malformed,sizeof(malformed),config)==HomeStyleResult::unsupported);
    assert(!std::memcmp(malformed,before.data(),sizeof(malformed)));
    for(std::size_t n=0;n<sizeof(malformed);++n)(void)patch_home_style(malformed,n,config);
    if(argc>=2) {
        std::ifstream input(argv[1],std::ios::binary);assert(input.good());
        const std::vector<unsigned char> original((std::istreambuf_iterator<char>(input)),{});
        assert(original.size()==1784176);
        const auto off=get32(original.data()+0x1c),plain=get32(original.data()+0x24);
        for(unsigned i=0;i<8;++i) {
            auto bytes=original;config.top=(i&1)!=0;config.color=(i%4)<2?0xffffff:0x12abef;config.outside=i<4;
            assert(patch_home_style(bytes.data(),bytes.size(),config)==HomeStyleResult::applied);
            constexpr unsigned functions[]={1461,6665,6513,6509,282,6448,1079};
            constexpr std::size_t offsets[]={0xe3d20,0x15b120,0x15654a,0x156394,0xcbf25,0x153de8,0xdaff4};
            constexpr std::size_t lengths[]={183,622,78,266,402,1747,361};
            for(std::size_t j=0;j<bytes.size();++j)if(bytes[j]!=original[j]) {
                bool allowed=false;
                if(j>=off) {
                    const auto at=j-off;
                    for(unsigned k=0;k<(config.outside?7u:1u);++k)allowed|=(at>=offsets[k]&&at<offsets[k]+lengths[k])||(at>=128+16*functions[k]&&at<144+16*functions[k]);
                    allowed|=at>=plain-20&&at<plain;
                }
                assert(allowed);
            }
            std::uint8_t hash[20];sha1_digest(bytes.data()+off,plain-20,hash);assert(!std::memcmp(hash,bytes.data()+off+plain-20,20));
            const std::string path="build-host/home_style_"+std::to_string(i)+".hbc";
            std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()+off),plain);assert(output.good());
        }
        for(unsigned mode=0;mode<15;++mode) {
            auto bytes=original;
            switch(mode) {
                case 0:bytes[off+0xe3d20]^=1;break;
                case 1:bytes[off+128+16*1461+14]^=1;break;
                case 2:bytes[off+plain-1]^=1;break;
                case 3:put32(bytes.data()+off+8,90);break;
                case 4:put32(bytes.data()+0x24,128);break;
                case 5:bytes[off+0x15b120]^=1;break;
                case 6:bytes[off+128+16*6665+14]^=1;break;
                case 7:bytes[off+0x15654a]^=1;break;
                case 8:bytes[off+128+16*6513+14]^=1;break;
                case 9:bytes[off+0xcbf25]^=1;break;
                case 10:bytes[off+0x153de8]^=1;break;
                case 11:bytes[off+0x156394]^=1;break;
                case 12:bytes[off+128+16*282+14]^=1;break;
                case 13:bytes[off+0xdaff4]^=1;break;
                case 14:bytes[off+128+16*1079+4]^=1;break;
            }
            const auto copy=bytes;assert(patch_home_style(bytes.data(),bytes.size(),config)==HomeStyleResult::invalid);assert(bytes==copy);
        }
        puts("PASS: supplied HomeUI patched in eight inside/outside colour/position combinations; only allowlisted functions, their metadata and checksum changed; altered/unsupported inputs left intact");
    } else puts("INFO: supply HomeUI_decrypted.bin to additionally validate the known console profile");
    puts("PASS: SHA-1 vectors, unsigned RGBA bytecode generation, white/bottom defaults, small-buffer and malformed-bundle refusal");
}

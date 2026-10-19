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
std::uint32_t get32(const unsigned char* p){std::uint32_t v;std::memcpy(&v,p,4);return v;}
void put32(unsigned char* p,std::uint32_t v){std::memcpy(p,&v,4);}
}
int main(int argc,char** argv) {
    using namespace app_title;
    if(argc<2){puts("INFO: supply the local 4.03 HomeUI fixture for RAM bundle checks");return 0;}
    std::ifstream input(argv[1],std::ios::binary);assert(input.good());
    const std::vector<unsigned char> original((std::istreambuf_iterator<char>(input)),{});
    assert(original.size()==1389744);
    const auto offset=get32(original.data()+0x1c),plain=get32(original.data()+0x24);
    assert(offset==2848&&plain==1386504&&get32(original.data()+offset)==0xfb0bd1e5u);
    constexpr unsigned ids[]={25,102,219,527,531,552};
    constexpr std::size_t starts[]={67100,148437,308526,727007,739593,771182};
    constexpr std::size_t lengths[]={1205,1016,567,5276,1507,1948};
    Config config{};assert(config.enabled&&!config.top&&config.outside&&config.color==0xffffff);
    for(unsigned mode=0;mode<12;++mode) {
        config.outside=mode<4||(mode>=8&&mode<10);config.top=(mode&1)!=0;
        config.color=mode>=8?0:mode%4<2?0xffffff:0x12abef;
        auto bytes=original;
        assert(patch_home_style(bytes.data(),bytes.size(),config)==HomeStyleResult::applied);
        for(std::size_t j=0;j<bytes.size();++j)if(bytes[j]!=original[j]) {
            bool allowed=false;
            if(j>=offset)for(unsigned k=0;k<6;++k)
                if(config.outside||k==2)allowed|=j-offset>=starts[k]&&j-offset<starts[k]+lengths[k];
            assert(allowed);
        }
        // Every table offset/length, startup code, terminal NUL and unrelated
        // module remains at its original address, including all launch logic.
        assert(!std::memcmp(bytes.data(),original.data(),offset+67100));
        for(unsigned i=0;i<858;++i) {
            const auto* entry=bytes.data()+offset+12+8*i;
            const auto at=6876+get32(entry),n=get32(entry+4);
            if(!n)continue;
            assert(at<=plain&&n<=plain-at&&bytes[offset+at+n-1]==0);
            assert(!std::memchr(bytes.data()+offset+at,0,n-1));
        }
        for(unsigned k=0;k<6;++k) {
            const auto* entry=bytes.data()+offset+12+8*ids[k];
            assert(get32(entry)==starts[k]-6876&&get32(entry+4)==lengths[k]);
        }
        const auto copy=bytes;
        assert(patch_home_style(bytes.data(),bytes.size(),config)!=HomeStyleResult::applied&&bytes==copy);
        const std::string path="build-host/ram_style_"+std::to_string(mode)+".bin";
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());assert(out.good());
    }
    for(unsigned mode=0;mode<23;++mode) {
        auto bytes=original;
        if(mode<6)bytes[offset+starts[mode]]^=1;
        else if(mode<12)bytes[offset+12+8*ids[mode-6]]^=1;
        else switch(mode) {
            case 12:bytes[offset+12+8*554+4]^=1;break;
            case 13:bytes[offset+plain-2]^=1;break;
            case 14:put32(bytes.data()+offset+4,859);break;
            case 15:put32(bytes.data()+offset+8,1796);break;
            case 16:put32(bytes.data()+0x24,128);break;
            case 17:put32(bytes.data()+0x24,plain-1);break;
            case 18:put32(bytes.data()+0x28,0xffffffffu);break;
            case 19:put32(bytes.data()+0x1c,0xfffffff0u);break;
            case 20:bytes[offset+starts[2]+lengths[2]-1]='x';break;
            case 21:bytes[offset+starts[5]+120]=0;break;
            case 22:bytes[offset+806000]^=1;break;
        }
        const auto copy=bytes;
        assert(patch_home_style(bytes.data(),bytes.size(),config)!=HomeStyleResult::applied&&bytes==copy);
    }
    auto unchanged=original;config.color=0x1000000;
    assert(patch_home_style(unchanged.data(),unchanged.size(),config)==HomeStyleResult::ignored&&unchanged==original);
    config={};
    for(std::size_t n:{0u,1u,63u,64u,2848u,4096u,1389360u-1}) {
        assert(patch_home_style(unchanged.data(),n,config)!=HomeStyleResult::applied&&unchanged==original);
    }
    puts("PASS: exact 4.03 RAM profile; twelve inside/outside colour/position combinations including black; module capacity/NUL preservation, unchanged headers, table, startup, launch modules and trailer; 23 tamper cases and truncated buffers refused without writes");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "relocate.hpp"
#include "hde64.h"
#include <cstring>
#include <climits>
namespace app_title {
void jump(std::uint8_t* out,std::uintptr_t target) {
    const std::uint8_t prefix[]={0xff,0x25,0,0,0,0};
    std::memcpy(out,prefix,6);std::memcpy(out+6,&target,8);
}
bool relative_jump(std::uint8_t* out,std::uintptr_t source,std::uintptr_t target) {
    const auto delta=static_cast<std::int64_t>(target)-static_cast<std::int64_t>(source+5);
    if(delta<INT32_MIN||delta>INT32_MAX)return false;
    const auto rel=static_cast<std::int32_t>(delta);out[0]=0xe9;std::memcpy(out+1,&rel,4);return true;
}
bool relocate(const std::uint8_t* code,std::size_t available,
              std::uintptr_t source,std::uintptr_t dest,
              std::uint8_t* output,std::size_t cap,
              std::size_t& used,std::size_t& written,std::size_t minimum) {
    used=written=0;if(!code||!output||available<32||cap<64||minimum<5||minimum>14)return false;
    if(!std::memcmp(code,"\xff\x25\0\0\0\0",6)) {
        std::uintptr_t next;std::memcpy(&next,code+6,8);
        if(next<0x10000||next>=0x800000000000ULL)return false;
        jump(output,next);used=14;written=14;return true;
    }
    struct Instruction {hde64s d; std::size_t offset;};
    Instruction list[16]{};unsigned count=0;
    while(used<minimum) {
        if(count>=16||used+16>available)return false;
        auto& i=list[count++];i.offset=used;
        hde64_disasm(code+used,&i.d);
        // Keep kernel entry instructions at their native addresses. Moving
        // SYSCALL/SYSENTER/INT changes the kernel-visible origin/return PC.
        const bool kernel_entry=(i.d.opcode==0x0f&&(i.d.opcode2==0x05||i.d.opcode2==0x34||i.d.opcode2==0x07||i.d.opcode2==0x35)) ||
            i.d.opcode==0xcd||i.d.opcode==0xcc||i.d.opcode==0xce||i.d.opcode==0xcf;
        if(!i.d.len || i.d.flags&F_ERROR || kernel_entry || i.d.opcode==0xc3 || i.d.opcode==0xc2 ||
           (i.d.opcode>=0xe0&&i.d.opcode<=0xe3))return false;
        used+=i.d.len;
    }
    for(unsigned k=0;k<count;++k) {
        const auto& d=list[k].d;const auto offset=list[k].offset;
        const auto* p=code+offset;std::uintptr_t target=0;
        if(written+32>cap)return false;
        if(d.flags&F_RELATIVE) {
            std::int64_t disp;
            if(d.flags&F_IMM8)disp=static_cast<std::int8_t>(d.imm.imm8);
            else if(d.flags&F_IMM32)disp=static_cast<std::int32_t>(d.imm.imm32);
            else return false;
            target=source+offset+d.len+disp;
            // Internal branches require instruction-offset fixups. Reject them.
            if(target>=source && target<source+used)return false;
            if((d.opcode>=0x70&&d.opcode<=0x7f) || (d.opcode==0x0f&&d.opcode2>=0x80&&d.opcode2<=0x8f)) {
                output[written++]=static_cast<std::uint8_t>(0x70|(((d.opcode==0x0f?d.opcode2:d.opcode)&15)^1));
                output[written++]=14;jump(output+written,target);written+=14;
            } else if(d.opcode==0xe9||d.opcode==0xeb) {
                jump(output+written,target);written+=14;
            } else if(d.opcode==0xe8) {
                const std::uint8_t call[]={0xff,0x15,2,0,0,0,0xeb,8};
                std::memcpy(output+written,call,8);std::memcpy(output+written+8,&target,8);written+=16;
            } else return false;
        } else {
            std::memcpy(output+written,p,d.len);
            if((d.flags&F_MODRM) && d.modrm_mod==0 && d.modrm_rm==5 && !d.p_67) {
                const unsigned imm=(d.flags&F_IMM8?1:0)+(d.flags&F_IMM16?2:0)+(d.flags&F_IMM32?4:0)+(d.flags&F_IMM64?8:0);
                if(d.len<4+imm)return false;
                const auto dp=d.len-imm-4;
                std::int32_t old;std::memcpy(&old,p+dp,4);
                const auto delta=static_cast<std::int64_t>(source+offset+d.len)+old-static_cast<std::int64_t>(dest+written+d.len);
                if(delta<INT32_MIN||delta>INT32_MAX)return false;
                const auto adjusted=static_cast<std::int32_t>(delta);std::memcpy(output+written+dp,&adjusted,4);
            }
            written+=d.len;
        }
    }
    if(written+14>cap)return false;
    jump(output+written,source+used);written+=14;return true;
}
}

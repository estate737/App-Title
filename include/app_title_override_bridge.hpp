// SPDX-License-Identifier: GPL-3.0-or-later
// Local App Title / Override cooperation, inside one resident ShellUI process.
#pragma once
#include <cstdint>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace app_title_override {
using Transform=void(*)(int,unsigned long,void*,int);
using Register=int(*)(std::uint32_t,int,Transform,bool);
constexpr std::uint64_t kMagic=0x314f525449545041ULL;
constexpr std::uint32_t kAbi=1;
constexpr char kPath[]="/system_tmp/app_title_override_bridge";
struct Record {
    std::uint64_t magic=kMagic;
    std::uint32_t abi=kAbi;
    std::int32_t pid=0;
    std::uint64_t registrar=0,target=0,replacement=0;
};
static_assert(sizeof(Record)==40,"App Title / Override bridge ABI");
// 0 means absent; -1 means unsafe/stale. Never call a stale process pointer.
inline int read(Record& out,int pid,const char* path=kPath) {
    const int fd=open(path,O_RDONLY|O_NOFOLLOW);
    if(fd<0)return errno==ENOENT?0:-1;
    struct stat st{};Record record{};char tail=0;
    const bool ok=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==getuid()&&
        !(st.st_mode&0022)&&st.st_size==static_cast<off_t>(sizeof(record))&&
        ::read(fd,&record,sizeof(record))==static_cast<ssize_t>(sizeof(record))&&::read(fd,&tail,1)==0;
    const bool closed=close(fd)==0;
    if(!ok||!closed||record.magic!=kMagic||record.abi!=kAbi||record.pid!=pid||
       record.registrar<0x10000||record.registrar>=0x800000000000ULL||
       record.target<0x10000||record.target>=0x800000000000ULL||
       record.replacement<0x10000||record.replacement>=0x800000000000ULL)return -1;
    out=record;return 1;
}
// App Title's checked relay remains the sole ioctl owner in this order.
inline bool matches(const Record& record,const void* target) {
    if(record.target!=reinterpret_cast<std::uintptr_t>(target))return false;
    const auto* code=static_cast<const unsigned char*>(target);
    if(code[0]!=0xe9)return false;
    std::int32_t displacement=0;std::memcpy(&displacement,code+1,4);
    const auto relay=record.target+5+displacement;
    if(relay<0x10000||relay>=0x800000000000ULL-14)return false;
    const auto* bytes=reinterpret_cast<const unsigned char*>(relay);
    const unsigned char prefix[]={0xff,0x25,0,0,0,0};
    std::uint64_t destination=0;std::memcpy(&destination,bytes+6,8);
    return !std::memcmp(bytes,prefix,6)&&destination==record.replacement;
}
}

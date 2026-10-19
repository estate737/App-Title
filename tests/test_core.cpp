// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include "relocate.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
int main(int argc,char** argv) {
    using namespace app_title;
    if(argc>1) {
        Config supplied{};
        assert(load_config(supplied,argv[1]));
        assert(supplied.enabled&&!supplied.top&&supplied.color==0xffffff);
        puts("PASS: supplied config.ini reads as enabled=1, bottom, ffffff");
    }
    assert(valid_id("CUSA00001",9));assert(valid_id("PPSA03846",9));assert(!valid_id("../X00001",9));assert(!valid_id("cusa00001",9));
    char path[256]{};assert(redirect_path("/user/appmeta/CUSA00001/icon0.png",path,sizeof(path)));
    assert(!std::strcmp(path,"/data/AppTitle/cache/CUSA00001.png"));
    assert(!redirect_path("/user/appmeta/CUSA00001/icon0.png/other",path,sizeof(path)));
    assert(!redirect_path("/user/appmeta/../../x/icon0.png",path,sizeof(path)));
    assert(!redirect_path("/mnt/game/CUSA00001/icon0.png",path,sizeof(path)));
    Config c{};assert(parse_settings("enabled=1&position=top&color=1122aF",c));assert(c.enabled&&c.top&&c.color==0x1122af);
    assert(c.outside);
    assert(parse_settings("enabled=1&position=top&color=1122af&placement=inside",c)&&!c.outside);
    assert(!parse_settings("enabled=1&position=top&color=1122af&placement=other",c));
    assert(!parse_settings("enabled=1&position=top&color=1122af&placement=inside&placement=outside",c));
    Config old=c;assert(!parse_settings("enabled=1&position=top&color=1122af&enabled=0",c));assert(c.color==old.color);
    assert(!parse_settings("enabled=1&position=../../&color=ffffff",c));assert(!parse_settings("enabled=1&position=top&color=bad",c));
    char file[]="/tmp/apptitle-test-XXXXXX";int fd=mkstemp(file);assert(fd>=0);close(fd);
    assert(save_config(c,file));Config reloaded{};assert(load_config(reloaded,file));assert(reloaded.color==c.color&&reloaded.top==c.top&&reloaded.outside==c.outside);unlink(file);
    RuntimeStatus runtime{};runtime.pid=123;runtime.enabled=1;runtime.overrides=7;
    assert(atomic_file(file,&runtime,sizeof(runtime)));
    RuntimeStatus loaded{};assert(load_runtime_status(123,loaded,file)&&loaded.overrides==7);
    assert(!load_runtime_status(124,loaded,file));
    assert(atomic_file(file,&runtime,sizeof(runtime)-1));assert(!load_runtime_status(123,loaded,file));
    unsigned char oversized[sizeof(RuntimeStatus)+1]{};std::memcpy(oversized,&runtime,sizeof(runtime));
    assert(atomic_file(file,oversized,sizeof(oversized)));assert(!load_runtime_status(123,loaded,file));
    runtime.style_top=3;assert(atomic_file(file,&runtime,sizeof(runtime)));assert(load_runtime_status(123,loaded,file));
    runtime.style_top=4;assert(atomic_file(file,&runtime,sizeof(runtime)));assert(!load_runtime_status(123,loaded,file));runtime.style_top=0;
    runtime.magic=0x3453544954505041ULL;assert(atomic_file(file,&runtime,sizeof(runtime)));assert(!load_runtime_status(123,loaded,file));
    runtime.magic=0;assert(atomic_file(file,&runtime,sizeof(runtime)));assert(!load_runtime_status(123,loaded,file));unlink(file);
    const char malformed[]="enabled=1\nposition=bottom\ncolor=ffffff\0suffix";
    fd=open(file,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0);
    assert(write(fd,malformed,sizeof(malformed)-1)==sizeof(malformed)-1);close(fd);
    assert(!load_config(reloaded,file));unlink(file);
    auto* memory=static_cast<unsigned char*>(mmap(nullptr,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(memory!=MAP_FAILED);
    auto* original=memory;auto* tramp=memory+512;
    // cmp edi,0; jl external-negative; mov eax,edi; add eax,7; nops; ret.
    const unsigned char function[]={0x83,0xff,0x00,0x7c,0x1b,0x89,0xf8,0x83,0xc0,0x07,0x90,0x90,0x90,0x90,0xc3};
    std::memcpy(original,function,sizeof(function));original[32]=0xb8;int neg=-99;std::memcpy(original+33,&neg,4);original[37]=0xc3;
    std::size_t used=0,written=0;assert(relocate(original,64,reinterpret_cast<std::uintptr_t>(original),reinterpret_cast<std::uintptr_t>(tramp),tramp,256,used,written));
    using Fn=int(*)(int);auto fn=reinterpret_cast<Fn>(tramp);assert(fn(12)==19);assert(fn(-1)==-99);
    // RIP-relative load must preserve its destination when it is representable.
    unsigned char rip[]={0x8b,0x05,0,0,0,0,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0xc3};
    int disp=128-6;std::memcpy(rip+2,&disp,4);std::memcpy(original,rip,sizeof(rip));int value=1234;std::memcpy(original+128,&value,4);
    assert(relocate(original,64,reinterpret_cast<std::uintptr_t>(original),reinterpret_cast<std::uintptr_t>(tramp),tramp,256,used,written));assert(reinterpret_cast<Fn>(tramp)(0)==1234);
    assert(!relocate(original,64,0x100000000ULL,0x700000000000ULL,tramp,256,used,written));
    // Unsupported short functions and branches into the displaced block are rejected.
    original[0]=0xc3;assert(!relocate(original,64,0x100000,0x200000,tramp,256,used,written));
    std::memset(original,0x90,64);original[0]=0xeb;original[1]=0;assert(!relocate(original,64,0x100000,0x200000,tramp,256,used,written));
    // Exact stat prefix from the PS5 log: a short patch moves only mov rax,
    // while the old 14-byte relocation would incorrectly move SYSCALL.
    const unsigned char native_stat[]={0x48,0xc7,0xc0,0xbc,0,0,0,0x49,0x89,0xca,0x0f,0x05,0x72,0x01,0xc3};
    std::memset(original,0x90,64);std::memcpy(original,native_stat,sizeof(native_stat));
    assert(!relocate(original,64,0x100000,0x200000,tramp,256,used,written));
    assert(relocate(original,64,0x100000,0x200000,tramp,256,used,written,5)&&used==7&&written==21);
    unsigned char rel[5]{};assert(relative_jump(rel,0x100000,0x200000)&&rel[0]==0xe9);
    assert(!relative_jump(rel,0x100000,0x700000000000ULL));
    munmap(memory,4096);puts("PASS: paths, settings persistence, real x86 conditional/RIP relocation and refusal cases");
}

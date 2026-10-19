// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the real installer; emulate only remote memory/ptrace operations.
// Generated x86 code is also executed with a live return address at patch time.
#include "app_title.hpp"
#include <array>
#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>
extern "C" {
#include "ps5/kernel.h"
#include "pt.h"
}
namespace {
using namespace app_title;
constexpr int target_pid=INT_MAX;
constexpr std::uint64_t original_auth=0x4800000000000006ULL;
constexpr std::uint64_t debug_auth=0x4800000000010003ULL;
struct Region {std::uintptr_t at;std::size_t size;int prot;};
std::vector<Region> regions;
std::vector<std::string> lines;
std::vector<std::uintptr_t> pcs;
std::uint64_t auth=original_auth;
unsigned attaches=0,detaches=0;
bool attached=false,thread_error=false,allocation_error=false,allocation_far=false;
std::vector<std::uintptr_t> live_relays;
unsigned allocations=0,unmaps=0;
std::uintptr_t fail_write=0;
unsigned char* memory=nullptr;
Request request{};
std::array<std::array<unsigned char,64>,kHooks> originals{};
unsigned during_installed=0;
bool install_during_call=false,during_result=false;
Region& region(std::uintptr_t at,std::size_t size) {
    for(auto& r:regions)if(at>=r.at&&at+size<=r.at+r.size)return r;
    assert(false&&"Access outside declared remote code");return regions[0];
}
bool has(const char* part) {for(const auto& line:lines)if(line.find(part)!=std::string::npos)return true;return false;}
using Fn=int(*)();
Fn function(std::uintptr_t at){return reinterpret_cast<Fn>(at);}
void result_code(std::uintptr_t at,int value) {
    auto* p=reinterpret_cast<unsigned char*>(at);p[0]=0xb8;std::memcpy(p+1,&value,4);p[5]=0xc3;
}
int callee() {
    if(install_during_call) {
        install_during_call=false;
        during_result=install_hooks(target_pid,request,during_installed);
    }
    return 73;
}
void call_code(unsigned slot,bool terminal,bool indirect=false) {
    const auto target=request.hooks[slot].target;
    auto* p=reinterpret_cast<unsigned char*>(target);
    std::memset(p,0x90,64);
    const unsigned char sub[]={0x48,0x83,0xec,0x08},add[]={0x48,0x83,0xc4,0x08};
    std::memcpy(p,sub,4);
    // Direct CALL ends at 14; indirect CALL ends at 15. Neither return
    // address is in the overwritten region. An early CALL returns at 9/10.
    const auto offset=terminal?9u:4u;
    const auto to=target+64;
    if(indirect) {
        p[offset]=0xff;p[offset+1]=0x15;
        const std::int32_t disp=static_cast<std::int32_t>((target+96)-(target+offset+6));
        std::memcpy(p+offset+2,&disp,4);std::memcpy(p+96,&to,8);
    } else {
        p[offset]=0xe8;
        const std::int32_t disp=static_cast<std::int32_t>(to-(target+offset+5));
        std::memcpy(p+offset+1,&disp,4);
    }
    const auto end=terminal&&indirect?15u:14u;
    std::memcpy(p+end,add,4);p[end+4]=0xc3;
    // Tail jump to a real C++ callee which runs the installer before return.
    p[64]=0x48;p[65]=0xb8;
    const auto callback=reinterpret_cast<std::uintptr_t>(&callee);
    std::memcpy(p+66,&callback,8);p[74]=0xff;p[75]=0xe0;
}
void snapshot() {for(unsigned i=0;i<kHooks;++i)std::memcpy(originals[i].data(),reinterpret_cast<void*>(request.hooks[i].target),64);}
void unchanged(unsigned slot){assert(!std::memcmp(originals[slot].data(),reinterpret_cast<void*>(request.hooks[slot].target),64));}
void clean() {
    assert(auth==original_auth&&!attached&&attaches==detaches);
    for(const auto& r:regions)assert(r.prot==(PROT_EXEC|PROT_READ));
}
void reset() {
    regions.clear();lines.clear();pcs={reinterpret_cast<std::uintptr_t>(memory)+8000};
    auth=original_auth;attaches=detaches=0;attached=thread_error=false;fail_write=0;
    allocation_error=allocation_far=false;live_relays.clear();allocations=unmaps=0;
    during_installed=0;install_during_call=during_result=false;
    std::memset(memory,0xcc,0x10000);
    request={};request.magic=kMagic;request.pid=target_pid;request.count=kHooks;
    for(unsigned i=0;i<kHooks;++i) {
        const auto base=reinterpret_cast<std::uintptr_t>(memory)+i*1024;
        request.hooks[i]={base,base+256,base+512};
        std::memset(reinterpret_cast<void*>(base),0x90,64);
        auto* p=reinterpret_cast<unsigned char*>(base);p[0]=0xb8;int value=10+i;std::memcpy(p+1,&value,4);p[14]=0xc3;
        result_code(base+256,91+i);
        regions.push_back({base,128,PROT_EXEC|PROT_READ});
        regions.push_back({base+256,64,PROT_EXEC|PROT_READ});
        regions.push_back({base+512,256,PROT_EXEC|PROT_READ});
    }
    snapshot();
}
}
namespace app_title {
void log_line(const char* text){lines.emplace_back(text);}
void log_fmt(const char* format,...) {
    char text[600];va_list ap;va_start(ap,format);vsnprintf(text,sizeof(text),format,ap);va_end(ap);log_line(text);
}
}
extern "C" {
std::uint64_t kernel_get_ucred_authid(pid_t){return auth;}
int kernel_set_ucred_authid(pid_t,std::uint64_t value){auth=value;return 0;}
int kernel_get_vmem_protection(pid_t pid,intptr_t at,std::size_t size){assert(pid==target_pid&&attached&&auth==debug_auth);return region(at,size).prot;}
int kernel_mprotect(pid_t pid,intptr_t at,std::size_t size,int prot){assert(pid==target_pid&&attached&&auth==debug_auth);region(at,size).prot=prot;return 0;}
int pt_attach(pid_t pid){assert(pid==target_pid&&!attached&&auth==debug_auth);attached=true;++attaches;return 0;}
int pt_detach(pid_t pid,int){assert(pid==target_pid&&attached);attached=false;++detaches;return 0;}
int app_title_pt_thread_count(pid_t pid){assert(pid==target_pid&&attached);return thread_error?-1:static_cast<int>(pcs.size());}
int app_title_pt_thread_list(pid_t pid,int* out,int count){assert(pid==target_pid&&attached&&count==static_cast<int>(pcs.size()));for(int i=0;i<count;++i)out[i]=100+i;return count;}
int pt_getregs(pid_t tid,struct reg* out){assert(attached&&tid>=100);out->r_rip=pcs.at(tid-100);return 0;}
int pt_copyout(pid_t pid,intptr_t at,void* out,std::size_t size) {
    assert(pid==target_pid&&attached&&(region(at,size).prot&PROT_READ));
    std::memcpy(out,reinterpret_cast<void*>(at),size);return 0;
}
int pt_copyin(pid_t pid,const void* data,intptr_t at,std::size_t size) {
    assert(pid==target_pid&&attached&&(region(at,size).prot&PROT_WRITE));
    if(fail_write==static_cast<std::uintptr_t>(at)){fail_write=0;std::memcpy(reinterpret_cast<void*>(at),data,2);return -1;}
    std::memcpy(reinterpret_cast<void*>(at),data,size);return 0;
}
intptr_t pt_mmap(pid_t pid,intptr_t,std::size_t size,int prot,int flags,int fd,off_t offset) {
    assert(pid==target_pid&&attached&&size==0x4000&&prot==(PROT_READ|PROT_WRITE));
    assert(!(flags&MAP_FIXED)&&fd==-1&&offset==0);++allocations;
    if(allocation_error)return -1;
    if(allocation_far)return 0x10000;
    const auto base=reinterpret_cast<std::uintptr_t>(memory)+allocations*0x4000;
    assert(base+size<=reinterpret_cast<std::uintptr_t>(memory)+0x10000);live_relays.push_back(base);
    regions.push_back({base,size,prot});return base;
}
int pt_munmap(pid_t pid,intptr_t at,std::size_t size) {
    assert(pid==target_pid&&attached&&size==0x4000);++unmaps;
    if(allocation_far){assert(at==0x10000);return 0;}
    bool found=false;for(auto i=live_relays.begin();i!=live_relays.end();++i)if(*i==static_cast<std::uintptr_t>(at)){live_relays.erase(i);found=true;break;}assert(found);
    for(auto i=regions.begin();i!=regions.end();++i)if(i->at==static_cast<std::uintptr_t>(at)){regions.erase(i);return 0;}
    assert(false);return -1;
}
}
int main() {
    memory=static_cast<unsigned char*>(mmap(nullptr,0x10000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(memory!=MAP_FAILED);
    unsigned installed=0;
    reset();assert(install_hooks(target_pid,request,installed)&&installed==kHooks&&live_relays.size()==kHooks);
    for(unsigned i=0;i<kHooks;++i){assert(function(request.hooks[i].target)()==static_cast<int>(91+i));assert(function(request.hooks[i].trampoline)()==static_cast<int>(10+i));}
    clean();
    // Both callbacks are required; a partial bootstrap may not become active.
    for(unsigned slot=0;slot<kRequiredHooks;++slot) {
        reset();*reinterpret_cast<unsigned char*>(request.hooks[slot].target)=0xc3;snapshot();
        assert(!install_hooks(target_pid,request,installed)&&installed==0&&live_relays.empty());
        for(unsigned i=0;i<kHooks;++i)unchanged(i);
        clean();
    }
    // Unsupported optional style callback keeps the proven ID/refresh pair.
    reset();*reinterpret_cast<unsigned char*>(request.hooks[2].target)=0xc3;snapshot();
    assert(install_hooks(target_pid,request,installed)&&installed==kRequiredHooks);
    unchanged(2);assert(function(request.hooks[0].target)()==91&&function(request.hooks[1].target)()==92);clean();
    // Unsafe early indirect CALL return is refused before any target write.
    reset();const unsigned char early[]={0x50,0xff,0xd7,0x90,0x90,0x59,0xc3};
    std::memcpy(reinterpret_cast<void*>(request.hooks[0].target),early,sizeof(early));snapshot();install_during_call=true;
    using Live=int(*)(int(*)());assert(reinterpret_cast<Live>(request.hooks[0].target)(&callee)==73&&!during_result&&during_installed==0);
    assert(has("live-call-return"));for(unsigned i=0;i<kHooks;++i)unchanged(i);
    clean();
    for(bool indirect:{false,true}) {
        reset();call_code(0,false,indirect);snapshot();install_during_call=true;
        assert(function(request.hooks[0].target)()==73&&during_result&&during_installed==kHooks);
        assert(function(request.hooks[0].target)()==91&&function(request.hooks[0].trampoline)()==73);clean();
    }
    // Existing Application.Update hooks (e.g. Common FPS) remain chained.
    reset();unsigned char chain[14]={0xff,0x25,0,0,0,0};const auto previous=request.hooks[1].target+80;
    result_code(previous,47);std::memcpy(chain+6,&previous,8);std::memcpy(reinterpret_cast<void*>(request.hooks[1].target),chain,14);
    assert(install_hooks(target_pid,request,installed)&&installed==kHooks&&function(request.hooks[1].trampoline)()==47);clean();
    reset();request.hooks[1].target=request.hooks[0].target;
    assert(!install_hooks(target_pid,request,installed)&&installed==0&&live_relays.empty());unchanged(0);clean();
    reset();pcs.push_back(request.hooks[0].target+1);
    assert(!install_hooks(target_pid,request,installed)&&installed==0&&live_relays.empty());
    for(unsigned i=0;i<kHooks;++i)unchanged(i);
    clean();
    reset();thread_error=true;assert(!install_hooks(target_pid,request,installed)&&installed==0&&live_relays.empty());
    for(unsigned i=0;i<kHooks;++i)unchanged(i);
    clean();
    for(unsigned slot=0;slot<kHooks;++slot)for(bool bridge:{false,true}) {
        reset();fail_write=bridge?request.hooks[slot].trampoline:request.hooks[slot].target;
        assert(!install_hooks(target_pid,request,installed)&&installed==0&&live_relays.empty());
        for(unsigned i=0;i<kHooks;++i)unchanged(i);
        clean();
    }
    for(bool far:{false,true}) {
        reset();allocation_error=!far;allocation_far=far;
        assert(!install_hooks(target_pid,request,installed)&&installed==0&&allocations==10&&live_relays.empty());
        assert(unmaps==(far?10u:0u));for(unsigned i=0;i<kHooks;++i)unchanged(i);
        clean();
    }
    munmap(memory,0x10000);
    puts("PASS: native registry/UI hooks and optional style callback, separate relays, existing Update chain, live CALL returns, no partial activation, thread checks, rollback and safe allocation failures");
}

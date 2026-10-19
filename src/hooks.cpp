// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include "relocate.hpp"
#include "hde64.h"
#include <csignal>
#include <climits>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
extern "C" {
#include <ps5/kernel.h>
#include "pt.h"
}
namespace app_title {
namespace {
bool address(std::uint64_t a){return a>=0x10000 && a<0x800000000000ULL;}
// The process is already stopped. Preserve the exact protection byte.
bool transfer(int pid,std::uintptr_t at,void* bytes,std::size_t size,bool writing) {
    const int prot=kernel_get_vmem_protection(pid,at,size);
    if(prot<0||prot>255||!(prot&PROT_EXEC))return false;
    const int want=prot|PROT_READ|(writing?PROT_WRITE:0);
    if(kernel_mprotect(pid,at,size,want)!=0)return false;
    int rc=-1;
    if(kernel_get_vmem_protection(pid,at,size)==want)
        rc=writing?pt_copyin(pid,bytes,at,size):pt_copyout(pid,at,bytes,size);
    const bool restored=kernel_mprotect(pid,at,size,prot)==0 && kernel_get_vmem_protection(pid,at,size)==prot;
    if(!restored)log_fmt("ERROR protection restore address=%lx original=%x",static_cast<unsigned long>(at),prot);
    return rc==0 && restored;
}
struct Patch {
    Hook hook{};std::uint8_t original[64]{},desired[64]{},trampoline[256]{};
    std::uintptr_t relay=0,near_trampoline=0;
    std::size_t consumed=0,written=0;unsigned slot=0;bool applied=false;
};
constexpr std::size_t relay_size=0x4000;
bool reachable(std::uintptr_t source,std::uintptr_t dest) {
    std::uint8_t bytes[5]{};return relative_jump(bytes,source,dest);
}
std::uintptr_t allocate_relay(int pid,std::uintptr_t target) {
    // mmap receives hints only. MAP_FIXED is never used: existing mappings
    // must not be replaced. Every returned address is checked before use.
    constexpr std::uint64_t offsets[]={0x1000000,0x4000000,0x10000000,0x20000000,0x40000000};
    const auto anchor=target&~(relay_size-1ULL);
    for(auto delta:offsets)for(unsigned side=0;side<2;++side) {
        if(side&&anchor<=delta+0x10000)continue;
        const auto hint=side?anchor-delta:anchor+delta;
        if(!address(hint)||!address(hint+relay_size))continue;
        const auto at=pt_mmap(pid,static_cast<intptr_t>(hint),relay_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(at<=0)continue;
        const bool near=address(at)&&address(at+relay_size)&&reachable(target,at);
        if(!near){(void)pt_munmap(pid,at,relay_size);continue;}
        if(kernel_mprotect(pid,at,relay_size,PROT_READ|PROT_WRITE|PROT_EXEC)!=0 ||
           kernel_get_vmem_protection(pid,at,relay_size)!=(PROT_READ|PROT_WRITE|PROT_EXEC)) {
            (void)pt_munmap(pid,at,relay_size);return 0;
        }
        return at;
    }
    return 0;
}
bool checked_write(int pid,std::uintptr_t at,void* data,std::size_t size) {
    std::uint8_t check[256]{};
    return size<=sizeof(check)&&transfer(pid,at,data,size,true)&&transfer(pid,at,check,size,false)&&!std::memcmp(check,data,size);
}
constexpr const char* names[kHooks]={"sceRegMgrGetInt","Application.Update","ioctl (HomeUI style)"};
void prologue(unsigned slot,const Patch& p) {
    char hex[65]{};
    for(unsigned i=0;i<24;++i)std::snprintf(hex+i*2,3,"%02x",p.original[i]);
    log_fmt("Hook candidate name=%s target=%lx bytes=%s",names[slot],static_cast<unsigned long>(p.hook.target),hex);
}
}
bool install_hooks(int pid,const Request& req,unsigned& installed) {
    installed=0;
    if(req.magic!=kMagic||req.pid!=pid||req.count!=kHooks||pid<=0||pid==getpid())return false;
    for(const auto& h:req.hooks)
        if(!address(h.target)||!address(h.replacement)||!address(h.trampoline) || h.target==h.replacement || h.target==h.trampoline)return false;
    const auto auth=kernel_get_ucred_authid(getpid());
    if(auth==0||auth==static_cast<std::uint64_t>(-1))return false;
    if(kernel_set_ucred_authid(getpid(),0x4800000000010003ULL)!=0)return false;
    if(kernel_get_ucred_authid(getpid())!=0x4800000000010003ULL){kernel_set_ucred_authid(getpid(),auth);return false;}
    bool attached=pt_attach(pid)==0,ok=attached;
    Patch patches[kHooks]{};unsigned count=0,mask=0,skipped=0;
    std::uintptr_t relays[kHooks]{};bool rolled_back=true,committed=false;
    if(attached) {
        for(unsigned i=0;ok&&i<kHooks;++i) {
            for(unsigned j=0;j<count;++j)if(req.hooks[i].target==patches[j].hook.target) {
                log_line("Native callbacks cannot share a target");ok=false;break;
            }
            if(!ok)break;
            auto& p=patches[count];p=Patch{};p.hook=req.hooks[i];p.slot=i;
            relays[i]=allocate_relay(pid,p.hook.target);
            if(!relays[i]){log_fmt("No reachable relay name=%s; targets left unchanged",names[i]);ok=false;break;}
            p.relay=relays[i];p.near_trampoline=p.relay+32;
            if(!transfer(pid,p.hook.target,p.original,sizeof(p.original),false)) {
                log_fmt("Hook memory read failed name=%s",names[i]);ok=false;break;
            }
            prologue(i,p);
            if(!relocate(p.original,sizeof(p.original),p.hook.target,p.near_trampoline,p.trampoline,sizeof(p.trampoline),p.consumed,p.written,5)) {
                log_fmt("Hook skipped name=%s reason=unsupported-prologue",names[i]);++skipped;continue;
            }
            bool safe=true;
            if(std::memcmp(p.original,"\xff\x25\0\0\0\0",6)) {
                for(std::size_t at=0;at<p.consumed;) {
                    hde64s d{};hde64_disasm(p.original+at,&d);
                    // A stopped thread may be inside a callee with its return
                    // address in the overwritten bytes. A terminal call is
                    // safe: its return address is at the untouched boundary.
                    const bool call=d.opcode==0xe8 || (d.opcode==0xff&&(d.modrm_reg==2||d.modrm_reg==3));
                    if(!d.len || (call&&at+d.len<p.consumed)) {
                        safe=false;
                        log_fmt("Hook skipped name=%s reason=live-call-return offset=%zu displaced=%zu",names[i],at,p.consumed);
                        break;
                    }
                    at+=d.len;
                }
            }
            if(!safe){++skipped;continue;}
            for(unsigned j=0;j<count;++j) {
                const auto& other=patches[j];
                if(p.hook.target<other.hook.target+other.consumed&&other.hook.target<p.hook.target+p.consumed) {
                    log_line("Overlapping native hook ranges; installation refused");ok=false;break;
                }
            }
            if(!ok)break;
            std::memcpy(p.desired,p.original,sizeof(p.desired));
            if(!relative_jump(p.desired,p.hook.target,p.relay)){ok=false;break;}
            for(std::size_t k=5;k<p.consumed;++k)p.desired[k]=0x90;
            std::uint8_t relay[14]{},bridge[14]{};
            jump(relay,p.hook.replacement);jump(bridge,p.near_trampoline);
            if(!checked_write(pid,p.near_trampoline,p.trampoline,p.written)||!checked_write(pid,p.relay,relay,sizeof(relay))||
               !checked_write(pid,p.hook.trampoline,bridge,sizeof(bridge))){ok=false;break;}
            log_fmt("Hook prepared name=%s patch=rel32 displaced=%zu trampoline=%zu",names[i],p.consumed,p.written);
            ++count;mask|=1u<<i;
        }
        // The registry override and UI-thread refresh must both be prepared
        // before either native function is changed.
        if(ok&&(mask&3u)!=3u){log_line("Registry and UI-thread hooks required; native targets left unchanged");ok=false;}
        if(ok)for(const auto at:relays)if(at&&(kernel_mprotect(pid,at,relay_size,PROT_READ|PROT_EXEC)!=0 ||
           kernel_get_vmem_protection(pid,at,relay_size)!=(PROT_READ|PROT_EXEC))){ok=false;break;}
        if(ok) {
            int threads[2048]{};
            const int number=app_title_pt_thread_count(pid);
            const int got=(number>0&&number<=2048)?app_title_pt_thread_list(pid,threads,number):-1;
            if(got!=number||got<=0){log_line("Cannot enumerate stopped ShellUI threads; hook refused");ok=false;}
            for(int i=0;ok&&i<got;++i) {
                struct reg registers{};
                if(pt_getregs(threads[i],&registers)!=0){ok=false;log_line("Cannot read stopped thread registers; hook refused");break;}
                for(unsigned j=0;j<count;++j) {
                    const auto& p=patches[j];
                    const auto pc=static_cast<std::uint64_t>(registers.r_rip);
                    if(pc>p.hook.target&&pc<p.hook.target+p.consumed) {
                        log_fmt("Thread is executing a displaced prologue name=%s; hook refused",names[p.slot]);ok=false;break;
                    }
                }
            }
        }
        if(ok)for(unsigned i=0;i<count;++i) {
            auto& p=patches[i];std::uint8_t check[64]{};
            if(!transfer(pid,p.hook.target,check,p.consumed,false)||std::memcmp(check,p.original,p.consumed)){ok=false;break;}
            // Mark before writing: even a partial failed write must be rolled back.
            p.applied=true;
            if(!transfer(pid,p.hook.target,p.desired,p.consumed,true)||
               !transfer(pid,p.hook.target,check,p.consumed,false)||std::memcmp(check,p.desired,p.consumed)){ok=false;break;}
            log_fmt("Hook applied name=%s verified=1",names[p.slot]);
        }
        committed=ok;
        if(!ok)for(unsigned i=count;i>0;--i) {
            auto& p=patches[i-1];if(!p.applied)continue;
            std::uint8_t check[64]{};
            const bool rollback=transfer(pid,p.hook.target,p.original,p.consumed,true) &&
                transfer(pid,p.hook.target,check,p.consumed,false) && !std::memcmp(check,p.original,p.consumed);
            log_fmt("Hook rollback target=%lx verified=%d",static_cast<unsigned long>(p.hook.target),rollback?1:0);
            rolled_back=rolled_back&&rollback;
        }
        if(!committed&&rolled_back)for(const auto at:relays)if(at)(void)pt_munmap(pid,at,relay_size);
        const bool detached=pt_detach(pid,0)==0;
        (void)kill(pid,SIGCONT);ok=ok&&detached;
    }
    const bool restored=kernel_set_ucred_authid(getpid(),auth)==0 && kernel_get_ucred_authid(getpid())==auth;
    ok=ok&&restored&&count>0;if(ok)installed=count;
    log_fmt("Native hooks pid=%d count=%u skipped=%u mask=0x%x attached=%d installed=%d auth_restored=%d",pid,count,skipped,ok?mask:0,attached?1:0,ok?1:0,restored?1:0);
    return ok;
}
}

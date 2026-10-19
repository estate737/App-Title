// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
namespace app_title {
bool valid_id(const char* text, std::size_t size) {
    if (!text || size != 9) return false;
    for (unsigned i=0; i<4; ++i) if (text[i]<'A' || text[i]>'Z') return false;
    for (unsigned i=4; i<9; ++i) if (text[i]<'0' || text[i]>'9') return false;
    return true;
}
bool redirect_path(const char* p, char* out, std::size_t cap) {
    if (!p || !out) return false;
    const char* roots[] = {"/user/appmeta/", "/system_data/priv/mms/appmeta/"};
    for (const char* root : roots) {
        const auto n=std::strlen(root);
        if (std::strncmp(p,root,n)!=0) continue;
        const char* id=p+n;
        if (std::strlen(id)!=19 || !valid_id(id,9) || std::strcmp(id+9,"/icon0.png")) return false;
        const int len=std::snprintf(out,cap,"%s/%.9s.png",kCache,id);
        return len>0 && static_cast<std::size_t>(len)<cap;
    }
    return false;
}
bool atomic_file(const char* path,const void* data,std::size_t size) {
    char tmp[1024];
    if (std::snprintf(tmp,sizeof(tmp),"%s.tmp",path)>=static_cast<int>(sizeof(tmp))) return false;
    const int fd=open(tmp,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0644);
    if (fd<0) return false;
    std::size_t n=0;
    while(n<size) {
        const auto rc=write(fd,static_cast<const char*>(data)+n,size-n);
        if(rc<0 && errno==EINTR)continue;
        if(rc<=0)break;
        n+=static_cast<std::size_t>(rc);
    }
    bool ok=n==size && fsync(fd)==0;
    if(close(fd)!=0)ok=false;
    if(ok)ok=rename(tmp,path)==0;
    if(!ok)unlink(tmp);
    return ok;
}
}

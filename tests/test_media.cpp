// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include "media_launcher.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "ps5/kernel.h"

namespace fs=std::filesystem;
namespace {
constexpr std::uint64_t original_auth=0x4800000000010003ULL;
constexpr std::uint64_t installer_auth=0x4800000000000006ULL;
std::uint64_t auth=original_auth;
bool user_loaded=false,installer_loaded=false,present=false,install_error=false,restore_error=false;
unsigned installs=0,user_init_count=0,app_init_count=0;
std::vector<std::string> lines;
int user_init(const void*){assert(auth==original_auth);++user_init_count;return 0;}
int app_init(){assert(user_init_count>0&&auth==installer_auth);++app_init_count;return 0;}
int exists(const char* id,int* out){assert(!std::strcmp(id,"APID00001")&&auth==installer_auth);*out=present?1:0;return 0;}
int install(const char* id,const char* base,void*) {
    assert(auth==installer_auth&&!std::strcmp(id,"APID00001"));
    assert(std::string(base)=="./build-host/media/");
    const auto p=fs::path(base)/id/"sce_sys/param.json";
    std::ifstream f(p);std::string data((std::istreambuf_iterator<char>(f)),{});
    assert(data.find("APID00001")!=std::string::npos);
    assert(fs::file_size(fs::path(base)/id/"sce_sys/icon0.png")>100);
    ++installs;if(install_error)return -321;present=true;return 0;
}
bool has(const char* text){for(const auto& line:lines)if(line.find(text)!=std::string::npos)return true;return false;}
}
namespace app_title {
void log_line(const char* text){lines.emplace_back(text);}
void log_fmt(const char* format,...) {
    char text[600];va_list ap;va_start(ap,format);vsnprintf(text,sizeof(text),format,ap);va_end(ap);log_line(text);
}
}
extern "C" {
int kernel_dynlib_handle(pid_t,const char* name,std::uint32_t* out) {
    if(!std::strcmp(name,"libSceUserService.sprx")&&user_loaded){*out=11;return 0;}
    if(!std::strcmp(name,"libSceAppInstUtil.sprx")&&installer_loaded){*out=14;return 0;}
    return -1;
}
intptr_t kernel_dynlib_dlsym(pid_t,std::uint32_t handle,const char* name) {
    if(handle==11&&!std::strcmp(name,"sceUserServiceInitialize"))return reinterpret_cast<intptr_t>(&user_init);
    if(handle==14&&!std::strcmp(name,"sceAppInstUtilInitialize"))return reinterpret_cast<intptr_t>(&app_init);
    if(handle==14&&!std::strcmp(name,"sceAppInstUtilAppExists"))return reinterpret_cast<intptr_t>(&exists);
    if(handle==14&&!std::strcmp(name,"sceAppInstUtilAppInstallTitleDir"))return reinterpret_cast<intptr_t>(&install);
    return 0;
}
std::uint64_t kernel_get_ucred_authid(pid_t){return auth;}
int kernel_set_ucred_authid(pid_t,std::uint64_t value){if(restore_error&&value==original_auth)return -1;auth=value;return 0;}
int sceKernelLoadStartModule(const char* path,std::size_t,const void*,unsigned,void*,int* result) {
    assert(auth==original_auth);*result=0;
    if(std::strstr(path,"UserService")){user_loaded=true;return 11;}
    if(std::strstr(path,"AppInstUtil")){installer_loaded=true;return 14;}
    return -1;
}
}
int main() {
    fs::remove_all("build-host/media");fs::create_directories("build-host/media");
    assert(app_title::ps5::ensure_media_launcher(0x11600005));
    assert(present&&installs==1&&auth==original_auth&&app_init_count==1);
    assert(has("media.module.native_load.end")&&has("media.install.end rc=0x0")&&has("auth_restored=1"));
    assert(app_title::ps5::ensure_media_launcher(0x11600005));assert(installs==1);
    // A failed install may not leave elevated credentials behind.
    present=false;install_error=true;
    assert(app_title::ps5::ensure_media_launcher(0x11600005));assert(auth==original_auth&&!present);
    install_error=false;
    // Recover our interrupted first staging, but never overwrite a foreign app.
    fs::remove("build-host/media/APID00001/sce_sys/param.json");
    assert(app_title::ps5::ensure_media_launcher(0x11600005));assert(present);
    std::ofstream("build-host/media/APID00001/sce_sys/param.json")<<"foreign application";
    const auto before=installs;
    assert(app_title::ps5::ensure_media_launcher(0x11600005));assert(installs==before&&has("media.assets.refused"));
    // The controller must be told to stop if restoring its identity failed.
    fs::remove_all("build-host/media/APID00001");present=false;restore_error=true;
    assert(!app_title::ps5::ensure_media_launcher(0x11600005));
    puts("PASS: actual media registrar with mock PS5 APIs: cold load, initialization order, registration, retry, collision guard and credential restoration");
    fs::remove_all("build-host/media");
}

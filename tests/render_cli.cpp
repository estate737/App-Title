// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include <cstring>
#include <cstdlib>
int main(int n,char** v) {
    if(n!=7)return 2;
    app_title::Config c{};c.top=!std::strcmp(v[4],"top");c.color=static_cast<unsigned>(std::strtoul(v[5],nullptr,16));
    return app_title::render_icon(v[1],v[2],v[3],c,v[6])?0:1;
}

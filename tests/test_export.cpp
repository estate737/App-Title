// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
int main() {
    using namespace app_title;
    assert(!std::strcmp(style_export_state(),"idle"));
    assert(request_style_export()&&!request_style_export());
    assert(!std::strcmp(style_export_state(),"queued"));
    run_style_export();assert(!std::strcmp(style_export_state(),"failed"));
    run_style_export();assert(!std::strcmp(style_export_state(),"failed"));
    assert(request_style_export());run_style_export();
    puts("PASS: manual export queue, duplicate refusal, idle loop and retry after failed result");
}

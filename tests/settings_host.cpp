// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_title.hpp"
#include <sys/stat.h>
#include <unistd.h>
int main() {
    mkdir("build-host",0755);
    if(!app_title::prepare_data_directory())return 1;
    app_title::Config c{};app_title::load_config(c);
    app_title::Stats s{};s.phase="Starting";s.shell_pid=-12345;
    s.queries=7;s.overrides=5;s.reloads=1;s.reload_errors=0;s.hooks=3;
    s.style_state=static_cast<unsigned>(app_title::StyleState::applied);s.style_patches=1;
    if(!app_title::start_settings(c,s))return 1;
    for(;;)usleep(100000);
}

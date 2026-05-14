#include "internal.h"

void main_display_menu_request_apply(main_display_menu_t* menu)
{
    if (menu->display_index < 0 || menu->display_index >= menu->display_count ||
        menu->mode_index < 0 || menu->mode_index >= menu->mode_count)
    {
        return;
    }
    menu->apply_requested = true;
    main_display_menu_close(menu);
}

void main_display_menu_commit_if_requested(main_display_menu_t* menu, const main_display_menu_runtime_t* runtime)
{
    if (!menu->apply_requested)
    {
        return;
    }
    menu->apply_requested = false;
    app_display_internal::commit_display_menu(menu, runtime);
}

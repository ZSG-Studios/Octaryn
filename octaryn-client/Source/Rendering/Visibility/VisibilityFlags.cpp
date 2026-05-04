#include "VisibilityFlags.h"

#include <cstdlib>
#include <cstring>

namespace {

auto ascii_lower(char value) -> char
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

auto ascii_equal_ignore_case(const char* left, const char* right) -> bool
{
    if (left == nullptr || right == nullptr)
    {
        return left == right;
    }

    while (*left != '\0' && *right != '\0')
    {
        if (ascii_lower(*left) != ascii_lower(*right))
        {
            return false;
        }
        ++left;
        ++right;
    }

    return *left == '\0' && *right == '\0';
}

} // namespace

int visibility_env_flag_enabled(const char* name)
{
    if (name == nullptr || name[0] == '\0')
    {
        return 0;
    }

    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
    {
        return 0;
    }

    if (std::strcmp(value, "0") == 0 ||
        ascii_equal_ignore_case(value, "false") ||
        ascii_equal_ignore_case(value, "off"))
    {
        return 0;
    }

    return 1;
}

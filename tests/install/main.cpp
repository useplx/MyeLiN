#include <mln/runtime/runtime.hpp>

int main() {
    Mln::Runtime runtime;
    return runtime.devices().empty() ? 1 : 0;
}

#include "mln/artifact.hpp"
#include "mln/audio/dsp.hpp"
#include "mln/buffer.hpp"
#include "mln/dtype.hpp"
#include "mln/package.hpp"
#include "mln/runtime/runtime.hpp"
#include "mln/sdk/backend/compatibility.hpp"
#include "mln/sdk/driver/module.hpp"
#include "mln/sdk/functional.hpp"
#include "mln/sdk/lowering/compatibility.hpp"
#include "mln/sdk/native.hpp"
#include "mln/sdk/operation/compatibility.hpp"
#include "mln/weight.hpp"

int main() {
    Mln::Buffer buffer(1);
    Mln::Runtime runtime;
    auto session = runtime.session();
    return buffer.size() == 1 ? 0 : 1;
}

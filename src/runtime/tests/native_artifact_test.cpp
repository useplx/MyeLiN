#include "mln/package.hpp"
#include "mln/runtime/metadata.hpp"
#include "mln/runtime/runtime.hpp"
#include "mln/sdk/native/abi.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 3)
        throw std::invalid_argument("native_artifact_test requires provider and artifact paths");

    std::ifstream provider(argv[1], std::ios::binary);
    if (!provider)
        throw std::runtime_error("failed to open native provider fixture");
    std::vector<std::uint8_t> library(std::istreambuf_iterator<char>(provider), {});

    Mln::Package::Builder builder;
    builder.add(Mln::Package::Section::Type::Native, "echo", std::move(library));
    const std::string entrypoint = "echo\tnative\tentrypoint\tforward\n";
    builder.add(Mln::Package::Section::Type::ApplicationEntrypoints, "public",
                std::vector<std::uint8_t>(entrypoint.begin(), entrypoint.end()));
    builder.add(Mln::Package::Section::Type::Requirements, "runtime",
                Mln::encode_requirements({.runtime_abi = 1,
                                          .native_abi = Mln::Sdk::Native::ABI_VERSION,
                                          .driver_abi = 0,
                                          .platform = Mln::current_platform()}));
    builder.write(argv[2]);

    Mln::Runtime runtime;
    auto session = runtime.session();
    session.load(argv[2]);
    Mln::Buffer input(4);
    const std::vector<std::byte> expected{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    input.write(expected);
    std::vector<std::byte> actual;
    session.invoke("echo", input, [&](std::span<const std::uint8_t> bytes) {
        actual.assign(reinterpret_cast<const std::byte *>(bytes.data()),
                      reinterpret_cast<const std::byte *>(bytes.data() + bytes.size()));
    });
    if (actual != expected || !session.devices().empty())
        throw std::runtime_error("native-only artifact did not round-trip its input");
}

#include "mln/package.hpp"
#include "mln/package_errors.hpp"
#include "mln/runtime/metadata.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc != 2)
        throw std::invalid_argument("package_test requer output.mln");
    using namespace Mln;
    Package::Builder builder;
    const std::vector<uint8_t> expected{1, 2, 3, 4};
    builder.add(Package::Section::Type::Asset, "tokenizer.json", expected);
    builder.add(Package::Section::Type::Driver, "qnn", expected);
    builder.add(Package::Section::Type::Vmfb, "vision", expected);
    builder.add(Package::Section::Type::Vmfb, "decode", expected);
    const std::string signature =
        "model.forward\t@decode::model.forward__specialization0\tf16:1,-1\n"
        "model.forward\t@model.forward__specialization1\tf16:8,-1\n";
    builder.add(Package::Section::Type::Entrypoints, "typed",
                std::vector<uint8_t>(signature.begin(), signature.end()));
    const std::string application = "chat\twasm\torchestration\tforward\n"
                                    "classify\tgraph\tmodel\tmodel.forward\n";
    builder.add(Package::Section::Type::ApplicationEntrypoints, "public",
                std::vector<uint8_t>(application.begin(), application.end()));
    const std::string hook = "chat\trag:augmentation\tforward\n";
    builder.add(Package::Section::Type::Hooks, "routes",
                std::vector<uint8_t>(hook.begin(), hook.end()));
    builder.add_optional(4096, "future", expected);
    builder.add(
        Package::Section::Type::Requirements, "runtime",
        encode_requirements(
            {.runtime_abi = 1, .native_abi = 6, .driver_abi = 3, .platform = current_platform()}));
    builder.write(argv[1]);

    const auto package = Package::File::open(argv[1]);
    const Package::Section asset =
        package->required(Package::Section::Type::Asset, "tokenizer.json");
    if (asset.bytes.size() != expected.size() ||
        !std::equal(asset.bytes.begin(), asset.bytes.end(), expected.begin()))
        throw std::runtime_error("mapped asset differs from the written bytes");
    package->discard(asset.bytes);
    if (!std::equal(asset.bytes.begin(), asset.bytes.end(), expected.begin()))
        throw std::runtime_error("descarte invalidou a view mapeada");
    if (package->sections(Package::Section::Type::Asset).size() != 1)
        throw std::runtime_error("incorrect asset count");
    if (package->sections(Package::Section::Type::Vmfb).size() != 2)
        throw std::runtime_error("incorrect modular VMFB count");
    const Package::Section driver = package->required(Package::Section::Type::Driver, "qnn");
    if (driver.bytes.size() != expected.size() ||
        !std::equal(driver.bytes.begin(), driver.bytes.end(), expected.begin()))
        throw std::runtime_error("driver differs from the written bytes");
    const Package::Section entrypoints =
        package->required(Package::Section::Type::Entrypoints, "typed");
    if (std::string(entrypoints.bytes.begin(), entrypoints.bytes.end()) != signature)
        throw std::runtime_error("typed signatures differ in the package");
    const auto decoded = decode_signatures(entrypoints.bytes);
    if (decoded.size() != 2 || decoded[0].module != "decode" || decoded[1].module != "module")
        throw std::runtime_error("modular signature routing differs");
    const Package::Section public_entrypoints =
        package->required(Package::Section::Type::ApplicationEntrypoints, "public");
    if (std::string(public_entrypoints.bytes.begin(), public_entrypoints.bytes.end()) !=
        application)
        throw std::runtime_error("public entrypoints differ in the package");
    const auto public_decoded = decode_application_entrypoints(public_entrypoints.bytes);
    if (public_decoded.size() != 2 || public_decoded[1].runtime != Artifact::Runtime::Graph)
        throw std::runtime_error("entrypoint domain differs in the package");
    const Package::Section hooks = package->required(Package::Section::Type::Hooks, "routes");
    if (std::string(hooks.bytes.begin(), hooks.bytes.end()) != hook)
        throw std::runtime_error("hook routes differ in the package");
    const auto future = package->sections(static_cast<Package::Section::Type>(4096));
    if (future.size() != 1 || !future.front().optional || future.front().raw_type != 4096)
        throw std::runtime_error("future optional section was not preserved");
    const Artifact::Requirements requirements = decode_requirements(
        package->required(Package::Section::Type::Requirements, "runtime").bytes);
    if (requirements.runtime_abi != 1 || requirements.native_abi != 6 ||
        requirements.driver_abi != 3 || requirements.platform != current_platform())
        throw std::runtime_error("requisitos do artefato divergiram");
    const Artifact::Metadata metadata = read_artifact_metadata(argv[1]);
    if (metadata.requirements.native_abi != 6 || metadata.entrypoints.size() != 2 ||
        metadata.signatures.size() != 2 || metadata.signatures.front().inputs.size() != 1 ||
        metadata.signatures.front().inputs.front().dtype != "f16" ||
        metadata.signatures.front().inputs.front().shape != std::vector<std::int64_t>({1, -1}) ||
        metadata.sections.size() != package->sections().size())
        throw std::runtime_error("metadata publica perdeu informacao do artefato");

    const std::filesystem::path corrupt = std::string(argv[1]) + ".corrupt";
    std::filesystem::copy_file(argv[1], corrupt, std::filesystem::copy_options::overwrite_existing);
    {
        // Header is 64 bytes; the first record offset field starts 8 bytes later.
        std::fstream stream(corrupt, std::ios::binary | std::ios::in | std::ios::out);
        stream.seekp(64 + 8);
        const std::uint64_t metadata_offset = 0;
        stream.write(reinterpret_cast<const char *>(&metadata_offset), sizeof(metadata_offset));
    }
    try {
        (void)Package::File::open(corrupt.string());
        throw std::runtime_error("metadata-overlapping section was accepted");
    } catch (const Diagnostic &diagnostic) {
        if (diagnostic.error() != Package::Errors::Section::Invalid)
            throw;
    }
    std::filesystem::remove(corrupt);

    const std::filesystem::path mandatory = std::string(argv[1]) + ".mandatory";
    std::filesystem::copy_file(argv[1], mandatory,
                               std::filesystem::copy_options::overwrite_existing);
    {
        // The future extension is the eighth record. Clearing its optional bit
        // must make an older runtime reject the artifact instead of guessing.
        std::fstream stream(mandatory, std::ios::binary | std::ios::in | std::ios::out);
        constexpr std::streamoff flags = 64 + 7 * 80 + 4;
        stream.seekp(flags);
        const std::uint32_t required = 0;
        stream.write(reinterpret_cast<const char *>(&required), sizeof(required));
    }
    try {
        (void)Package::File::open(mandatory.string());
        throw std::runtime_error("future required section was accepted");
    } catch (const Diagnostic &diagnostic) {
        if (diagnostic.error() != Package::Errors::Section::Invalid)
            throw;
    }
    std::filesystem::remove(mandatory);
}

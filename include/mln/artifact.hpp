#pragma once

#include "mln/package.hpp"
#include "mln/weight.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Mln::Artifact {
    struct Requirements {
            std::uint32_t runtime_abi = 1;
            std::uint32_t native_abi = 0;
            std::uint32_t driver_abi = 0;
            std::string platform;
    };
    struct File {
            std::string name;
            std::string path;
    };

    enum class Runtime {
        Graph,
        Wasm,
        Native,
        Driver,
    };

    struct Module {
            Runtime runtime;
            File artifact;
    };

    struct Entrypoint {
            std::string name;
            std::string module;
            std::string export_name;
            Runtime runtime = Runtime::Wasm;
    };

    struct Hook {
            std::string path;
            std::string module;
            std::string export_name;
    };

    struct Input {
            std::string dtype;
            std::vector<std::int64_t> shape;
    };

    struct Signature {
            std::string path;
            std::string module = "module";
            std::string symbol;
            std::vector<Input> inputs;
    };

    struct Section {
            Package::Section::Type type;
            std::string name;
            std::uint64_t bytes = 0;
            std::uint32_t raw_type = 0;
            bool optional = false;
    };

    struct Metadata {
            std::string path;
            std::uint64_t file_bytes = 0;
            std::uint32_t format_version = 0;
            std::vector<Section> sections;
            std::vector<std::string> devices;
            std::vector<std::string> backends;
            std::vector<Entrypoint> entrypoints;
            std::vector<Hook> hooks;
            std::vector<Signature> signatures;
            std::vector<std::pair<std::string, std::uint32_t>> deployment;
            std::vector<Weight::Descriptor> parameters;
            std::uint64_t parameter_bytes = 0;
            std::uint64_t parameter_section_bytes = 0;
            Requirements requirements;
    };

    struct Plan {
            std::vector<Module> modules;
            std::vector<File> assets;
            std::vector<Entrypoint> entrypoints;
            std::vector<Hook> hooks;
    };
}

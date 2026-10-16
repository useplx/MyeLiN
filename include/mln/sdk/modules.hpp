#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "type_token.hpp"

namespace Mln::Sdk {
    class Module;

    class Modules {
        private:
            struct Entry {
                    std::unique_ptr<Module> value;
                    const void *type;
            };

            Module *parent;
            std::map<std::string, Entry, std::less<>> modules;

            void insert_typed(std::string name, std::unique_ptr<Module> child, const void *type);
            Module *at_typed(std::string_view name, const void *type);

        public:
            explicit Modules(Module *parent);
            ~Modules();

            void insert(std::string name, std::unique_ptr<Module> child);

            template <typename T, typename... Args> void insert(std::string name, Args &&...args) {
                insert_typed(
                    std::move(name),
                    std::unique_ptr<Module>(std::make_unique<T>(std::forward<Args>(args)...)),
                    Type::type_token<T>());
            }

            Module *at(std::string_view name);

            template <typename T> T *at(std::string_view name) {
                return static_cast<T *>(at_typed(name, Type::type_token<T>()));
            }

            template <typename T, typename... Args>
            auto forward(std::string_view name, Args &&...args);

            auto begin() { return modules.begin(); }
            auto end() { return modules.end(); }

            void erase(std::string_view name);
            void clear();
    };
}

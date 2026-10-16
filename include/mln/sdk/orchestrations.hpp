#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "type_token.hpp"

namespace Mln::Sdk {
    class Orchestration;

    class Orchestrations {
        private:
            struct Entry {
                    std::unique_ptr<Orchestration> value;
                    const void *type;
            };

            std::map<std::string, Entry, std::less<>> orchestrations;

            void insert_typed(std::string name, std::unique_ptr<Orchestration> child,
                              const void *type);
            Orchestration *at_typed(std::string_view name, const void *type);

        public:
            void insert(std::string name, std::unique_ptr<Orchestration> child);

            template <typename T, typename... Args> void insert(std::string name, Args &&...args) {
                insert_typed(std::move(name),
                             std::unique_ptr<Orchestration>(
                                 std::make_unique<T>(std::forward<Args>(args)...)),
                             Type::type_token<T>());
            }

            Orchestration *at(std::string_view name);

            template <typename T> T *at(std::string_view name) {
                return static_cast<T *>(at_typed(name, Type::type_token<T>()));
            }

            auto begin() { return orchestrations.begin(); }
            auto end() { return orchestrations.end(); }

            void erase(std::string_view name);
            void clear();
    };
}

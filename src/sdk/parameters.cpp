#include "mln/error.hpp"
#include "mln/sdk/errors/sdk.hpp"
#include <mln/sdk/functional/const.hpp>
#include <mln/sdk/module.hpp>
#include <mln/sdk/operations.hpp>
#include <mln/sdk/parameters.hpp>

#include <format>

namespace Mln::Sdk {

    Parameters::Parameters(Module &owner_value) : owner(&owner_value) {}

    void Parameters::insert(std::string name, Weight weight) {
        weight.name = name;
        parameters.insert_or_assign(std::move(name), std::move(weight));
    }

    Tensor Parameters::at(std::string_view name) {
        const auto found = parameters.find(name);
        if (found == parameters.end())
            raise(Sdk::Errors::Parameter::Missing, name);
        Weight weight = found->second;
        weight.name = owner->path();
        weight.name += '.';
        weight.name.append(name);
        return owner->operations.insert<Functional::Const>(weight, true);
    }
}

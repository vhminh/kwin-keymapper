#include "argparse.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <format>
#include <ostream>
#include <variant>

bool starts_with_double_dash(const char* arg) {
    return std::strncmp(arg, "--", 2) == 0;
}

ArgParser::ArgParser(std::string_view program) : program(program) {}

ArgParser& ArgParser::add_option(ArgDef def) {
    this->option_defs[def.name] = def;
    return *this;
}

ArgParser& ArgParser::add_positional_argument(ArgDef def) {
    if (def.required && !this->positional_arg_names.empty() &&
        this->positional_arg_defs[this->positional_arg_names.back()].required == false) {
        throw std::runtime_error(
            std::format(
                "cannot add positional argument {}, optional positional args are only allowed at the end", def.name
            )
        );
    }
    this->positional_arg_defs[def.name] = def;
    this->positional_arg_names.push_back(def.name);
    return *this;
}

bool ArgParser::should_print_help(int argc, const char* argv[]) const {
    return argc == 2 && (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0);
}

void ArgParser::print_help(std::ostream& os) const {
    // USAGE
    os << std::format("Usage: {}", this->program);
    std::vector<ArgDef> required_options;
    std::vector<ArgDef> non_required_options;
    for (const auto& p : this->option_defs) {
        const auto& def = p.second;
        if (def.required)
            required_options.push_back(def);
        else
            non_required_options.push_back(def);
    }
    if (!non_required_options.empty()) {
        os << " [OPTIONS]";
    }
    for (const ArgDef& def : required_options) {
        switch (def.type) {
        case ArgType::BOOL:
            os << std::format(" {}", def.name);
            break;
        case ArgType::STRING:
            os << std::format(" {} str", def.name);
            break;
        case ArgType::INT:
            os << std::format(" {} N", def.name);
            break;
        }
    }
    for (const auto& name : this->positional_arg_names) {
        const auto& def = this->positional_arg_defs.find(name)->second;
        if (def.required) {
            os << std::format(" <{}>", def.name);
        } else {
            os << std::format(" [{}]", def.name);
        }
    }
    os << std::endl;
    os << std::endl;
    // POSITIONAL ARGS
    if (!this->positional_arg_defs.empty()) {
        os << "Positional arguments:\n";
        for (const auto& name : this->positional_arg_names) {
            const auto& def = this->positional_arg_defs.find(name)->second;
            os << std::format("  {}\t\t{}\n", def.name, def.desc);
        }
        os << std::endl;
    }
    // REQUIRED OPTIONS
    if (!required_options.empty()) {
        os << "Required options:\n";
        for (const ArgDef& def : required_options) {
            os << std::format("  {}\t\t{}\n", def.name, def.desc);
        }
        os << std::endl;
    }
    // OPTIONS
    if (!non_required_options.empty()) {
        os << "Options:\n";
        for (const ArgDef& def : non_required_options) {
            os << std::format("  {}\t\t{}\n", def.name, def.desc);
        }
        os << std::endl;
    }
    // EXAMPLE
    os << std::format("Example: {}", this->program);
    for (const auto& p : this->option_defs) {
        const auto& def = p.second;
        switch (def.type) {
        case ArgType::BOOL:
            os << std::format(" {}", def.name);
            break;
        case ArgType::STRING:
            os << std::format(" {} \"{}\"", def.name, def.example);
            break;
        case ArgType::INT:
            os << std::format(" {} {}", def.name, def.example);
            break;
        }
    }
    for (const auto& name : this->positional_arg_names) {
        const auto& def = this->positional_arg_defs.find(name)->second;
        if (def.required) {
            os << std::format(" {}", def.example);
        } else {
            os << std::format(" {}", def.example);
        }
    }
    os << std::endl;
}

void validate_required_options(
    const std::map<std::string_view, ArgDef>& defs, const std::map<std::string_view, ArgValue>& options
) {
    for (const auto& p : defs) {
        const auto& def = p.second;
        auto it = options.find(def.name);
        if (def.required && it == options.end()) {
            throw ArgParseException(std::format("missing option \"{}\"", def.name));
        }
        if (it != options.end()) {
            switch (def.type) {
            case ArgType::BOOL:
                assert(std::holds_alternative<bool>(it->second));
                break;
            case ArgType::STRING:
                assert(std::holds_alternative<const char*>(it->second));
                break;
            case ArgType::INT:
                assert(std::holds_alternative<int>(it->second));
                break;
            }
        }
    }
}

void valildate_required_positional_arguments(
    const std::map<std::string_view, ArgDef>& positional_arg_defs,
    const std::vector<std::string_view>& positional_arg_names,
    const std::map<std::string_view, ArgValue>& positional_args
) {
    for (const std::string_view name : positional_arg_names) {
        auto def = positional_arg_defs.find(name)->second;
        auto it = positional_args.find(name);
        if (def.required && it == positional_args.end()) {
            throw ArgParseException(std::format("missing positional arg \"{}\"", name));
        }
        if (it != positional_args.end()) {
            switch (def.type) {
            case ArgType::BOOL:
                assert(std::holds_alternative<bool>(it->second));
                break;
            case ArgType::STRING:
                assert(std::holds_alternative<const char*>(it->second));
                break;
            case ArgType::INT:
                assert(std::holds_alternative<int>(it->second));
                break;
            }
        }
    }
}

void ArgParser::parse(int argc, const char* argv[]) {
    std::map<std::string_view, ArgValue> options;
    std::map<std::string_view, ArgValue> positional_args;
    size_t positional_arg_idx = 0;
    for (int i = 1; i < argc; ++i) {
        if (this->option_defs.contains(argv[i])) {
            ArgDef def = this->option_defs[argv[i]];
            switch (def.type) {
            case ArgType::BOOL:
                options[argv[i]] = true;
                break;
            case ArgType::STRING:
                if (i + 1 >= argc || starts_with_double_dash(argv[i + 1])) {
                    throw ArgParseException(std::format("missing string value for {}", argv[i]));
                }
                options[argv[i]] = argv[i + 1];
                ++i;
                break;
            case ArgType::INT:
                if (i + 1 >= argc || starts_with_double_dash(argv[i + 1])) {
                    throw ArgParseException(std::format("missing int value for {}", argv[i]));
                }
                try {
                    options[argv[i]] = std::stoi(argv[i + 1]);
                } catch (const std::invalid_argument& e) {
                    throw ArgParseException(std::format("expect int value but found {}", argv[i + 1]));
                }
                ++i;
                break;
            }
        } else if (starts_with_double_dash(argv[i])) {
            throw ArgParseException(std::format("unknown flag \"{}\"", argv[i]));
        } else {
            if (positional_arg_idx >= this->positional_arg_defs.size()) {
                throw ArgParseException(
                    std::format("expect {} args, but found {}", this->positional_arg_defs.size(), argv[i])
                );
            }
        }
    }
    validate_required_options(this->option_defs, options);
    valildate_required_positional_arguments(this->positional_arg_defs, this->positional_arg_names, positional_args);
    this->options = options;
    this->positional_args = positional_args;
}

bool ArgParser::exist_opt(std::string_view name) {
    return this->options.contains(name);
}

bool ArgParser::exist_positional_arg(std::string_view name) {
    return this->positional_args.contains(name);
}

#include "test.h"
TEST_CASE("parse 1 string option") {
    auto parser = ArgParser("hello").add_option(
        ArgDef{
            .name = "--name",
            .type = ArgType::STRING,
            .desc = "The name to greet",
            .example = "M",
            .required = false,
        }
    );
    int argc = 3;
    const char* argv[] = {"hello", "--name", "Minh"};
    parser.parse(argc, argv);
    assert(strcmp(parser.opt<const char*>("--name"), "Minh") == 0);
}

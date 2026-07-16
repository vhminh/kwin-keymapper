#include "argparse.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
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

void validate_required_positional_arguments(
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
            std::string_view name = this->positional_arg_names[positional_arg_idx++];
            ArgDef def = this->positional_arg_defs[name];
            switch (def.type) {
            case ArgType::BOOL:
                if (strcmp(argv[i], "true") == 0) {
                    positional_args[name] = true;
                } else if (strcmp(argv[i], "false") == 0) {
                    positional_args[name] = false;
                } else {
                    throw ArgParseException(std::format("expect bool value but found {}", argv[i]));
                }
                break;
            case ArgType::STRING:
                positional_args[name] = argv[i];
                break;
            case ArgType::INT:
                try {
                    positional_args[name] = std::stoi(argv[i]);
                } catch (const std::invalid_argument& e) {
                    throw ArgParseException(std::format("expect int value but found {}", argv[i]));
                }
                break;
            }
        }
    }
    validate_required_options(this->option_defs, options);
    validate_required_positional_arguments(this->positional_arg_defs, this->positional_arg_names, positional_args);
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
        }
    );
    const char* argv[] = {"hello", "--name", "Minh"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_opt("--name"));
    ASSERT_EQ(std::string_view(parser.opt<const char*>("--name")), "Minh");
}

TEST_CASE("parse 1 bool option") {
    auto parser = ArgParser("hello").add_option(
        ArgDef{
            .name = "--version",
            .type = ArgType::BOOL,
            .desc = "Get version",
            .example = "",
        }
    );
    const char* argv[] = {"hello", "--version"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_opt("--version"));
    ASSERT_TRUE(parser.opt<bool>("--version"));
}

TEST_CASE("parse 1 int option") {
    auto parser = ArgParser("square").add_option(
        ArgDef{
            .name = "--num",
            .type = ArgType::INT,
            .desc = "Number to square",
            .example = "4",
        }
    );
    const char* argv[] = {"square", "--num", "4"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_opt("--num"));
    ASSERT_EQ(parser.opt<int>("--num"), 4);
}

TEST_CASE("parse 1 positional arg") {
    auto parser = ArgParser("ls").add_positional_argument(
        ArgDef{
            .name = "dir",
            .type = ArgType::STRING,
            .desc = "Directory",
            .example = "folder",
        }
    );
    const char* argv[] = {"ls", "folder"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_positional_arg("dir"));
    ASSERT_EQ(std::string_view(parser.positional_arg<const char*>("dir")), "folder");
}

TEST_CASE("parse 2 positional args") {
    auto parser = ArgParser("add")
                      .add_positional_argument(
                          ArgDef{
                              .name = "num1",
                              .type = ArgType::INT,
                              .desc = "First number",
                              .example = "12",
                          }
                      )
                      .add_positional_argument(

                          ArgDef{
                              .name = "num2",
                              .type = ArgType::INT,
                              .desc = "Second number",
                              .example = "13",
                          }
                      );
    const char* argv[] = {"add", "12", "13"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_positional_arg("num1"));
    ASSERT_EQ(parser.positional_arg<int>("num1"), 12);
    ASSERT_TRUE(parser.exist_positional_arg("num2"));
    ASSERT_EQ(parser.positional_arg<int>("num2"), 13);
}

TEST_CASE("parse bool option before positional arg") {
    auto parser = ArgParser("ls")
                      .add_option(
                          ArgDef{
                              .name = "--hidden",
                              .type = ArgType::BOOL,
                              .desc = "Include hidden files",
                              .example = "",
                          }
                      )
                      .add_positional_argument(
                          ArgDef{
                              .name = "dir",
                              .type = ArgType::STRING,
                              .desc = "Directory",
                              .example = "folder",
                          }
                      );
    const char* argv[] = {"ls", "--hidden", "folder"};
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_opt("--hidden"));
    ASSERT_TRUE(parser.opt<bool>("--hidden"));
    ASSERT_TRUE(parser.exist_positional_arg("dir"));
    ASSERT_EQ(std::string_view(parser.positional_arg<const char*>("dir")), "folder");
}

TEST_CASE("parse our kwin-keymapper command") {
    auto parser = ArgParser("kwin-keymapper")
                      .add_option(
                          ArgDef{
                              .name = "--dbus-addr",
                              .type = ArgType::STRING,
                              .desc = "User session DBus address",
                              .example = "$DBUS_SESSION_BUS_ADDRESS",
                              .required = true,
                          }
                      )
                      .add_option(
                          ArgDef{
                              .name = "--device-file",
                              .type = ArgType::STRING,
                              .desc = "Path to your keyboard input device file",
                              .example = "/dev/input/event8",
                              .required = true,
                          }
                      );
    const char* argv[] = {
        "kwin-keymapper", "--dbus-addr", "unix:path=/run/user/1000/bus", "--device-file", "/dev/input/event8"
    };
    int argc = sizeof(argv) / sizeof(const char*);
    parser.parse(argc, argv);

    ASSERT_TRUE(parser.exist_opt("--dbus-addr"));
    ASSERT_EQ(std::string_view(parser.opt<const char*>("--dbus-addr")), "unix:path=/run/user/1000/bus");
    ASSERT_TRUE(parser.exist_opt("--device-file"));
    ASSERT_EQ(std::string_view(parser.opt<const char*>("--device-file")), "/dev/input/event8");
}

#pragma once
#include <exception>
#include <map>
#include <string>
#include <variant>
#include <vector>

enum class ArgType {
    BOOL = 1,
    STRING = 2,
    INT = 3,
};

typedef std::variant<const char*, int, bool> ArgValue;

struct ArgDef {
    std::string_view name;
    ArgType type;
    bool required;
};

class ArgParser {
public:
    ArgParser();

    ArgParser& add_option(ArgDef def);
    // only supports optional positional argument at the end
    ArgParser& add_positional_argument(ArgDef def);

    void print_help(std::ostream&);

    // @throws ArgParseException if the args can't be parsed
    void parse(int argc, const char* argv[]);

    bool exist_opt(std::string_view name);
    bool exist_positional_arg(std::string_view name);
    template <class T> T opt(std::string_view name) {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, const char*> || std::is_same_v<T, int>);
        return std::get<T>(this->options[name]);
    }
    template <class T>
    T positional_arg(std::string_view name) {
        static_assert(std::is_same_v<T, bool> || std::is_same_v<T, const char*> || std::is_same_v<T, int>);
        return std::get<T>(this->positional_args[name]);
    }

private:
    std::map<std::string_view, ArgValue> options;
    std::map<std::string_view, ArgValue> positional_args;

    std::map<std::string_view, ArgDef> option_defs;
    std::map<std::string_view, ArgDef> positional_arg_defs;
    std::vector<std::string_view> positional_arg_names;
};

class ArgParseException : public std::exception {
public:
    ArgParseException(std::string&& msg) : msg(std::move(msg)) {}
    ArgParseException(const ArgParseException&) = delete;
    ArgParseException(ArgParseException&& other) : ArgParseException(std::move(other.msg)) {}
    ArgParseException& operator=(const ArgParseException&) = delete;
    ArgParseException& operator=(ArgParseException&&) = delete;
    const char* what() const noexcept override { return this->msg.c_str(); }

private:
    std::string msg;
};

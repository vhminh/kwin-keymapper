#pragma once
#include <exception>
#include <map>
#include <string>

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

// only support string args
std::map<std::string_view, const char*> parse_opts(int argc, const char* argv[]);

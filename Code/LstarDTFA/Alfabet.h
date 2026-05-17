#pragma once
#include <format>
#include <iostream>
#include <string>
#include <vector>


namespace alphabet {

struct Symbol
{
    size_t      m_arity;
    std::string m_name;
};

class Alphabet
{
private:
    std::vector<Symbol> symbols = {};

public:
    void set_alphabet(const std::string& str)
    {
        symbols.clear();
        extend_alphabet(str);
    }

    const std::vector<Symbol>& get_symbols() const
    {
        return symbols;
    }

    void extend_alphabet(const std::string& str)
    {
        size_t pos_start = str.find('(');
        size_t pos_end   = str.find(')');

        if (pos_end < pos_start)
        {
            return;
        }

        while (pos_start != std::string::npos && pos_end != std::string::npos)
        {
            add_symbol(str.substr(pos_start + 1, pos_end - pos_start - 1));
            pos_start = str.find('(', pos_end + 1);
            pos_end   = str.find(')', pos_start + 1);
        }
    }

    void add_symbol(const std::string& str)
    {
        auto comma_pos = str.find(',');
        symbols.emplace_back(std::stoul(str.substr(comma_pos + 1)), str.substr(0, comma_pos));
    }

    Alphabet& add_symbol(const Symbol& s)
    {
        symbols.emplace_back(s);
        return *this;
    }

    const Symbol& get_symbol(const std::string& str) const
    {
        for (const auto& s : symbols)
        {
            if (s.m_name == str)
            {
                return s;
            }
        }

        std::cerr << std::format("Failed while getting symbol with name {}\n", str);
        throw std::invalid_argument(std::format("Failed while getting symbol with name {}\n", str));
    }


    size_t arity(const std::string& str) const
    {
        for (const auto& [arity, name] : symbols)
        {
            if (name == str)
            {
                return arity;
            }
        }

        std::cerr << std::format("Failed in arity when finding {}\nSymbols:\n", str);

        for (auto s : symbols)
        {
            std::cerr << std::format("name: \'{}\' arity: {}\n", s.m_name, s.m_arity);
        }

        throw std::invalid_argument(std::format("Failed in arity when finding {}\n", str));
    }
};
} // namespace alphabet

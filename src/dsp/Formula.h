#pragma once

#include <juce_core/juce_core.h>

#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// M7.4: a small expression evaluator for the wavetable editor's formula
// input. It parses once into a tree and evaluates it per sample.
//
// Variables: x (the phase through the cycle, 0..1), f (the frame's position
// through the table, 0..1), n (the frame index), pi, e.
// Operators: + - * / % ^ (power), unary minus, comparisons (< > <= >=, 1 or 0).
// Functions: sin cos tan asin acos atan tanh abs sqrt exp log log2 floor ceil
// round fract sign min max pow clamp mix (a, b, t) and the shapes saw, square,
// tri and pulse (x, width), each one cycle over x = 0..1 and ranging -1..1.
class Formula
{
public:
    struct Variables { double x = 0.0, f = 0.0, n = 0.0; };

    // Returns an empty string on success, otherwise the error.
    juce::String parse (const juce::String& text)
    {
        source = text.toStdString();
        position = 0;
        error.clear();
        root.reset();
        try
        {
            root = parseComparison();
            skipSpace();
            if (position < source.size())
                fail ("unexpected '" + juce::String::charToString ((juce::juce_wchar) source[position]) + "'");
        }
        catch (const std::runtime_error& failure)
        {
            error = failure.what();
            root.reset();
        }
        return error;
    }

    bool isValid() const { return root != nullptr; }

    double evaluate (const Variables& variables) const
    {
        if (root == nullptr)
            return 0.0;
        const auto value = root->evaluate (variables);
        return std::isfinite (value) ? value : 0.0;
    }

private:
    struct Node
    {
        virtual ~Node() = default;
        virtual double evaluate (const Variables&) const = 0;
    };

    struct Constant : Node
    {
        explicit Constant (double v) : value (v) {}
        double evaluate (const Variables&) const override { return value; }
        double value;
    };

    struct Variable : Node
    {
        explicit Variable (char n) : name (n) {}
        double evaluate (const Variables& v) const override { return name == 'x' ? v.x : name == 'f' ? v.f : v.n; }
        char name;
    };

    struct Unary : Node
    {
        Unary (std::unique_ptr<Node> a, double (*fn) (double)) : arg (std::move (a)), function (fn) {}
        double evaluate (const Variables& v) const override { return function (arg->evaluate (v)); }
        std::unique_ptr<Node> arg;
        double (*function) (double);
    };

    struct Binary : Node
    {
        Binary (std::unique_ptr<Node> a, std::unique_ptr<Node> b, double (*fn) (double, double))
            : left (std::move (a)), right (std::move (b)), function (fn) {}
        double evaluate (const Variables& v) const override { return function (left->evaluate (v), right->evaluate (v)); }
        std::unique_ptr<Node> left, right;
        double (*function) (double, double);
    };

    struct Ternary : Node
    {
        Ternary (std::unique_ptr<Node> a, std::unique_ptr<Node> b, std::unique_ptr<Node> c, double (*fn) (double, double, double))
            : first (std::move (a)), second (std::move (b)), third (std::move (c)), function (fn) {}
        double evaluate (const Variables& v) const override
        {
            return function (first->evaluate (v), second->evaluate (v), third->evaluate (v));
        }
        std::unique_ptr<Node> first, second, third;
        double (*function) (double, double, double);
    };

    [[noreturn]] static void fail (const juce::String& message) { throw std::runtime_error (message.toStdString()); }

    void skipSpace()
    {
        while (position < source.size() && std::isspace ((unsigned char) source[position]))
            ++position;
    }

    bool accept (const char* token)
    {
        skipSpace();
        const auto length = std::strlen (token);
        if (source.compare (position, length, token) == 0)
        {
            position += length;
            return true;
        }
        return false;
    }

    std::unique_ptr<Node> parseComparison()
    {
        auto left = parseSum();
        for (;;)
        {
            if (accept ("<="))
                left = std::make_unique<Binary> (std::move (left), parseSum(), [] (double a, double b) { return a <= b ? 1.0 : 0.0; });
            else if (accept (">="))
                left = std::make_unique<Binary> (std::move (left), parseSum(), [] (double a, double b) { return a >= b ? 1.0 : 0.0; });
            else if (accept ("<"))
                left = std::make_unique<Binary> (std::move (left), parseSum(), [] (double a, double b) { return a < b ? 1.0 : 0.0; });
            else if (accept (">"))
                left = std::make_unique<Binary> (std::move (left), parseSum(), [] (double a, double b) { return a > b ? 1.0 : 0.0; });
            else
                return left;
        }
    }

    std::unique_ptr<Node> parseSum()
    {
        auto left = parseProduct();
        for (;;)
        {
            if (accept ("+"))
                left = std::make_unique<Binary> (std::move (left), parseProduct(), [] (double a, double b) { return a + b; });
            else if (accept ("-"))
                left = std::make_unique<Binary> (std::move (left), parseProduct(), [] (double a, double b) { return a - b; });
            else
                return left;
        }
    }

    std::unique_ptr<Node> parseProduct()
    {
        auto left = parseUnary();
        for (;;)
        {
            if (accept ("*"))
                left = std::make_unique<Binary> (std::move (left), parseUnary(), [] (double a, double b) { return a * b; });
            else if (accept ("/"))
                left = std::make_unique<Binary> (std::move (left), parseUnary(),
                                                 [] (double a, double b) { return b == 0.0 ? 0.0 : a / b; });
            else if (accept ("%"))
                left = std::make_unique<Binary> (std::move (left), parseUnary(),
                                                 [] (double a, double b) { return b == 0.0 ? 0.0 : a - b * std::floor (a / b); });
            else
                return left;
        }
    }

    std::unique_ptr<Node> parseUnary()
    {
        if (accept ("-"))
            return std::make_unique<Unary> (parseUnary(), [] (double a) { return -a; });
        if (accept ("+"))
            return parseUnary();
        return parsePower();
    }

    std::unique_ptr<Node> parsePower()
    {
        auto base = parsePrimary();
        if (accept ("^"))
            return std::make_unique<Binary> (std::move (base), parseUnary(), [] (double a, double b) { return std::pow (a, b); });
        return base;
    }

    std::vector<std::unique_ptr<Node>> parseArguments()
    {
        std::vector<std::unique_ptr<Node>> arguments;
        if (! accept ("("))
            fail ("expected '('");
        if (accept (")"))
            return arguments;
        do
            arguments.push_back (parseComparison());
        while (accept (","));
        if (! accept (")"))
            fail ("expected ')'");
        return arguments;
    }

    static double fract (double a) { return a - std::floor (a); }

    std::unique_ptr<Node> parsePrimary()
    {
        skipSpace();
        if (position >= source.size())
            fail ("unexpected end");

        if (accept ("("))
        {
            auto inner = parseComparison();
            if (! accept (")"))
                fail ("expected ')'");
            return inner;
        }

        const auto c = source[position];
        if (std::isdigit ((unsigned char) c) || c == '.')
        {
            size_t used = 0;
            const auto value = std::stod (source.substr (position), &used);
            position += used;
            return std::make_unique<Constant> (value);
        }

        if (! std::isalpha ((unsigned char) c))
            fail ("unexpected '" + juce::String::charToString ((juce::juce_wchar) c) + "'");

        std::string word;
        while (position < source.size() && (std::isalnum ((unsigned char) source[position]) || source[position] == '_'))
            word += source[position++];

        if (word == "x" || word == "t") return std::make_unique<Variable> ('x');
        if (word == "f") return std::make_unique<Variable> ('f');
        if (word == "n") return std::make_unique<Variable> ('n');
        if (word == "pi") return std::make_unique<Constant> (juce::MathConstants<double>::pi);
        if (word == "e") return std::make_unique<Constant> (juce::MathConstants<double>::euler);

        struct OneArgument { const char* name; double (*function) (double); };
        static const OneArgument oneArgument[] {
            { "sin", [] (double a) { return std::sin (a); } }, { "cos", [] (double a) { return std::cos (a); } },
            { "tan", [] (double a) { return std::tan (a); } }, { "asin", [] (double a) { return std::asin (juce::jlimit (-1.0, 1.0, a)); } },
            { "acos", [] (double a) { return std::acos (juce::jlimit (-1.0, 1.0, a)); } }, { "atan", [] (double a) { return std::atan (a); } },
            { "tanh", [] (double a) { return std::tanh (a); } }, { "abs", [] (double a) { return std::abs (a); } },
            { "sqrt", [] (double a) { return std::sqrt (juce::jmax (0.0, a)); } }, { "exp", [] (double a) { return std::exp (juce::jmin (a, 60.0)); } },
            { "log", [] (double a) { return a > 0.0 ? std::log (a) : -60.0; } }, { "log2", [] (double a) { return a > 0.0 ? std::log2 (a) : -60.0; } },
            { "floor", [] (double a) { return std::floor (a); } }, { "ceil", [] (double a) { return std::ceil (a); } },
            { "round", [] (double a) { return std::round (a); } }, { "fract", [] (double a) { return fract (a); } },
            { "sign", [] (double a) { return a > 0.0 ? 1.0 : (a < 0.0 ? -1.0 : 0.0); } },
            { "saw", [] (double a) { return 2.0 * fract (a + 0.5) - 1.0; } },
            { "square", [] (double a) { return fract (a) < 0.5 ? 1.0 : -1.0; } },
            { "tri", [] (double a) { return 1.0 - 4.0 * std::abs (fract (a + 0.25) - 0.5); } },
        };
        struct TwoArguments { const char* name; double (*function) (double, double); };
        static const TwoArguments twoArguments[] {
            { "min", [] (double a, double b) { return juce::jmin (a, b); } }, { "max", [] (double a, double b) { return juce::jmax (a, b); } },
            { "pow", [] (double a, double b) { return std::pow (a, b); } },
            { "pulse", [] (double a, double b) { return fract (a) < juce::jlimit (0.0, 1.0, b) ? 1.0 : -1.0; } },
        };
        struct ThreeArguments { const char* name; double (*function) (double, double, double); };
        static const ThreeArguments threeArguments[] {
            { "clamp", [] (double a, double lo, double hi) { return juce::jlimit (juce::jmin (lo, hi), juce::jmax (lo, hi), a); } },
            { "mix", [] (double a, double b, double t) { return a + (b - a) * t; } },
        };

        for (const auto& entry : oneArgument)
            if (word == entry.name)
            {
                auto arguments = parseArguments();
                if (arguments.size() != 1) fail (juce::String (entry.name) + " takes 1 argument");
                return std::make_unique<Unary> (std::move (arguments[0]), entry.function);
            }
        for (const auto& entry : twoArguments)
            if (word == entry.name)
            {
                auto arguments = parseArguments();
                if (arguments.size() != 2) fail (juce::String (entry.name) + " takes 2 arguments");
                return std::make_unique<Binary> (std::move (arguments[0]), std::move (arguments[1]), entry.function);
            }
        for (const auto& entry : threeArguments)
            if (word == entry.name)
            {
                auto arguments = parseArguments();
                if (arguments.size() != 3) fail (juce::String (entry.name) + " takes 3 arguments");
                return std::make_unique<Ternary> (std::move (arguments[0]), std::move (arguments[1]),
                                                  std::move (arguments[2]), entry.function);
            }

        fail ("unknown name '" + juce::String (word) + "'");
    }

    std::string source;
    size_t position = 0;
    juce::String error;
    std::unique_ptr<Node> root;
};

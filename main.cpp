#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <variant>
#include <vector>

static constexpr size_t CommandBits = 5;

// The following will contain the awatisms and the relevant data
template<uint8_t Value, bool HasParameter = false>
struct Command {
    static_assert(Value < 1<<CommandBits, "We are currently limited to commands less than a certain number of bits");
    [[maybe_unused]] static constexpr uint8_t VALUE         = Value;
                     static constexpr bool    HAS_PARAMETER = HasParameter;
};

template<uint8_t Value, class T>
struct ParameterizedCommand : public Command<Value, true> { T parameter; };

struct NoOp        : public Command<0x0> {};
struct Print       : public Command<0x1> {};
struct PrintNum    : public Command<0x2> {};
struct Read        : public Command<0x3> {};
struct ReadNum     : public Command<0x4> {};
struct Blow        : public ParameterizedCommand<0x5, int8_t> {};
struct Submerge    : public ParameterizedCommand<0x6, uint8_t> {};
struct Pop         : public Command<0x7> {};
struct Duplicate   : public Command<0x8> {};
struct Surround    : public ParameterizedCommand<0x9, uint8_t> {};
struct Merge       : public Command<0xA> {};
struct Add         : public Command<0xB> {};
struct Subtract    : public Command<0xC> {};
struct Multiply    : public Command<0xD> {};
struct Divide      : public Command<0xE> {};
struct Count       : public Command<0xF> {};
struct Label       : public ParameterizedCommand<0x10, uint8_t> {};
struct Jump        : public ParameterizedCommand<0x11, uint8_t> {};
struct EqualTo     : public Command<0x12> {};
struct LessThan    : public Command<0x13> {};
struct GreaterThan : public Command<0x14> {};
struct Terminate   : public Command<0x1F> {};

using CommandObject = std::variant<
        NoOp, Print, PrintNum, Read,
        ReadNum, Blow, Submerge, Pop,
        Duplicate, Surround, Merge, Add,
        Subtract, Multiply, Divide, Count,
        Label, Jump, EqualTo, LessThan,
        GreaterThan, Terminate>;

template<std::size_t I = 0>
constexpr CommandObject CommandFromInteger(std::size_t value) {
    if constexpr (I < std::variant_size_v<CommandObject>) {
        if (std::variant_alternative_t<I, CommandObject>::VALUE == value) {
            return std::variant_alternative_t<I, CommandObject>{};
        }

        return CommandFromInteger<I+1>(value);
    }

    return NoOp{};
}

struct ParameterGetterVisitor {
    bool result;

    template<class T>
    void operator()(T) {
        result = T::HAS_PARAMETER;
    }
};

constexpr bool HasParameter(CommandObject command) {
    ParameterGetterVisitor visitor {};
    std::visit(visitor, command);
    return visitor.result;
}

struct IsParameterSignedVisitor {
    bool result = false;

    template<class T>
    void operator()(T) {
        if constexpr (T::HAS_PARAMETER) {
            result = std::is_signed_v<decltype(T::parameter)>;
        }
    }
};


constexpr bool HasSignedParameter(CommandObject command) {
    IsParameterSignedVisitor visitor;
    std::visit(visitor, command);
    return visitor.result;
}

template<class Value>
struct ParameterSetterVisitor {
    Value value;

    template<class T>
    void operator()(T& command) {
        if constexpr (T::HAS_PARAMETER) {
            command.parameter = value;
        }
    }
};

template<class T>
void SetParameter(CommandObject& command, T parameter) {
    ParameterSetterVisitor visitor {parameter};
    std::visit(visitor, command);
}

using Program = std::vector<CommandObject>;

// this section contains the actual interpreter


using Bubble = int32_t;
using DoubleBubble = std::vector<Bubble>;
using BubbleAbyss = std::vector<std::variant<DoubleBubble, Bubble>>;

[[nodiscard]] constexpr char GetAwascii(Bubble bubble) {
    constexpr std::string_view awascii =
            "AWawJELYHOSIUMjelyhosiumPCNTpcntBDFGRbdfgr0123456789 .,!'()~_/;\n";

    if (bubble < 0 || bubble > awascii.size())
        return 'X';

    return awascii[bubble];
}

class Interpreter {
    BubbleAbyss bubbleAbyss = {};
    Program program;
    static constexpr size_t NUM_LABELS = 32;
    std::array<size_t, NUM_LABELS> labels = {};
    size_t programCounter = 0;
    bool needsTermination = false;

public:
    void operator()(NoOp) const noexcept {
        // do nothing
    }

    void operator()(Print) {
        struct PrintVisitor {
            void operator()(Bubble bubble) {
                std::cout << GetAwascii(bubble) << std::flush;
            }

            void operator()(const DoubleBubble& bubbles) {
                for (auto bubble : bubbles) {
                    std::cout << GetAwascii(bubble);
                }
                std::cout << std::flush;
            }
        };

        std::visit(PrintVisitor{}, bubbleAbyss.back());
        bubbleAbyss.pop_back();
    }

    void operator()(PrintNum) {
        struct PrintNumVisitor {
            void operator()(Bubble bubble) {
                std::cout << bubble << std::flush;
            }

            void operator()(const DoubleBubble& bubbles) {
                for (auto i = 0; i < bubbles.size(); i++) {
                    if (bubbles[i] < 0)
                        std::cout << '~';
                    std::cout << std::abs(bubbles[i]);
                    if (i != bubbles.size() - 1) {
                        std::cout << ' ';
                    }
                }
                std::cout << std::flush;
            }
        };

        std::visit(PrintNumVisitor{}, bubbleAbyss.back());
        bubbleAbyss.pop_back();
    }

    void operator()(Read) {
        // TODO
    }

    void operator()(ReadNum) {
        // TODO
    }

    void operator()(Blow blow) {
        bubbleAbyss.emplace_back(blow.parameter);
    }

    void operator()(Submerge submerge) {
        if ((submerge.parameter > bubbleAbyss.size() - 1) || submerge.parameter == 0) {
            std::rotate(bubbleAbyss.rbegin(), bubbleAbyss.rbegin() + 1, bubbleAbyss.rend());
        } else {
            std::rotate(bubbleAbyss.rbegin(), bubbleAbyss.rbegin() + 1, bubbleAbyss.rbegin() + submerge.parameter);
        }
    }

    void operator()(Pop) {
        struct PopVisitor {
            std::vector<Bubble> bubbles;

            void operator()(Bubble) {};
            void operator()(DoubleBubble& doubleBubble) {
                bubbles = std::move(doubleBubble);
            }
        };

        PopVisitor visitor;
        std::visit(visitor, bubbleAbyss.back());
        bubbleAbyss.pop_back();

        bubbleAbyss.insert(bubbleAbyss.end(), visitor.bubbles.begin(), visitor.bubbles.end());
    }

    void operator()(Duplicate) {
        auto back = std::move(bubbleAbyss.back());
        bubbleAbyss.push_back(back);
    }

    void operator()(Surround surround) {
        struct SurroundVisitor {
            DoubleBubble newBubble;

            void operator()(Bubble bubble) {
                newBubble.push_back(bubble);
            }

            void operator()(const DoubleBubble& doubleBubble) {
                newBubble.insert(newBubble.end(), doubleBubble.begin(), doubleBubble.end());
            }
        };
        SurroundVisitor visitor;
        for (auto i = 0; i < surround.parameter; i++) {
            std::visit(visitor, bubbleAbyss.back());
            bubbleAbyss.pop_back();
        }
        bubbleAbyss.emplace_back(std::move(visitor.newBubble));
    }

    void operator()(Merge) {
        struct MergeVisitor {
            DoubleBubble bubbles;

            void operator()(Bubble bubble) {
                bubbles.push_back(bubble);
            }

            void operator()(DoubleBubble& doubleBubble) {
                if (bubbles.empty()) {
                    bubbles = std::move(doubleBubble);
                } else {
                    bubbles.insert(bubbles.end(), doubleBubble.begin(), doubleBubble.end());
                }
            }
        };

        auto left = std::move(bubbleAbyss.back());
        bubbleAbyss.pop_back();
        auto right = std::move(bubbleAbyss.back());
        bubbleAbyss.pop_back();

        if (const Bubble *leftBubble = std::get_if<Bubble>(&left), *rightBubble = std::get_if<Bubble>(&right);
            leftBubble && rightBubble) {
            bubbleAbyss.emplace_back(*leftBubble+*rightBubble);
        } else {
            MergeVisitor visitor;
            std::visit(visitor, left);
            std::visit(visitor, right);
            bubbleAbyss.emplace_back(std::move(visitor.bubbles));
        }
    }

    template<class Operation, class ComplimentaryOp = std::nullptr_t>
    void DoArithmeticOperation(Operation operation, ComplimentaryOp complimentaryOp = nullptr) {
        auto left = std::move(bubbleAbyss.back());
        bubbleAbyss.pop_back();

        auto right = std::move(bubbleAbyss.back());
        bubbleAbyss.pop_back();

        if (const Bubble *leftBubble = std::get_if<Bubble>(&left), *rightBubble = std::get_if<Bubble>(&right);
                leftBubble && rightBubble) { // both are single
            if constexpr (std::is_same_v<ComplimentaryOp, std::nullptr_t>) {
                bubbleAbyss.push_back(operation(*leftBubble, *rightBubble));
            } else {
                /**
                 * We have to special case division because
                 * jerry is too cool to have a dedicated
                 * modulus operation or something idk
                 */
                DoubleBubble newBubble {operation(*leftBubble, *rightBubble), complimentaryOp(*leftBubble, *rightBubble)};
                bubbleAbyss.emplace_back(std::move(newBubble));
            }
        } else if (!leftBubble && !rightBubble) { // both are double
            auto& leftDouble = std::get<DoubleBubble>(left);
            auto& rightDouble = std::get<DoubleBubble>(right);

            DoubleBubble result;
            result.reserve(std::max(leftDouble.size(), rightDouble.size()));
            auto liter = leftDouble.begin();
            auto riter = rightDouble.begin();
            for (;liter != leftDouble.end() && riter != rightDouble.end(); liter++, riter++) {
                result.push_back(operation(*liter, *riter));
            }

            for (;liter != leftDouble.end(); liter++) {
                result.push_back(*liter);
            }

            for (;riter != leftDouble.end(); riter++) {
                result.push_back(*riter);
            }

            bubbleAbyss.emplace_back(std::move(result));
        } else { // only one is a double
            const Bubble single = leftBubble ? *leftBubble : *rightBubble;
            DoubleBubble& doubleBubble = leftBubble ? std::get<DoubleBubble>(right) : std::get<DoubleBubble>(left);
            std::transform(doubleBubble.begin(), doubleBubble.end(), doubleBubble.begin(), [&single, &operation](Bubble val) {
                return operation(val, single);
            });
            bubbleAbyss.emplace_back(std::move(doubleBubble));
        }
    }

    void operator()(Add)      { DoArithmeticOperation(std::plus{});                                  }
    void operator()(Subtract) { DoArithmeticOperation(std::minus{});                                 }
    void operator()(Multiply) { DoArithmeticOperation(std::multiplies{});                            }
    void operator()(Divide)   { DoArithmeticOperation(std::divides{}, std::modulus{}); }

    void operator()(Count) {
        struct CountVisitor {
            Bubble result;

            void operator()(Bubble) {
                result = 0;
            }

            void operator()(const DoubleBubble& doubleBubble) {
                result = static_cast<Bubble>(doubleBubble.size());
            }
        };

        CountVisitor visitor {};
        std::visit(visitor, bubbleAbyss.back());
        bubbleAbyss.emplace_back(visitor.result);
    }

    void operator()(Label label) {
        if (label.parameter < labels.size()) {
            labels[label.parameter] = programCounter; // NOLINT(*-pro-bounds-constant-array-index)
        }
    }

    void operator()(Jump jump) {
        if (jump.parameter < labels.size() && labels[jump.parameter] != std::numeric_limits<size_t>::max()) { // NOLINT(*-pro-bounds-constant-array-index)
            programCounter = labels[jump.parameter]; // NOLINT(*-pro-bounds-constant-array-index)
        }
    }

    template<class Comparison>
    void DoComparison(Comparison comparison) {
        if (bubbleAbyss.size() <= 1) {
            programCounter++;
            return;
        }
        const auto* first = std::get_if<Bubble>(&bubbleAbyss[bubbleAbyss.size()]);
        const auto* second = std::get_if<Bubble>(&bubbleAbyss[bubbleAbyss.size()-1]);

        if (first && second && comparison(*first, *second))
            return;

        programCounter++;
    }

    void operator()(EqualTo)     { DoComparison(std::equal_to{}); }
    void operator()(LessThan)    { DoComparison(std::less{});     }
    void operator()(GreaterThan) { DoComparison(std::greater{});  }

    void operator()(Terminate)  {needsTermination = true;}

    explicit Interpreter(Program pProgram) : program(std::move(pProgram)) {
        labels.fill(std::numeric_limits<size_t>::max());
    }

    void Execute() {
        while (programCounter < program.size() && !needsTermination) {
            auto& currentCommand = program[programCounter];
            // execute the command
            std::visit(*this, currentCommand);
        }
    }

};
void CleanLine(std::string& line) {
    line.erase(std::remove_if(line.begin(), line.end(), [](char c) { // NOLINT(*-identifier-length)
        switch (c) {
            case 'a':
            case 'A':
            case 'w':
            case 'W':
            case ' ':
                return false;
            default:
                return true;
        }
    }), line.end());
}

CommandObject ReadAwaTalk(std::string_view line) {
    constexpr std::string_view SENTINEL = "awa";
    const auto first = std::search(line.begin(), line.end(), SENTINEL.begin(), SENTINEL.end()); // NOLINT(*-qualified-auto)
    if (first == line.end())
        return NoOp{};

    // the first five bits tell us the command
    int bits = 0;
    uint8_t command = 0;
    auto i = std::distance(line.begin(), first) + 3; // NOLINT(*-identifier-length)
    for (; i < line.size() && bits < CommandBits; i++) {
        if (line.substr(i, 2) == "wa") {
            bits++;
            command <<= 1;
            command++;
            i += 2;
        } else if (line.substr(i, 3) == "awa") {
            bits++;
            command <<= 1;
            i += 4; // awa must be followed by a space lmao
        } else {
            // the awas don't awa, but we will keep awawaing until they awa
            i++;
        }
    }

    auto code = CommandFromInteger(command);
    if (!HasParameter(code))
        return code;

    int32_t parameter = 0;
    bool isNegative = false;
    for (; i < line.size(); i++) {
        if (line.substr(i, 2) == "wa") {
            if (HasSignedParameter(code)) {
                isNegative = true;
            } else {
                bits++;
                parameter <<= 1;
                parameter++;
            }
            i += 2;
        } else if (line.substr(i, 3) == "awa") {
            bits++;
            parameter <<= 1;
            i += 4; // awa must be followed by a space lmao
        } else {
            // the awas don't awa, but we will keep awawaing until they awa
            i++;
        }
    }
    parameter *= -isNegative;
    SetParameter(code, parameter);
    return code;
}

Program ReadProgram(std::istream& stream) {
    std::string line;
    Program result;
    while (std::getline(stream, line)) {
        CleanLine(line);
        result.push_back(ReadAwaTalk(line));
    }
    return result;
}


int main(int argc, char* argv[]) {
    if (argc < 1) return -1;
    std::ifstream file (argv[1]); // NOLINT(*-pro-bounds-pointer-arithmetic)
    Program program = ReadProgram(file);
    Interpreter interpreter (program);
    interpreter.Execute();
    return 0;
}

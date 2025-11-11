#include <benchmark/benchmark.h>
#include <string>
#include <random>
#include <functional>
#include "LispParseTree.h"

namespace {
    std::string BuildDeepProgram(std::size_t n) {
        std::string code;
        code.reserve(n);
        code.push_back('(');
        for (std::size_t i = 0; i < n; ++i) {
            code += "(+";
            code += i%2 == 0 ? std::to_string(i) : "_a"+std::to_string(i);
        }
        for (std::size_t i = 0; i < n; ++i) {
            code.push_back(')');
        }
        code.push_back(')');
        code.push_back(EOF);
        return code;
    }

    std::string BuildLargeAdjacentSExpressions(std::size_t n) {
        std::string code;
        code.reserve(n);
        code+= "(";
        for (int i = 0; i < n; ++i) {
            code += "(+" + std::to_string(i) + " " + "_b"+std::to_string(i) + ")";
        }
        code+= ")";
        code+= EOF;
        return code;
    }

    std::string BuildWideList(std::size_t elements) {
        std::string code;
        code.reserve(elements * 10);
        code += "(list ";
        for (std::size_t i = 0; i < elements; ++i) {
            if (i > 0) code += " ";
            code += std::to_string(i);
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildMixedDepth(std::size_t sections, std::size_t depth_per_section) {
        std::string code;
        code.reserve(sections * depth_per_section * 10);
        code += "(progn ";
        for (std::size_t s = 0; s < sections; ++s) {
            code += "(+ " + std::to_string(s) + " " + std::to_string(s + 1) + ") ";
            for (std::size_t d = 0; d < depth_per_section; ++d) {
                code += "(+ ";
            }
            code += std::to_string(s);
            for (std::size_t d = 0; d < depth_per_section; ++d) {
                code += ")";
            }
            code += " ";
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildFunctionDefinitions(std::size_t count) {
        std::string code;
        code.reserve(count * 200);
        code += "(progn ";
        for (std::size_t i = 0; i < count; ++i) {
            code += "(defun func" + std::to_string(i) + " (x y) "
                    "(if (> x y) "
                    "(+ x (* y 2)) "
                    "(- y (/ x 3))))";
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildLetBindings(std::size_t nesting, std::size_t bindings_per_level) {
        std::string code;
        code.reserve(nesting * bindings_per_level * 50);

        for (std::size_t i = 0; i < nesting; ++i) {
            code += "(let (";
            for (std::size_t b = 0; b < bindings_per_level; ++b) {
                code += "(var" + std::to_string(i * bindings_per_level + b) + " " + std::to_string(b) + ")";
            }
            code += ") ";
        }
        code += "42";
        for (std::size_t i = 0; i < nesting; ++i) {
            code += ")";
        }
        code.push_back(EOF);
        return code;
    }

    std::string BuildMixedAtoms(std::size_t count) {
        std::string code;
        code.reserve(count * 30);
        code += "(list ";
        for (std::size_t i = 0; i < count; ++i) {
            switch (i % 5) {
                case 0: code += std::to_string(i) + " "; break;
                case 1: code += std::to_string(i * 1.5) + " "; break;
                case 2: code += "sym" + std::to_string(i) + " "; break;
                case 3: code += "\"string" + std::to_string(i) + "\" "; break;
                case 4: code += (i % 2 == 0 ? "t " : "nil "); break;
                default: break;
            }
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildQuotedExpressions(std::size_t count) {
        std::string code;
        code.reserve(count * 50);
        code += "(list ";
        for (std::size_t i = 0; i < count; ++i) {
            code += "'(a b c " + std::to_string(i) + ") ";
        }
        code += ")";
        for (int i=0;i<32;++i) {
            code.push_back(EOF);
        }
        code.push_back('\0');
        return code;
    }

    std::string BuildWithComments(std::size_t expressions) {
        std::string code;
        code.reserve(expressions * 100);
        code += "(progn ";
        for (std::size_t i = 0; i < expressions; ++i) {
            code += "; Comment " + std::to_string(i) + "\n";
            code += "(+ " + std::to_string(i) + " " + std::to_string(i + 1) + ")\n";
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildLongSymbols(std::size_t count, std::size_t symbol_length) {
        std::string code;
        code.reserve(count * (symbol_length + 10));
        code += "(list ";
        for (std::size_t i = 0; i < count; ++i) {
            code += "very-long-symbol-name-";
            for (std::size_t j = 0; j < symbol_length; ++j) {
                code += static_cast<char>('a' + (j % 26));
            }
            code += "-" + std::to_string(i) + " ";
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildMacroDefinitions(std::size_t count) {
        std::string code;
        code.reserve(count * 150);
        code += "(progn ";
        for (std::size_t i = 0; i < count; ++i) {
            code += "(defmacro mac" + std::to_string(i) + " (x) "
                    "`(let ((temp ,x)) (* temp temp)))";
        }
        code += ")";
        code.push_back(EOF);
        return code;
    }

    std::string BuildRealisticCode(std::size_t complexity) {
        const std::string code = R"((defun factorial (n)
  (if (<= n 1)
      1
      (* n (factorial (- n 1)))))

(defun fibonacci (n)
  (cond ((= n 0) 0)
        ((= n 1) 1)
        (t (+ (fibonacci (- n 1))
              (fibonacci (- n 2))))))

(defun map-tree (fn tree)
  (cond ((null tree) nil)
        ((atom tree) (funcall fn tree))
        (t (cons (map-tree fn (car tree))
                 (map-tree fn (cdr tree))))))

)";
        std::string result;
        result.reserve(code.size() * complexity);
        for (std::size_t i = 0; i < complexity; ++i) {
            result += code;
        }
        return "(progn " + result + ")" + std::string(1, EOF);
    }

    std::string Build1GBDeeplyNestedProgram() {
        constexpr auto size = 85'000'000;
        std::string code;
        code.reserve(size);
        code.push_back('(');
        for (std::size_t i = 0; i < size; ++i) {
            code += "(+";
            code += i%2 == 0 ? std::to_string(i) : "_x"+std::to_string(i);;
        }
        for (std::size_t i = 0; i < size; ++i) {
            code.push_back(')');
        }
        code.push_back(')');
        code.push_back(EOF);
        return code;
    }

    std::string Build1GBAdjacentSExpressions() {
        constexpr auto size = 52'000'000;
        std::string code;
        code.reserve(size);
        code+= "(";
        for (int i = 0; i < size; ++i) {
            code += "(+" + std::to_string(i) + " " + std::to_string(i + 1) + ")";
        }
        code+= ")";
        code+= EOF;
        return code;
    }

} // namespace

constexpr int Repetitions = 10;

static void BM_ParseDeeplyNested(benchmark::State& state) {
    std::string code = BuildDeepProgram(300'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseDeeplyNested)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseAdjacent(benchmark::State& state) {
    std::string code = BuildLargeAdjacentSExpressions(250'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseAdjacent)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseWideList(benchmark::State& state) {
    std::string code = BuildWideList(250'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseWideList)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseMixedDepth(benchmark::State& state) {
    std::string code = BuildMixedDepth(1000, 100);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseMixedDepth)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseFunctionDefinitions(benchmark::State& state) {
    std::string code = BuildFunctionDefinitions(50'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseFunctionDefinitions)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseLetBindings(benchmark::State& state) {
    std::string code = BuildLetBindings(100, 20);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseLetBindings)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseMixedAtoms(benchmark::State& state) {
    std::string code = BuildMixedAtoms(250'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseMixedAtoms)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseQuotedExpressions(benchmark::State& state) {
    std::string code = BuildQuotedExpressions(250'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseQuotedExpressions)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseWithComments(benchmark::State& state) {
    std::string code = BuildWithComments(50'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),true);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseWithComments)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseLongSymbols(benchmark::State& state) {
    std::string code = BuildLongSymbols(10'000, 100);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseLongSymbols)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseMacroDefinitions(benchmark::State& state) {
    std::string code = BuildMacroDefinitions(50'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseMacroDefinitions)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ParseRealisticCode(benchmark::State& state) {
    std::string code = BuildRealisticCode(1000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ParseRealisticCode)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_ColdCache_DeeplyNested(benchmark::State& state) {
    std::string code = BuildDeepProgram(100'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    for ([[maybe_unused]]auto _ : state) {
        const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_ColdCache_DeeplyNested)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_WarmCache_DeeplyNested(benchmark::State& state) {
    std::string code = BuildDeepProgram(100'000);
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_WarmCache_DeeplyNested)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_Parse1GBDeeplyNested(benchmark::State& state) {
    std::string code = Build1GBDeeplyNestedProgram();
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_Parse1GBDeeplyNested)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

static void BM_Parse1GBAdjacentSExpressions(benchmark::State& state) {
    std::string code = Build1GBAdjacentSExpressions();
    benchmark::DoNotOptimize(code.data());
    benchmark::DoNotOptimize(code.size());
    benchmark::ClobberMemory();
    std::size_t bytes = 0;
    const auto parser = std::make_unique<WideLips::LispParser>(std::string_view(code),false);
    for ([[maybe_unused]]auto _ : state) {
        bytes += code.size();
        auto parsedProgram = parser->Parse();
        benchmark::DoNotOptimize(parsedProgram);
        parser->Reuse();
    }
    state.counters["Gigabytes"] = benchmark::Counter(
            static_cast<double>(bytes), benchmark::Counter::kIsRate,
            benchmark::Counter::OneK::kIs1000);
    state.counters["Files"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kIsRate);
    state.counters["CodeSize"] = static_cast<double>(code.size());
}

BENCHMARK(BM_Parse1GBAdjacentSExpressions)
    ->Repetitions(Repetitions)
    ->ComputeStatistics("max", [](const std::vector<double>& v) -> double {
        return *std::ranges::max_element(v);
    })
    ->Unit(benchmark::kMillisecond)
    ->DisplayAggregatesOnly(true);

BENCHMARK_MAIN();
// Copyright (C) 2022 Tycho Softworks.
// This code is licensed under MIT license.

#undef  NDEBUG
#include "compiler.hpp"     // IWYU pragma: keep
#include "tasks.hpp"
#include "print.hpp"

#include <string>
#include <tuple>
#include <memory>

using namespace tycho;

namespace {
auto count = 0;
std::string str;
task_queue tq;
func_queue fq;

auto process_command(const std::string& text, int number) {
    using args_t = std::tuple<std::string, int>;
    return fq.dispatch([](std::any args) {
        const auto& [text, num] = std::any_cast<args_t>(args);
        str = text;
        count += num;
    }, args_t{text, number});
}
} // end namespace

auto main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) -> int {
    assert(process_command("test", 42));
    assert(process_command("more", 10));
    while(!fq.empty()) {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    fq.shutdown();
    assert(count == 52);
    assert(str == "more");

    const std::shared_ptr<int> ptr = std::make_shared<int>(count);
    auto use = ptr.use_count();
    tq.startup();
    tq.dispatch([ptr, &use] {
        use = ptr.use_count();
        ++count;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    tq.shutdown();
    assert(count == 53);
    assert(use == 2);
}



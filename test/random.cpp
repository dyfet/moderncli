// Copyright (C) 2023 Tycho Softworks.
// This code is licensed under MIT license.

#undef  NDEBUG
#include "compiler.hpp"
#include "print.hpp"
#include "random.hpp"
#include "strings.hpp"

auto main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) -> int {
    const crypto::random_t<crypto::sha512_key> key1, key2;
    assert(key1.bits() == 512);
    assert(key1.size() == 64);
    assert(key1 != key2);

    const uint8_t txt[7] = {'A', 'B', 'C', 'D', 'Z', '1', '2'};
    uint8_t msg[8];
    msg[7] = 0;
    assert(crypto::to_b64(txt, sizeof(txt)) == "QUJDRFoxMg==");
    crypto::from_b64("QUJDRFoxMg==", msg, sizeof(msg));
    assert(eq("ABCDZ12", reinterpret_cast<const char *>(msg)));
}



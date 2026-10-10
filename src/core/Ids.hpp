// Deterministic, stable identifier generation for model objects.
#pragma once

#include <cstdint>
#include <string>

namespace gmb {

// Generates ids like "el_1", "act_3", "scr_2", "res_5".
// The counter is owned by the project and serialized with it, so ids remain
// stable across save/load cycles and never collide after a reload.
class IdGenerator {
public:
    explicit IdGenerator(uint64_t next = 1) : m_next(next) {}

    std::string make(const std::string& prefix) {
        return prefix + "_" + std::to_string(m_next++);
    }

    uint64_t next() const { return m_next; }
    void setNext(uint64_t v) { if (v > m_next) m_next = v; }

private:
    uint64_t m_next;
};

} // namespace gmb

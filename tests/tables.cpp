#include <reflect/reflect.hpp>

#include "check.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

struct Packet {
    std::uint8_t kind = 1;
    std::uint32_t length = 2;
    std::uint16_t checksum = 3;
    unsigned urgent : 1 = 0;

private:
    std::uint64_t sequence = 4;
};

// Standard layout, so offsetof is allowed.
struct Header {
    std::uint8_t kind = 1;
    std::uint32_t length = 2;
    std::uint16_t checksum = 3;
};

struct Nothing {};

constexpr auto table = reflect::field_table<Packet>();

static_assert(table.size() == 5);
static_assert(reflect::field_table<Packet, reflect::public_access>().size() == 4);
static_assert(reflect::field_table<Nothing>().empty());

static_assert(table[0].name == "kind" && table[0].index == 0);
static_assert(table[1].name == "length" && table[1].index == 1);
static_assert(table[0].size == 1 && table[1].size == 4 && table[2].size == 2);
static_assert(table[3].name == "urgent" && table[3].size == 0);  // bit-field
static_assert(table[0].is_public && !table[4].is_public);
static_assert(table[4].size == sizeof(std::uint64_t));

constexpr auto header = reflect::field_table<Header>();
static_assert(header[0].offset == 0);
static_assert(header[1].offset == offsetof(Header, length));
static_assert(header[2].offset == offsetof(Header, checksum));

int main() {
    // The table is plain data: an ordinary loop, no templates in sight.
    std::string names;
    std::size_t bytes = 0;
    for (const reflect::field_info& field : table) {
        names += field.name;
        names += ' ';
        bytes += field.size;
        CHECK(!field.type_name.empty());
        CHECK(field.offset < sizeof(Packet));
    }
    CHECK(names == "kind length checksum urgent sequence ");
    CHECK(bytes == 1 + 4 + 2 + 8);

    // Offsets and sizes are enough to read a field out of raw memory.
    Header packet;
    packet.length = 0xDEADBEEF;
    std::uint32_t length = 0;
    std::memcpy(&length, reinterpret_cast<const char*>(&packet) + header[1].offset,
                header[1].size);
    CHECK(length == 0xDEADBEEF);

    // Look a field up by a run-time name.
    std::string wanted = "checksum";
    const reflect::field_info* found = nullptr;
    for (const reflect::field_info& field : table)
        if (field.name == wanted)
            found = &field;
    CHECK(found != nullptr && found->index == 2);

    return test::result();
}

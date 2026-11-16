// Inspect how a struct is laid out in memory.
//
// Useful when a struct has to match a file format, a network packet or a
// hardware register block: see the padding instead of guessing it.

#include <reflect/reflect.hpp>

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>

struct Header {
    std::uint8_t version;
    std::uint32_t length;
    std::uint16_t flags;
    std::uint64_t timestamp;
    std::uint8_t checksum;
};

// The same members, ordered from large to small.
struct PackedHeader {
    std::uint64_t timestamp;
    std::uint32_t length;
    std::uint16_t flags;
    std::uint8_t version;
    std::uint8_t checksum;
};

template <typename T>
void print_layout() {
    // field_table is plain data: an array of structs, usable in a normal loop.
    static constexpr auto fields = reflect::field_table<T>();

    std::cout << reflect::name_of<T>() << " (" << sizeof(T) << " bytes, alignment "
              << alignof(T) << ")\n";

    std::size_t end_of_previous = 0;
    std::size_t padding = 0;
    for (const reflect::field_info& field : fields) {
        if (field.offset > end_of_previous) {
            std::cout << "  " << std::setw(6) << end_of_previous << "  "
                      << field.offset - end_of_previous << " byte(s) of padding\n";
            padding += field.offset - end_of_previous;
        }
        std::cout << "  " << std::setw(6) << field.offset << "  " << std::left
                  << std::setw(10) << field.name << std::right << ' ' << field.size
                  << " byte(s)\n";
        end_of_previous = field.offset + field.size;
    }
    if (sizeof(T) > end_of_previous) {
        std::cout << "  " << std::setw(6) << end_of_previous << "  "
                  << sizeof(T) - end_of_previous << " byte(s) of padding\n";
        padding += sizeof(T) - end_of_previous;
    }
    std::cout << "  padding in total: " << padding << " byte(s)\n\n";
}

// The check can also be made at compile time, so a careless reorder of the
// members fails the build.
template <typename T>
constexpr bool has_no_padding() {
    std::size_t used = 0;
    for (const reflect::field_info& field : reflect::field_table<T>())
        used += field.size;
    return used == sizeof(T);
}
static_assert(!has_no_padding<Header>());
static_assert(has_no_padding<PackedHeader>());

int main() {
    print_layout<Header>();
    print_layout<PackedHeader>();
}

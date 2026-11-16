// Base classes and inherited fields.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct Entity {
    int id = 1;
};

struct Timestamped {
    long created = 1700000000;
    long updated = 1700000500;
};

struct Document : Entity, Timestamped {
    std::string title = "Report";
    int pages = 12;
};

// Prints a class and, indented below it, each of its base classes.
template <typename T>
void print_tree(int depth = 0) {
    std::cout << std::string(depth * 2, ' ') << reflect::name_of<T>() << " {";
    reflect::for_each_field<T>([](auto field) { std::cout << ' ' << field.name(); });
    std::cout << " }\n";

    reflect::for_each_base<T>([&](auto base) {
        // base::type is the base class; hand it back in to go one level up.
        print_tree<typename decltype(base)::type>(depth + 1);
    });
}

int main() {
    Document document;

    // get_fields_of lists only what Document declares itself.
    std::cout << "declared here:";
    reflect::for_each_field<Document>([](auto field) { std::cout << ' ' << field.name(); });
    std::cout << '\n';

    // get_all_fields_of adds the inherited ones, base classes first.
    std::cout << "all fields:\n";
    reflect::get_all_fields_of<Document>().for_each([&](auto field) {
        using Owner = typename decltype(field)::declaring_type;
        std::cout << "  " << reflect::name_of<Owner>() << "::" << field.name() << " = "
                  << field.get(document) << '\n';
    });

    std::cout << "class tree:\n";
    print_tree<Document>();

    // A base proxy can hand out the base part of an object.
    reflect::for_each_base<Document>([&](auto base) {
        using Base = typename decltype(base)::type;
        const Base& part = base.get(document);
        std::cout << reflect::name_of<Base>() << " part has "
                  << reflect::get_fields_of<Base>().size() << " field(s), "
                  << sizeof(part) << " bytes\n";
    });
}

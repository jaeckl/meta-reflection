// A builder for any struct, without generated code.
//
// reflect::builder checks the field names while compiling, and refuses to
// build an object as long as a field marked [[=reflect::required]] is
// missing.

#include <reflect/reflect.hpp>

#include <iostream>
#include <string>

struct Connection {
    [[=reflect::required]] std::string host;
    [[=reflect::required]] int port = 0;
    int timeout_seconds = 30;
    bool encrypted = true;
};

void print(const Connection& connection) {
    std::cout << connection.host << ':' << connection.port << ", timeout "
              << connection.timeout_seconds << "s, "
              << (connection.encrypted ? "encrypted" : "plain") << '\n';
}

int main() {
    // The calls can come in any order. Fields that are not set keep their
    // default.
    Connection connection = reflect::builder<Connection>()
                                .set<"port">(443)
                                .set<"host">("example.org")
                                .set<"timeout_seconds">(5)
                                .build();
    print(connection);

    // A missing required field is a compile error:
    //
    //     reflect::builder<Connection>().set<"host">("example.org").build();
    //
    //     error: ... 'reflect::builder::build: the required field 'port'
    //     was not set'

    // A half-finished builder can be kept and continued more than once.
    // Each set() returns a new type, so the variable is `auto`.
    auto local = reflect::builder<Connection>().set<"host">("localhost").set<"encrypted">(false);
    print(local.set<"port">(8080).build());
    print(local.set<"port">(9090).build());
}

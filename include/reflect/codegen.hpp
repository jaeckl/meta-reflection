#pragma once

// Experimental: everything a code generator program needs.
//
// This header is for generator programs, which run while a project is
// built. It is not part of <reflect/reflect.hpp>, and the annotations that
// processors read (reflect::generate_builder, reflect::required) do not
// need it.

#include "reflect.hpp"

#include "codegen/builder_processor.hpp"
#include "codegen/context.hpp"
#include "codegen/discover.hpp"
#include "codegen/registry_processor.hpp"
#include "codegen/run.hpp"
#include "codegen/writer.hpp"

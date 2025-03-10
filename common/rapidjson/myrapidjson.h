#ifndef __MYSYSTEM_INCLUDE_RAPIDJSON
#define __MYSYSTEM_INCLUDE_RAPIDJSON

#include <cstddef>
#include <cstdint>
#include <string>

// SIMD optimization
// #define RAPIDJSON_SSE2
// or
// #define RAPIDJSON_SSE42
// or
// #define RAPIDJSON_NEON 
// or
// not enable

#  define RAPIDJSON_HAS_CXX11_TYPETRAITS 1
#  define RAPIDJSON_HAS_CXX11_NOEXCEPT 1
#  define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#  define RAPIDJSON_HAS_CXX11_RANGE_FOR 1
#  define RAPIDJSON_HAS_STDSTRING 1
#  define RAPIDJSON_NO_INT64DEFINE 
   namespace rapidjson { typedef ::std::uint64_t uint64_t; typedef ::std::int64_t int64_t;}
#  define RAPIDJSON_NO_SIZETYPEDEFINE 
   namespace rapidjson { typedef ::std::size_t SizeType;}
#  include "rapidjson.h"
#  include "allocators.h"
#  include "encodedstream.h"
#  include "filereadstream.h"
#  include "memorystream.h"
#  include "pointer.h"
#  include "reader.h"
#  include "stringbuffer.h"
#  include "cursorstreamwrapper.h"
#  include "encodings.h"
#  include "filewritestream.h"
#  include "istreamwrapper.h"
#  include "prettywriter.h"
#  include "schema.h"
#  include "writer.h"
#  include "document.h"
#  include "fwd.h"
#  include "memorybuffer.h"
#  include "ostreamwrapper.h"
#  include "stream.h"
#  include "error/en.h"
#endif

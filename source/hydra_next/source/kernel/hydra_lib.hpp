#pragma once

//
// Hydra Lib - v0.30
//
// Header to track different core libraries within Hydra framework.
//
//      Source code     : https://www.github.com/jorenjoestar/
//
//      Created         : 2019/06/20, 19.23
//
// Files /////////////////////////////////
//
// array.hpp, assert.hpp/.cpp, bit.hpp/.cpp, blob_serialization.hpp/.cpp, data_structures.hpp/.cpp,
// file.hpp/.cpp, hash_map.hpp, log.hpp/.cpp, memory.hpp/.cpp, memory_utils.hpp, numerics.hpp/.cpp,
// platform.hpp, primitive_types.hpp, process.hpp/.cpp, relative_data_structures.hpp, service.hpp/.cpp,
// service_manager.hpp/.cpp, string.hpp/.cpp, time.hpp/.cpp .
//
// Revision history //////////////////////
//
//      0.30 (2021/10/22): + Fixed hash map iterator behaviour, added clear method.
//                         + Added file_name_from_path to retrieve just the file name from a path.
//                         + Added StringArray implementation based on FlatHashMap.
//      0.29 (2021/10/21): + Changed MemoryBlob to BlobSerializer, added Blob and BlobHeader. Added write_serialize and write_prepare
//                           to separate different writing methods.
//      0.28 (2021/10/21): + Added directory_current method to retrieve current process directory.
//      0.27 (2021/10/19): + Blob serialization rewrite.
//      0.26 (2021/10/10): + Added hash map default key/value and method to change it.
//                         + Added hashing specialization for written strings. + Fixed hash map 'get structure' methods.
//      0.25 (2021/10/09): + Finished serialization of versioned Relative data structures. + Added set_null and set_empty methods to Relative data structures.
//      0.24 (2021/10/07): + Added RelativePointer serialization. + Fixed zero-sized array serialization.
//      0.23 (2021/09/30): + Added ResourcePoolType templated data structure.
//		0.22 (2021/09/28): + Restored tracking of different libraries within the kernel layer.
//      0.21 (2021/05/11): + Created dynamic array.
//      0.20 (2021/01/27): + Added proper memory macros - halloc, hfree, halloca, hfreea.
//      0.19 (2021/01/02): + BREAKING: changed file_read methods to binary and text versions. + Added float/double to int/uint ceil/floor/round methods.
//      0.18 (2021/01/01): + Renamed infamous macro 'array' in 'array_type'.
//      0.17 (2020/12/27): + Added allocator to file_read_in_memory function. + Added optional leak detections on memory service shutdown.
//      0.16 (2020/12/27): + Added RingBufferFloat - class to use for static sized values that needs to be updated constantly.
//      0.15 (2020/12/23): + Added typedef for native types.
//      0.14 (2020/12/21): + Separated StrinBuffer variadic method from just string one. + Fixed bug in reading text.
//      0.13 (2020/12/20): + Added custom printf callback.
//      0.12 (2020/03/23): + Added stdout and stderr redirection when executing a process. + Added method to remove filename from a path.
//      0.11 (2020/03/20): + Added Directory struct and utility functions to navigate filesystem.
//      0.10 (2020/03/18): + Added memory allocators to file library + Added memory allocators to StringBuffer and StringArray + Fixed file_write function.
//      0.09 (2020/03/16): + Added Memory Service and first Allocator.
//      0.08 (2020/03/11): + Moved StringArray methods inside the struct. + Renamed lenu methods to size.
//      0.07 (2020/03/10): + Renamed all file methods to start with "file_".
//      0.06 (2020/03/08): + Changed all StringBuffer methods with size to use size_t. + Moved StringRef methods as static inside the struct.
//      0.05 (2020/03/02): + Implemented Time Subsystem. + Improved Execute Process error message.
//      0.04 (2020/02/27): + Removal of STB-dependent parts
//      0.03 (2019/12/17): + Interface cleanup. + Added array init macro.
//      0.02 (2019/09/26): + added stb.
//      0.01 (2019/06/20): + initial implementation.
//

// TODO
// - Implement memory debugging UI
// - Implement gpu device debugging UI
// - Implement callstack retrieval

// DONE
//
// + Implement string buffer using array
// + Implement array
// + Implement hash map
// + Implement logging using service
// + Implement assert using service

#include "kernel/primitive_types.hpp"
#include "kernel/platform.hpp"


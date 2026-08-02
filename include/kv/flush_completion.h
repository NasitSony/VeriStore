#pragma once

#include <string>

namespace kv {

struct FlushCompletion {
    std::string sstable_path;
};

}
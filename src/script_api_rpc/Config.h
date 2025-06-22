#pragma once

namespace script_api_rpc {

struct Config {
    int  version                = 1;
    bool doGiveClockOnFirstJoin = true;
    bool enableClockMenu        = true;
    bool registerSuicideCommand = true;
};

} // namespace script_api_rpc
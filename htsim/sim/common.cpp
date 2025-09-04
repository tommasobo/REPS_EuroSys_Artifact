#include "failuregenerator.h"
#include <filesystem>
#include <stdint.h>
#include <sys/types.h>
#include <unordered_map>
#include <string>

typedef uint64_t simtime_picosec;

std::string SAVE_DATA_FOLDER = ""; // By default same folder as the input file
failuregenerator *FAILURE_GENERATOR = new failuregenerator();
simtime_picosec GLOBAL_TIME = 0; // Global variable for current sim time
std::unordered_map<std::string, ConnectionInfo> CONNECTION_INFO_MAP;



#pragma once

#include "LifeTypes.h"
#include <string>

// Parse RLE (Run Length Encoded) format into PlacedGrid
// RLE format: https://conwaylife.com/wiki/Run_Length_Encoded
// Example: "3o$2bo$bo!" represents a glider
class RLEParser
{
public:
    // Parse RLE string into a PlacedGrid
    // The grid will be sized to fit the pattern with optional padding
    static PlacedGrid parse(const char* rle, int padding = 2);
    
private:
    static void skip_whitespace_and_comments(const char*& ptr, const char* end);
    static int parse_number(const char*& ptr, const char* end);
};

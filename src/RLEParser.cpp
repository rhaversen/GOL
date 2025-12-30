#include "RLEParser.h"
#include "BitGrid.h"
#include <cctype>
#include <cstring>
#include <stdexcept>

void RLEParser::skip_whitespace_and_comments(const char*& ptr, const char* end)
{
    while (ptr < end)
    {
        if (std::isspace(*ptr))
        {
            ++ptr;
        }
        else if (*ptr == '#')
        {
            // Skip until newline
            while (ptr < end && *ptr != '\n')
                ++ptr;
        }
        else
        {
            break;
        }
    }
}

int RLEParser::parse_number(const char*& ptr, const char* end)
{
    if (ptr >= end || !std::isdigit(*ptr))
        return 1; // Default count is 1
    
    int num = 0;
    while (ptr < end && std::isdigit(*ptr))
    {
        num = num * 10 + (*ptr - '0');
        ++ptr;
    }
    return num;
}

PlacedGrid RLEParser::parse(const char* rle, int padding)
{
    const char* ptr = rle;
    const char* end = ptr + std::strlen(rle);
    
    // Parse header (x = w, y = h, rule = ...)
    skip_whitespace_and_comments(ptr, end);
    
    int width = 0, height = 0;
    
    // Look for x = ... and y = ...
    while (ptr < end)
    {
        skip_whitespace_and_comments(ptr, end);
        
        if (ptr >= end)
            break;
        
        if (*ptr == 'x')
        {
            ++ptr;
            skip_whitespace_and_comments(ptr, end);
            if (ptr < end && *ptr == '=')
            {
                ++ptr;
                skip_whitespace_and_comments(ptr, end);
                width = parse_number(ptr, end);
            }
        }
        else if (*ptr == 'y')
        {
            ++ptr;
            skip_whitespace_and_comments(ptr, end);
            if (ptr < end && *ptr == '=')
            {
                ++ptr;
                skip_whitespace_and_comments(ptr, end);
                height = parse_number(ptr, end);
            }
        }
        else if (*ptr == 'r')
        {
            // Skip rule specification
            while (ptr < end && *ptr != '\n')
                ++ptr;
        }
        else if (*ptr == ',' || *ptr == '\n')
        {
            ++ptr;
        }
        else
        {
            // Pattern data starts
            break;
        }
    }
    
    // If no header, we'll determine size from pattern
    if (width == 0 || height == 0)
    {
        // Do a quick pre-parse to determine size
        const char* temp_ptr = ptr;
        int max_x = 0, max_y = 0;
        int x = 0, y = 0;
        
        while (temp_ptr < end)
        {
            skip_whitespace_and_comments(temp_ptr, end);
            if (temp_ptr >= end)
                break;
            
            int count = parse_number(temp_ptr, end);
            
            if (temp_ptr >= end)
                break;
            
            char tag = *temp_ptr++;
            
            if (tag == 'b' || tag == 'o')
            {
                x += count;
                if (x > max_x)
                    max_x = x;
            }
            else if (tag == '$')
            {
                y += count;
                if (y > max_y)
                    max_y = y;
                x = 0;
            }
            else if (tag == '!')
            {
                break;
            }
        }
        
        width = max_x;
        height = max_y + 1;
    }
    
    // Create grid with padding
    PlacedGrid pg;
    pg.grid = BitGrid(width + 2 * padding, height + 2 * padding);
    pg.offset_x = 0;
    pg.offset_y = 0;
    
    // Parse pattern data
    int x = padding;
    int y = padding;
    
    skip_whitespace_and_comments(ptr, end);
    
    while (ptr < end)
    {
        skip_whitespace_and_comments(ptr, end);
        
        if (ptr >= end)
            break;
        
        int count = parse_number(ptr, end);
        
        if (ptr >= end)
            break;
        
        char tag = *ptr++;
        
        if (tag == 'b')
        {
            // Dead cells - just advance x
            x += count;
        }
        else if (tag == 'o')
        {
            // Live cells
            for (int i = 0; i < count; ++i)
            {
                pg.grid.set(x, y);
                ++x;
            }
        }
        else if (tag == '$')
        {
            // End of line(s)
            y += count;
            x = padding;
        }
        else if (tag == '!')
        {
            // End of pattern
            break;
        }
    }
    
    return pg;
}

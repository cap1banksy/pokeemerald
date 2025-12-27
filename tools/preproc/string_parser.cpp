// Copyright(c) 2016 YamaArashi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <cstdio>
#include <cstdarg>
#include <stdexcept>
#include <vector>
#include "preproc.h"
#include "string_parser.h"
#include "char_util.h"
#include "utf8.h"
#include "persian_shaper.h"

// Reads a charmap constant, i.e. "{FOO}".
std::string StringParser::ReadBracketedConstants()
{
    std::string totalSequence;

    m_pos++; // Assume we're on the left curly bracket.

    while (m_buffer[m_pos] != '}')
    {
        SkipWhitespace();

        if (IsIdentifierStartingChar(m_buffer[m_pos]))
        {
            long startPos = m_pos;

            m_pos++;

            while (IsIdentifierChar(m_buffer[m_pos]))
                m_pos++;

            std::string sequence = g_charmap->Constant(std::string(&m_buffer[startPos], m_pos - startPos));

            if (sequence.length() == 0)
            {
                m_buffer[m_pos] = 0;
                RaiseError("unknown constant '%s'", &m_buffer[startPos]);
            }

            totalSequence += sequence;
        }
        else if (IsAsciiDigit(m_buffer[m_pos]))
        {
            Integer integer = ReadInteger();

            switch (integer.size)
            {
            case 1:
                totalSequence += (unsigned char)integer.value;
                break;
            case 2:
                totalSequence += (unsigned char)integer.value;
                totalSequence += (unsigned char)(integer.value >> 8);
                break;
            case 4:
                totalSequence += (unsigned char)integer.value;
                totalSequence += (unsigned char)(integer.value >> 8);
                totalSequence += (unsigned char)(integer.value >> 16);
                totalSequence += (unsigned char)(integer.value >> 24);
                break;
            }
        }
        else if (m_buffer[m_pos] == 0)
        {
            if (m_pos >= m_size)
                RaiseError("unexpected EOF after left curly bracket");
            else
                RaiseError("unexpected null character within curly brackets");
        }
        else
        {
            if (IsAsciiPrintable(m_buffer[m_pos]))
                RaiseError("unexpected character '%c' within curly brackets", m_buffer[m_pos]);
            else
                RaiseError("unexpected character '\\x%02X' within curly brackets", m_buffer[m_pos]);
        }
    }

    m_pos++; // Go past the right curly bracket.

    return totalSequence;
}

// Reads a single character or escape and returns it as a ParsedElement,
// preserving the codepoint for later shaping.
ParsedElement StringParser::ReadCharOrEscapeElement()
{
    ParsedElement elem;

    bool isEscape = (m_buffer[m_pos] == '\\');

    if (isEscape)
    {
        m_pos++;

        if (m_buffer[m_pos] == '"')
        {
            // Escaped double quote - return as sequence since it's special
            elem.type = ParsedElement::Type::Sequence;
            elem.sequence = g_charmap->Char('"');
            if (elem.sequence.length() == 0)
                RaiseError("no mapping exists for double quote");
            m_pos++;
            return elem;
        }
        else if (m_buffer[m_pos] == '\\')
        {
            // Escaped backslash - return as sequence since it's special
            elem.type = ParsedElement::Type::Sequence;
            elem.sequence = g_charmap->Char('\\');
            if (elem.sequence.length() == 0)
                RaiseError("no mapping exists for backslash");
            m_pos++;
            return elem;
        }
    }

    unsigned char c = m_buffer[m_pos];

    if (c == 0)
    {
        if (m_pos >= m_size)
            RaiseError("unexpected EOF in UTF-8 string");
        else
            RaiseError("unexpected null character in UTF-8 string");
    }

    if (IsAscii(c) && !IsAsciiPrintable(c))
        RaiseError("unexpected character U+%X in UTF-8 string", c);

    UnicodeChar unicodeChar = DecodeUtf8(&m_buffer[m_pos]);
    m_pos += unicodeChar.encodingLength;
    std::int32_t code = unicodeChar.code;

    if (code == -1)
        RaiseError("invalid encoding in UTF-8 string");

    if (isEscape && code >= 128)
        RaiseError("escapes using non-ASCII characters are invalid");

    if (isEscape)
    {
        // Escape sequences are returned as pre-resolved sequences (not shaped)
        elem.type = ParsedElement::Type::Sequence;
        elem.sequence = g_charmap->Escape(code);
        if (elem.sequence.length() == 0)
            RaiseError("unknown escape '\\%c'", code);
    }
    else
    {
        // Regular character - preserve codepoint for shaping
        elem.type = ParsedElement::Type::Codepoint;
        elem.codepoint = code;
    }

    return elem;
}

// Reads a charmap string with Persian/Arabic text shaping.
int StringParser::ParseString(long srcPos, unsigned char* dest, int& destLength)
{
    m_pos = srcPos;

    if (m_buffer[m_pos] != '"')
        RaiseError("expected UTF-8 string literal");

    long start = m_pos;

    m_pos++;

    // First pass: collect all elements
    std::vector<ParsedElement> elements;

    while (m_buffer[m_pos] != '"')
    {
        if (m_buffer[m_pos] == '{')
        {
            // Bracketed constants are pre-resolved (not shaped)
            ParsedElement elem;
            elem.type = ParsedElement::Type::Sequence;
            elem.sequence = ReadBracketedConstants();
            elements.push_back(std::move(elem));
        }
        else
        {
            elements.push_back(ReadCharOrEscapeElement());
        }
    }

    // Second pass: apply Persian shaping to codepoint runs
    // We need to extract codepoints, shape them, and put them back
    std::vector<std::int32_t> codepoints;
    std::vector<size_t> codepointIndices; // Maps codepoint index -> element index

    for (size_t i = 0; i < elements.size(); ++i)
    {
        if (elements[i].type == ParsedElement::Type::Codepoint)
        {
            codepoints.push_back(elements[i].codepoint);
            codepointIndices.push_back(i);
        }
    }

    // Apply Persian/Arabic shaping
    if (!codepoints.empty())
    {
        ShapePersianText(codepoints);

        // Note: shaping may change the number of codepoints (e.g., lam-alef ligature)
        // We need to handle this by rebuilding the elements list
        if (codepoints.size() != codepointIndices.size())
        {
            // Shaping changed the count - rebuild elements
            std::vector<ParsedElement> newElements;
            size_t cpIdx = 0;
            for (size_t i = 0; i < elements.size(); ++i)
            {
                if (elements[i].type == ParsedElement::Type::Sequence)
                {
                    newElements.push_back(std::move(elements[i]));
                }
                else
                {
                    // This was a codepoint - check if we still have shaped codepoints
                    // Skip original codepoints that were merged into ligatures
                    if (cpIdx < codepoints.size())
                    {
                        ParsedElement elem;
                        elem.type = ParsedElement::Type::Codepoint;
                        elem.codepoint = codepoints[cpIdx++];
                        newElements.push_back(elem);
                    }
                    // If cpIdx >= codepoints.size(), this codepoint was consumed by a ligature
                }
            }
            // Add any remaining shaped codepoints (shouldn't happen, but be safe)
            while (cpIdx < codepoints.size())
            {
                ParsedElement elem;
                elem.type = ParsedElement::Type::Codepoint;
                elem.codepoint = codepoints[cpIdx++];
                newElements.push_back(elem);
            }
            elements = std::move(newElements);
        }
        else
        {
            // Same count - just update the codepoints in place
            for (size_t i = 0; i < codepoints.size(); ++i)
            {
                elements[codepointIndices[i]].codepoint = codepoints[i];
            }
        }
    }

    // Third pass: convert all elements to bytes
    destLength = 0;

    for (const ParsedElement& elem : elements)
    {
        std::string sequence;

        if (elem.type == ParsedElement::Type::Sequence)
        {
            sequence = elem.sequence;
        }
        else
        {
            // Skip consumed codepoints
            if (elem.codepoint == -1)
                continue;

            sequence = g_charmap->Char(elem.codepoint);
            if (sequence.length() == 0)
                RaiseError("unknown character U+%X", elem.codepoint);
        }

        for (const char& c : sequence)
        {
            if (destLength == kMaxStringLength)
                RaiseError("mapped string longer than %d bytes", kMaxStringLength);

            dest[destLength++] = c;
        }
    }

    m_pos++; // Go past the right quote.

    return m_pos - start;
}

void StringParser::RaiseError(const char* format, ...)
{
    const int bufferSize = 1024;
    char buffer[bufferSize];

    std::va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, bufferSize, format, args);
    va_end(args);

    throw std::runtime_error(buffer);
}

// Converts digit character to numerical value.
static int ConvertDigit(char c, int radix)
{
    int digit;

    if (c >= '0' && c <= '9')
        digit = c - '0';
    else if (c >= 'A' && c <= 'F')
        digit = 10 + c - 'A';
    else if (c >= 'a' && c <= 'f')
        digit = 10 + c - 'a';
    else
        return -1;

    return (digit < radix) ? digit : -1;
}

void StringParser::SkipRestOfInteger(int radix)
{
    while (ConvertDigit(m_buffer[m_pos], radix) != -1)
        m_pos++;
}

StringParser::Integer StringParser::ReadDecimal()
{
    const int radix = 10;
    std::uint64_t n = 0;
    int digit;
    std::uint64_t max = UINT32_MAX;
    long startPos = m_pos;

    while ((digit = ConvertDigit(m_buffer[m_pos], radix)) != -1)
    {
        n = n * radix + digit;

        if (n >= max)
        {
            SkipRestOfInteger(radix);

            std::string intLiteral(m_buffer + startPos, m_pos - startPos);
            RaiseError("integer literal \"%s\" is too large", intLiteral.c_str());
        }

        m_pos++;
    }

    int size;

    if (m_buffer[m_pos] == 'H')
    {
        if (n >= 0x10000)
        {
            RaiseError("%lu is too large to be a halfword", (unsigned long)n);
        }

        size = 2;
        m_pos++;
    }
    else if (m_buffer[m_pos] == 'W')
    {
        size = 4;
        m_pos++;
    }
    else
    {
        if (n >= 0x10000)
            size = 4;
        else if (n >= 0x100)
            size = 2;
        else
            size = 1;
    }

    return{ static_cast<std::uint32_t>(n), size };
}

StringParser::Integer StringParser::ReadHex()
{
    const int radix = 16;
    std::uint64_t n = 0;
    int digit;
    std::uint64_t max = UINT32_MAX;
    long startPos = m_pos;

    while ((digit = ConvertDigit(m_buffer[m_pos], radix)) != -1)
    {
        n = n * radix + digit;

        if (n >= max)
        {
            SkipRestOfInteger(radix);

            std::string intLiteral(m_buffer + startPos, m_pos - startPos);
            RaiseError("integer literal \"%s\" is too large", intLiteral.c_str());
        }

        m_pos++;
    }

    int length = m_pos - startPos;
    int size = 0;

    switch (length)
    {
    case 2:
        size = 1;
        break;
    case 4:
        size = 2;
        break;
    case 8:
        size = 4;
        break;
    default:
    {
        std::string intLiteral(m_buffer + startPos, m_pos - startPos);
        RaiseError("hex integer literal \"0x%s\" doesn't have length of 2, 4, or 8 digits", intLiteral.c_str());
    }
    }

    return{ static_cast<std::uint32_t>(n), size };
}

StringParser::Integer StringParser::ReadInteger()
{
    if (!IsAsciiDigit(m_buffer[m_pos]))
        RaiseError("expected integer");

    if (m_buffer[m_pos] == '0' && m_buffer[m_pos + 1] == 'x')
    {
        m_pos += 2;
        return ReadHex();
    }

    return ReadDecimal();
}

// Skips tabs and spaces.
void StringParser::SkipWhitespace()
{
    while (m_buffer[m_pos] == '\t' || m_buffer[m_pos] == ' ')
        m_pos++;
}

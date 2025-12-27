#include <cstddef>
#include <unordered_map>
#include "persian_shaper.h"

struct GlyphForms
{
    std::int32_t isolated;
    std::int32_t initial;
    std::int32_t medial;
    std::int32_t final_;
};

static const std::unordered_map<std::int32_t, GlyphForms> s_glyphDictionary = {
    {0x0627, {0xFE8D, 0xFE8D, 0xFE8E, 0xFE8E}}, // Alef
    {0x0622, {0xFE81, 0xFE81, 0xFE82, 0xFE82}}, // Alef with Madda
    {0x0628, {0xFE8F, 0xFE91, 0xFE92, 0xFE90}}, // Beh
    {0x067E, {0xFB56, 0xFB58, 0xFB59, 0xFB57}}, // Peh (Persian)
    {0x062A, {0xFE95, 0xFE97, 0xFE98, 0xFE96}}, // Teh
    {0x062B, {0xFE99, 0xFE9B, 0xFE9C, 0xFE9A}}, // Theh
    {0x062C, {0x062C, 0xFE9F, 0xFEA0, 0xFE9E}}, // Jeem
    {0x0686, {0xFB7A, 0xFB7C, 0xFB7D, 0xFB7B}}, // Tcheh (Persian)
    {0x062D, {0xFEA1, 0xFEA3, 0xFEA4, 0xFEA2}}, // Hah
    {0x062E, {0xFEA5, 0xFEA7, 0xFEA8, 0xFEA6}}, // Khah
    {0x062F, {0x062F, 0x062F, 0xFEAA, 0xFEAA}}, // Dal
    {0x0630, {0x0630, 0x0630, 0xFEAC, 0xFEAC}}, // Thal
    {0x0631, {0x0631, 0x0631, 0xFEAE, 0xFEAE}}, // Reh
    {0x0632, {0xFEAF, 0xFEAF, 0xFEB0, 0xFEB0}}, // Zain
    {0x0698, {0xFB8A, 0xFB8A, 0xFB8B, 0xFB8B}}, // Jeh (Persian)
    {0x0633, {0x0633, 0xFEB3, 0xFEB4, 0xFEB2}}, // Seen
    {0x0634, {0xFEB5, 0xFEB7, 0xFEB8, 0xFEB6}}, // Sheen
    {0x0635, {0x0635, 0xFEBB, 0xFEBC, 0xFEBA}}, // Sad
    {0x0636, {0x0636, 0xFEBF, 0xFEC0, 0xFEBE}}, // Dad
    {0x0637, {0xFEC1, 0xFEC3, 0xFEC4, 0xFEC2}}, // Tah
    {0x0638, {0xFEC5, 0xFEC7, 0xFEC8, 0xFEC6}}, // Zah
    {0x0639, {0x0639, 0xFECB, 0xFECC, 0xFECA}}, // Ain
    {0x063A, {0x063A, 0xFECF, 0xFED0, 0xFECE}}, // Ghain
    {0x0641, {0xFED1, 0xFED3, 0xFED4, 0xFED2}}, // Feh
    {0x0642, {0xFED5, 0xFED7, 0xFED8, 0xFED6}}, // Qaf
    {0x06A9, {0xFB8E, 0xFB90, 0xFB91, 0xFB8F}}, // Kaf (Persian)
    {0x06AF, {0xFB92, 0xFB94, 0xFB95, 0xFB93}}, // Gaf (Persian)
    {0x0644, {0x0644, 0xFEDF, 0xFEE0, 0xFEDE}}, // Lam
    {0x0645, {0x0645, 0xFEE3, 0xFEE4, 0xFEE2}}, // Meem
    {0x0646, {0xFEE5, 0xFEE7, 0xFEE8, 0xFEE6}}, // Noon
    {0x0648, {0xFEED, 0xFEED, 0xFEEE, 0xFEEE}}, // Waw
    {0x0647, {0xFEE9, 0xFEEB, 0xFEEC, 0xFEEA}}, // Heh
    {0x06CC, {0xFBFC, 0xFBFE, 0xFBFF, 0xFBFD}}, // Yeh (Persian)
    {0xFEFB, {0xFEFB, 0xFEFB, 0xFEFC, 0xFEFC}}, // Lam-Alef ligature
};

// Zero-width non-joiner
static const std::int32_t ZWNJ = 0x200C;

// Base codepoints for Lam-Alef detection
static const std::int32_t LAM = 0x0644;
static const std::int32_t ALEF = 0x0627;
static const std::int32_t LAM_ALEF_LIGATURE = 0xFEFB;

bool IsPersianLetter(std::int32_t codepoint)
{
    return s_glyphDictionary.find(codepoint) != s_glyphDictionary.end();
}

// Check if there's a Persian letter after this position
static bool HasFollowingChar(const std::vector<std::int32_t>& codepoints, size_t index)
{
    if (index >= codepoints.size() - 1)
        return false;

    return s_glyphDictionary.find(codepoints[index + 1]) != s_glyphDictionary.end();
}

// Check if this position is the start of a Lam-Alef ligature
static bool IsLamAlef(const std::vector<std::int32_t>& codepoints, size_t index)
{
    if (index >= codepoints.size() - 1)
        return false;

    return codepoints[index] == LAM && codepoints[index + 1] == ALEF;
}

// Check if letter at index is a connecting letter (isolated != initial)
static bool IsConnectingLetter(const std::vector<std::int32_t>& codepoints, size_t index)
{
    auto it = s_glyphDictionary.find(codepoints[index]);
    if (it == s_glyphDictionary.end())
        return false;
    // A letter connects if its isolated form differs from its initial form
    return it->second.isolated != it->second.initial;
}

// Check if there's a connecting Persian letter before this position (matches Python exactly)
static bool HasPrecedingChar(const std::vector<std::int32_t>& codepoints, size_t index)
{
    if (index == 0)
        return false;

    auto it = s_glyphDictionary.find(codepoints[index - 1]);
    if (it == s_glyphDictionary.end())
        return false;

    // Check if previous letter is a connecting letter
    return IsConnectingLetter(codepoints, index - 1);
}

void ShapePersianText(std::vector<std::int32_t>& codepoints)
{
    std::vector<std::int32_t> result;
    result.reserve(codepoints.size());

    for (size_t i = 0; i < codepoints.size(); ++i)
    {
        std::int32_t cp = codepoints[i];

        auto it = s_glyphDictionary.find(cp);
        if (it != s_glyphDictionary.end())
        {
            const GlyphForms& forms = it->second;
            bool hasFollowing = HasFollowingChar(codepoints, i);
            bool hasPreceding = HasPrecedingChar(codepoints, i);
            bool isConnecting = IsConnectingLetter(codepoints, i);

            std::int32_t shapedChar;

            if (!isConnecting)
            {
                // Non-connecting letter (alef, dal, reh, waw, etc.)
                if (hasPreceding)
                {
                    // Check if previous output char was a lam-alef ligature
                    if (!result.empty() && (result.back() == 0xFEFB || result.back() == 0xFEFC))
                    {
                        // Mark this alef as consumed (part of the ligature)
                        result.push_back(-1);
                        continue;
                    }
                    shapedChar = forms.medial; // final form for non-connecting
                }
                else
                {
                    shapedChar = forms.isolated;
                }
            }
            else
            {
                // Connecting letter
                if (hasFollowing)
                {
                    // Check for Lam-Alef ligature
                    if (IsLamAlef(codepoints, i))
                    {
                        // Get the lam-alef ligature forms
                        auto lamAlefIt = s_glyphDictionary.find(LAM_ALEF_LIGATURE);
                        if (lamAlefIt != s_glyphDictionary.end())
                        {
                            if (hasPreceding)
                            {
                                shapedChar = lamAlefIt->second.medial; // 0xFEFC
                            }
                            else
                            {
                                shapedChar = lamAlefIt->second.isolated; // 0xFEFB
                            }
                            result.push_back(shapedChar);
                            // Don't skip alef here - let the next iteration handle it
                            // (it will be skipped by the ligature check above)
                            continue;
                        }
                    }

                    if (hasPreceding)
                    {
                        shapedChar = forms.medial;
                    }
                    else
                    {
                        shapedChar = forms.initial;
                    }
                }
                else if (hasPreceding)
                {
                    shapedChar = forms.final_;
                }
                else
                {
                    shapedChar = forms.isolated;
                }
            }

            result.push_back(shapedChar);
        }
        else
        {
            // Not a Persian letter
            // Skip zero-width non-joiner
            if (cp == ZWNJ)
                continue;
            // Pass through unchanged
            result.push_back(cp);
        }
    }

    codepoints = std::move(result);
}

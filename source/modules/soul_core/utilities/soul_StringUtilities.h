/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

namespace soul
{

bool isWhitespace (char);
bool isDigit (char);

std::string repeatedCharacter (char, size_t num);
std::string padded (const std::string&, int minSize);

bool containsChar (const std::string&, char) noexcept;
bool containsChar (const char*, char) noexcept;

std::string trimCharacterAtStart (const std::string& s, char charToRemove);
std::string trimStart (const std::string&);
std::string trimEnd (std::string);
std::string trim (std::string);

template <typename StringType>
static inline bool contains (const std::string& text, const StringType& substring)
{
    return text.find (substring) != std::string::npos;
}

bool startsWith (const std::string& text, char possibleStart);
bool startsWith (const std::string& text, const char* possibleStart);
bool startsWith (const std::string& text, const std::string& possibleStart);
bool endsWith (const std::string& text, const std::string& possibleEnd);

std::string replaceSubString (std::string s, const std::string& toReplace, const std::string& replacement);
std::string retainCharacters (std::string s, const std::string& charactersToRetain);
std::string removeCharacter (std::string s, char charToRemove);

template <typename StringArray>
static std::string joinStrings (const StringArray& strings, const std::string& separator)
{
    if (strings.empty())
        return {};

    std::string s (strings.front());

    for (size_t i = 1; i < strings.size(); ++i)
    {
        s += separator;
        s += strings[i];
    }

    return s;
}

template <typename Array, typename StringifyFn>
static std::string joinStrings (const Array& strings, const std::string& separator, StringifyFn&& stringify)
{
    if (strings.empty())
        return {};

    std::string s (stringify (strings.front()));

    for (size_t i = 1; i < strings.size(); ++i)
    {
        s += separator;
        s += stringify (strings[i]);
    }

    return s;
}

std::vector<std::string> splitAtDelimiter (const std::string& text, char delimiter);
std::vector<std::string> splitAtWhitespace (const std::string& text);
std::vector<std::string> splitIntoLines (const std::string& text);
std::vector<std::string> splitLinesOfCode (const std::string& text, size_t targetLineLength);
size_t getMaxLineLength (const std::string& textWithLines);

std::string loadFileAsString (const char* filename);

std::string replaceLine (const std::string& text, size_t line, const std::string& replacementLine);
std::string makeSafeIdentifierName (std::string);
bool isSafeIdentifierName (std::string);
std::string makeIdentifierRemovingColons (std::string);

template <typename IsAlreadyUsedFn>
static std::string addSuffixToMakeUnique (const std::string& name, IsAlreadyUsedFn&& isUsed)
{
    auto nameToUse = name;
    int suffix = 1;

    while (isUsed (nameToUse))
        nameToUse = name + "_" + std::to_string (++suffix);

    return nameToUse;
}

std::string toStringWithDecPlaces (double n, size_t numDecPlaces);
std::string floatToAccurateString (float n);
std::string doubleToAccurateString (double n);
std::string getDescriptionOfTimeInSeconds (double numSeconds);
std::string toHexString (int64_t value);
std::string toHexString (int64_t value, int numDigits);
int getHexDigitValue (uint32_t digit) noexcept;
std::string doubleToJSONString (double n);
std::string getReadableDescriptionOfByteSize (uint64_t bytes);
size_t levenshteinDistance (const std::string&, const std::string&);
std::string toCppStringLiteral (const std::string& text,
                                int maxCharsOnLine, bool breakAtNewLines,
                                bool replaceSingleQuotes, bool allowStringBreaks);
std::string toJSONString (const std::string& text);

//==============================================================================
std::string addDoubleQuotes (const std::string&);
std::string addSingleQuotes (const std::string&);
std::string removeDoubleQuotes (const std::string&);

std::string convertToString (const std::string& name);
std::string convertToString (const Identifier& name);
std::string convertToString (const IdentifierPath& name);

// These apply a standard quoting style for things like a variable or type name.
std::string quoteName (const std::string& name);
std::string quoteName (const Identifier& name);

//==============================================================================
/** Creates a table of strings, where each column gets padded out based on the longest
    item that it contains. Use calls to startRow/appendItem to create the table, then
    when you've added all the rows, you can iterate over each line as a string.
*/
struct PaddedStringTable
{
    void startRow();
    void appendItem (std::string item);
    size_t getNumRows() const;
    size_t getNumColumns (size_t row) const;
    std::string getRow (size_t rowIndex) const;
    std::string& getCell (size_t row, size_t column);

    template <typename RowHandlerFn>
    void iterateRows (RowHandlerFn&& handleRow)
    {
        for (size_t i = 0; i < rows.size(); ++i)
            handleRow (getRow (i));
    }

    int numExtraSpaces = 1;

private:
    using Row = ArrayWithPreallocation<std::string, 8>;
    ArrayWithPreallocation<Row, 16> rows;
    ArrayWithPreallocation<size_t, 16> columnWidths;
};

//==============================================================================
/** A medium speed & strength string hasher.
    This isn't cryptographically strong, but very unlikely to have collisions in
    most practical circumstances, so useful for situations where a collision
    is unwanted, but wouldn't cause a security problem.
*/
struct HashBuilder
{
    HashBuilder& operator<< (char) noexcept;
    HashBuilder& operator<< (const std::string&) noexcept;
    HashBuilder& operator<< (ArrayView<char>) noexcept;

    std::string toString() const;

private:
    uint32_t data[256] = {};
    uint32_t index = 0;
};


} // namespace soul

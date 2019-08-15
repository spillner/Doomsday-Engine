/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/Info"
#include "de/App"
#include "de/Folder"
#include "de/Log"
#include "de/LogBuffer"
#include "de/RecordValue"
#include "de/ScriptLex"
#include "de/SourceLineTable"
#include <de/TextValue>

#include <fstream>

namespace de {

static const char *WHITESPACE_OR_COMMENT = " \t\r\n#";
static const char *TOKEN_BREAKING_CHARS  = "#:=$(){}<>,;\" \t\r\n";
static const char *INCLUDE_TOKEN         = "@include";
static const char *SCRIPT_TOKEN          = "script";
static const char *GROUP_TOKEN           = "group";

static SourceLineTable sourceLineTable;

DE_PIMPL(Info)
{
    DE_ERROR(OutOfElements);
    DE_ERROR(EndOfFile);

    struct DefaultIncludeFinder : public IIncludeFinder
    {
        String findIncludedInfoSource(String const &includeName, Info const &info,
                                      String *sourcePath) const override
        {
            String path = info.sourcePath().fileNamePath() / includeName;
            if (sourcePath) *sourcePath = path;
            return String::fromUtf8(Block(App::rootFolder().locate<File const>(path)));
        }
    };

    Set<String> scriptBlockTypes;
    StringList allowDuplicateBlocksOfType;
    String implicitBlockType = GROUP_TOKEN;

    String sourcePath; ///< May be unknown (empty).
    String content;
    int currentLine = 0;
    String::const_iterator cursor; ///< Index of the next character from the source.
    Char currentChar;
    String::const_iterator tokenStartOffset;
    String currentToken;
    BlockElement rootBlock;
    DefaultIncludeFinder defaultFinder;
    const IIncludeFinder *finder = &defaultFinder;

    using InfoValue = Info::Element::Value;

    Impl(Public *i)
        : Base(i)
        , rootBlock("", "", *i)
    {
        scriptBlockTypes << SCRIPT_TOKEN;
    }

    /**
     * Initialize the parser for reading a block of source content.
     * @param source  Text to be parsed.
     */
    void init(String const &source)
    {
        rootBlock.clear();

        // The source data. Add an extra newline so the character reader won't
        // get confused.
        content = source + "\n";
        currentLine = 1;

        currentChar = '\0';
        cursor = content.begin();

        // When nextToken() is called and the current token is empty,
        // it is deduced that the source file has ended. We must
        // therefore set a dummy token that will be discarded
        // immediately.
        currentToken = " ";
        tokenStartOffset = cursor;

//        if (source.isEmpty())
//        {
//            content.clear();
//            currentLine = 0;
//        }

        try
        {
            nextChar();
            nextToken();
        }
        catch (EndOfFile const &)
        {
            currentToken.clear();
        }
    }

    /**
     * Returns the next character from the source file.
     */
    Char peekChar()
    {
        return currentChar;
    }

    /**
     * Move to the next character in the source file.
     */
    void nextChar()
    {
        if (cursor == content.end())
        {
            // No more characters to read.
            throw EndOfFile(stringf("EOF on line %i", currentLine));
        }
        if (currentChar == '\n')
        {
            currentLine++;
        }
        currentChar = *cursor++;
    }

    /**
     * Read a line of text from the content and return it.
     */
    String readLine()
    {
        String line;
        nextChar();
        while (currentChar != '\n')
        {
            line += currentChar;
            nextChar();
        }
        return line;
    }

    /**
     * Read until a newline is encountered. Returns the contents of the line.
     */
    String readToEOL()
    {
        cursor = tokenStartOffset;
        String line = readLine();
        try
        {
            nextChar();
        }
        catch (EndOfFile const &)
        {
            // If the file ends right after the line, we'll get the EOF
            // exception.  We can safely ignore it for now.
        }
        return line;
    }

    String peekToken()
    {
        return currentToken;
    }

    /**
     * Returns the next meaningful token from the source file.
     */
    String nextToken()
    {
        // Already drawn a blank?
        if (currentToken.isEmpty())
        {
            throw EndOfFile("out of tokens");
        }

        currentToken = "";

        try
        {
            // Skip over any whitespace.
            while (strchr(WHITESPACE_OR_COMMENT, peekChar()))
            {
                // Comments are considered whitespace.
                if (peekChar() == '#') readLine();
                nextChar();
            }

            // Store the offset where the token begins.
            tokenStartOffset = cursor;

            // The first nonwhite is accepted.
            currentToken += peekChar();
            nextChar();

            // Token breakers are tokens all by themselves.
            if (strchr(TOKEN_BREAKING_CHARS, currentToken.first()))
                return currentToken;

            while (!strchr(TOKEN_BREAKING_CHARS, peekChar()))
            {
                currentToken += peekChar();
                nextChar();
            }
        }
        catch (EndOfFile const &)
        {}

        return currentToken;
    }

    /**
     * This is the method that the user calls to retrieve the next element from
     * the source file. If there are no more elements to return, a
     * OutOfElements exception is thrown.
     *
     * @return Parsed element. Caller gets owernship.
     */
    Element *get()
    {
        Element *e = parseElement();
        if (!e) throw OutOfElements("");
        return e;
    }

    /**
     * Returns the next element from the source file.
     * @return An instance of one of the Element classes, or @c NULL if there are none.
     */
    Element *parseElement()
    {
        String key;
        String next;
        try
        {
            key = peekToken();

            // The next token decides what kind of element we have here.
            next = nextToken();
        }
        catch (EndOfFile const &)
        {
            // The file ended.
            return nullptr;
        }

        int const elementLine = currentLine;
        Element *result = nullptr;

        if (next == ":" || next == "=" || next == "$")
        {
            result = parseKeyElement(key);
        }
        else if (next == "<")
        {
            result = parseListElement(key);
        }
        else
        {
            // It must be a block element.
            result = parseBlockElement(key);
        }

        DE_ASSERT(result);

        result->setSourceLocation(sourcePath, elementLine);
        return result;
    }

    /**
     * Parse a string literal. Returns the string sans the quotation marks in
     * the beginning and the end.
     */
    String parseString()
    {
        if (peekToken() != "\"")
        {
            throw SyntaxError(
                "Info::parseString",
                stringf("Expected string to begin with '\"', but '%s' found instead (on line %i).",
                        peekToken().c_str(),
                        currentLine));
        }

        // The collected characters.
        String chars;

        while (peekChar() != '"')
        {
            if (peekChar() == '\'')
            {
                // Double single quotes form a double quote ('' => ").
                nextChar();
                if (peekChar() == '\'')
                {
                    chars.append("\"");
                }
                else
                {
                    chars.append("'");
                    continue;
                }
            }
            else
            {
                // Other characters are appended as-is, even newlines.
                chars.append(peekChar());
            }
            nextChar();
        }

        // Move the parser to the next token.
        nextChar();
        nextToken();
        return chars;
    }

    /**
     * Parse a value from the source file. The current token should be on the
     * first token of the value. Values come in different flavours:
     * - single token
     * - string literal (can be split)
     *
     * An optional semicolon may follow a value.
     */
    InfoValue parseValue()
    {
        InfoValue value;

        if (peekToken() == "$")
        {
            // Marks a script value.
            value.flags |= InfoValue::Script;
            nextToken();
        }

        // Check if it is the beginning of a string literal.
        // The value will be composed of any number of sub-strings.
        if (peekToken() == "\"")
        {
            value.flags |= InfoValue::StringLiteral;
            while (peekToken() == "\"")
            {
                value.text += parseString();
            }
        }
        else
        {
            // Then it must be a single token.
            if (peekToken() != ";")
            {
                value = peekToken();
                nextToken();
                if (peekToken() == ";") nextToken(); // Ignore the semicolon.
            }
        }
        return value;
    }

    InfoValue parseScript(int requiredStatementCount = 0)
    {
        const auto startPos = cursor.pos() - 1;
        String remainder = content.substr(startPos);
        ScriptLex lex(remainder);

        TokenBuffer tokens;
        int count = 0;

        // Read an appropriate number of statements.
        while (lex.getStatement(tokens, ScriptLex::StopAtMismatchedCloseBrace))
        {
            if (requiredStatementCount > 0 &&
               ++count == requiredStatementCount) break; // We're good now.
        }

        // Continue parsing normally from here.
        auto endPos = startPos + lex.pos().pos();
        do { nextChar(); } while (cursor.pos() < endPos); // fast-forward

        // Update the current token.
        currentToken = String(1, peekChar());
        nextChar();

        if (currentToken != ")" && currentToken != "}")
        {
            // When parsing just a statement, we might stop at something else
            // than a bracket; if so, skip to the next valid token.
            nextToken();
        }

        //qDebug() << "now at" << content.substr(endPos - 15, endPos) << "^" << content.substr(endPos);

        // Whitespace is removed from beginning and end.
        return InfoValue(content.substr(startPos, int(lex.pos().pos().index) - 1).strip(),
                         InfoValue::Script);
    }

    /**
     * Parse a key element.
     * @param name Name of the parsed key element.
     */
    KeyElement *parseKeyElement(String const &name)
    {
        InfoValue value;

        if (peekToken() == "$")
        {
            // This is a script value.
            value.flags |= InfoValue::Script;
            nextToken();
        }

        // A colon means that that the rest of the line is the value of
        // the key element.
        if (peekToken() == ":")
        {
            value.text = readToEOL().strip();
            nextToken();
        }
        else if (peekToken() == "=")
        {
            if (value.flags & InfoValue::Script)
            {
                // Parse one script statement.
                value = parseScript(1);
                value.text = value.text.strip();
            }
            else
            {
                /**
                 * Key =
                 *   "This is a long string "
                 *   "that spans multiple lines."
                 */
                nextToken();
                value.text = parseValue();
            }
        }
        else
        {
            throw SyntaxError("Info::parseKeyElement",
                              stringf("Expected either '=' or ':', but '%s' found instead (on line %i).",
                              peekToken().c_str(), currentLine));
        }
        return new KeyElement(name, value);
    }

    /**
     * Parse a list element, identified by the given name.
     */
    ListElement *parseListElement(String const &name)
    {
        if (peekToken() != "<")
        {
            throw SyntaxError("Info::parseListElement",
                              stringf("List must begin with a '<', but '%s' found instead (on line %i).",
                              peekToken().c_str(), currentLine));
        }

        std::unique_ptr<ListElement> element(new ListElement(name));

        /// List syntax:
        /// list ::= list-identifier '<' [value {',' value}] '>'
        /// list-identifier ::= token

        // Move past the opening angle bracket.
        nextToken();

        if (peekToken() == ">")
        {
            nextToken();
            return element.release();
        }

        for (;;)
        {
            element->add(parseValue());

            // List elements are separated explicitly.
            String separator = peekToken();
            nextToken();

            // The closing bracket?
            if (separator == ">") break;

            // There should be a comma here.
            if (separator != ",")
            {
                throw SyntaxError("Info::parseListElement",
                                  stringf("List values must be separated with a comma, but '%s' "
                                          "found instead (on line %i).",
                                          separator.c_str(),
                                          currentLine));
            }
        }
        return element.release();
    }

    /**
     * Parse a block element, identified by the given name.
     * @param blockType Identifier of the block.
     */
    BlockElement *parseBlockElement(String blockType)
    {
        DE_ASSERT(blockType != "}");
        DE_ASSERT(blockType != ")");

        String blockName;
        String endToken;
        std::unique_ptr<BlockElement> block;
        int startLine = currentLine;

        try
        {
            if (!scriptBlockTypes.contains(blockType)) // script blocks are never named
            {
                if (peekToken() != "(" && peekToken() != "{")
                {
                    blockName = parseValue();
                }
            }

            if (!implicitBlockType.isEmpty() && blockName.isEmpty() &&
                blockType != implicitBlockType &&
                !scriptBlockTypes.contains(blockType))
            {
                blockName = blockType;
                blockType = implicitBlockType;
            }

            block.reset(new BlockElement(blockType, blockName, self()));
            startLine = currentLine;

            // How about some attributes?
            // Syntax: {token value} '('|'{'

            while (peekToken() != "(" && peekToken() != "{")
            {
                String keyName = peekToken();
                nextToken();
                if (peekToken() == "(" || peekToken() == "{")
                {
                    throw SyntaxError(
                        "Info::parseBlockElement",
                        stringf("Attribute on line %i is missing a value", currentLine));
                }
                InfoValue value = parseValue();

                // This becomes a key element inside the block but it's
                // flagged as Attribute.
                block->add(new KeyElement(keyName, value, KeyElement::Attribute));
            }

            endToken = (peekToken() == "("? ")" : "}");

            // Parse the contents of the block.
            if (scriptBlockTypes.contains(blockType))
            {
                // Parse as Doomsday Script.
                block->add(new KeyElement(SCRIPT_TOKEN, parseScript()));
            }
            else
            {
                // Move past the opening parentheses.
                nextToken();

                // Parse normally as Info.
                while (peekToken() != endToken)
                {
                    Element *element = parseElement();
                    if (!element)
                    {
                        throw SyntaxError(
                            "Info::parseBlockElement",
                            stringf("Block element (on line %i) was never closed, end of file "
                                    "encountered before '%s' was found (on line %i).",
                                    startLine,
                                    endToken.c_str(),
                                    currentLine));
                    }
                    block->add(element);
                }
            }

            DE_ASSERT(peekToken() == endToken);

            // Move past the closing parentheses.
            nextToken();
        }
        catch (EndOfFile const &)
        {
            throw SyntaxError("Info::parseBlockElement",
                              stringf("End of file encountered unexpectedly while parsing a block "
                                      "element (block started on line %i).",
                                      startLine));
        }

        return block.release();
    }

    void includeFrom(String const &includeName)
    {
        try
        {
            DE_ASSERT(finder != nullptr);

            String includePath;
            String content = finder->findIncludedInfoSource(includeName, self(), &includePath);

            LOG_MSG("Including: %s") << includePath;

            Info included;
            included.setImplicitBlockType(implicitBlockType);
            included.setScriptBlocks(compose<StringList>(scriptBlockTypes.begin(),
                                                         scriptBlockTypes.end()));
            included.setAllowDuplicateBlocksOfType(allowDuplicateBlocksOfType);
            included.setFinder(*finder); // use ours
            included.setSourcePath(includePath);
            included.parse(content);

            // Move the contents of the resulting root block to our root block.
            included.d->rootBlock.moveContents(rootBlock);
        }
        catch (Error const &er)
        {
            throw IIncludeFinder::NotFoundError("Info::includeFrom",
                    stringf("Cannot include '%s': %s",
                    includeName.c_str(),
                    er.asText().c_str()));
        }
    }

    void parse(String const &source)
    {
        init(source);
        for (;;)
        {
            Element *e = parseElement();
            if (!e) break;

            // If this is an include directive, try to acquire the inclusion and parse it
            // instead. Inclusions are only possible at the root level.
            if (e->isList() && e->name() == INCLUDE_TOKEN)
            {
                for (Element::Value const &val : e->as<ListElement>().values())
                {
                    includeFrom(val);
                }
            }

            rootBlock.add(e);
        }
    }

    void parse(File const &file)
    {
        sourcePath = file.path();
        parse(String::fromUtf8(Block(file)));
    }
};

//---------------------------------------------------------------------------------------

DE_PIMPL_NOREF(Info::Element)
{
    Type type;
    String name;
    BlockElement *parent = nullptr;
    SourceLineTable::LineId sourceLine = 0;
};

Info::Element::Element(Type type, String const &name)
    : d(new Impl)
{
    d->type = type;
    setName(name);
}

void Info::Element::setParent(BlockElement *parent)
{
    d->parent = parent;
}

Info::BlockElement *Info::Element::parent() const
{
    return d->parent;
}

void Info::Element::setSourceLocation(const String &sourcePath, int line)
{
    d->sourceLine = de::sourceLineTable.lineId(sourcePath, line);
}

String Info::Element::sourceLocation() const
{
    return de::sourceLineTable.sourceLocation(d->sourceLine);
}

duint32 Info::Element::sourceLineId() const
{
    return d->sourceLine;
}

Info::Element::Type Info::Element::type() const
{
    return d->type;
}

String const &Info::Element::name() const
{
    return d->name;
}

void Info::Element::setName(String const &name)
{
    d->name = name;
}

Info::BlockElement::~BlockElement()
{
    clear();
}

void Info::BlockElement::clear()
{
    for (auto *i : _contentsInOrder)
    {
        delete i;
    }
    _contents.clear();
    _contentsInOrder.clear();
}

void Info::BlockElement::add(Info::Element *elem)
{
    DE_ASSERT(elem);

    elem->setParent(this);
    _contentsInOrder.append(elem); // owned
    if (!elem->name().isEmpty())
    {
        _contents.insert(elem->name().lower(), elem); // not owned (name may be empty)
    }
}

Info::Element *Info::BlockElement::find(String const &name) const
{
    auto found = _contents.find(name.lower());
    if (found == _contents.end()) return nullptr;
    return found->second;
}

Info::Element::Value Info::BlockElement::keyValue(String const &name,
                                                  String const &defaultValue) const
{
    Element *e = findByPath(name);
    if (!e || !e->isKey()) return Value(defaultValue);
    return e->as<KeyElement>().value();
}

String Info::BlockElement::operator[](const String &name) const
{
    return keyValue(name).text;
}

Info::Element *Info::BlockElement::findByPath(String const &path) const
{
    String name;
    String remainder;
    if (auto pos = path.indexOf(':'))
    {
        name      = path.left(pos);
        remainder = path.substr(pos + 1);
    }
    else
    {
        name = path;
    }
    name = name.strip();

    // Does this element exist?
    Element *e = find(name);
    if (!e) return nullptr;

    if (e->isBlock())
    {
        // Descend into sub-blocks.
        return e->as<BlockElement>().findByPath(remainder);
    }
    return e;
}

void Info::BlockElement::moveContents(BlockElement &destination)
{
    for (Element *e : _contentsInOrder)
    {
        destination.add(e);
    }
    _contentsInOrder.clear();
    _contents.clear();
}

Record Info::BlockElement::asRecord() const
{
    Record rec;
    for (auto i = _contents.begin(); i != _contents.end(); ++i)
    {
        std::unique_ptr<Variable> var(new Variable(i->first));
        const Element *elem = i->second;
        if (elem->isBlock())
        {
            var->set(RecordValue::takeRecord(elem->as<BlockElement>().asRecord()));
        }
        else if (elem->isList())
        {
            std::unique_ptr<ArrayValue> array(new ArrayValue);
            for (const auto &value : elem->values())
            {
                array->add(new TextValue(value.text));
            }
            var->set(array.release());
        }
        else
        {
            var->set(new TextValue(elem->as<KeyElement>().value().text));
        }
        rec.add(var.release());
    }
    return rec;
}

//---------------------------------------------------------------------------------------

Info::Info() : d(new Impl(this))
{}

Info::Info(String const &source) : d(nullptr)
{
    std::unique_ptr<Impl> inst(new Impl(this)); // parsing may throw exception
    inst->parse(source);
    d.reset(inst.release());
}

Info::Info(File const &file) : d(nullptr)
{
    std::unique_ptr<Impl> inst(new Impl(this)); // parsing may throw exception
    inst->parse(file);
    d.reset(inst.release());
}

Info::Info(String const &source, IIncludeFinder const &finder) : d(nullptr)
{
    std::unique_ptr<Impl> inst(new Impl(this)); // parsing may throw exception
    inst->finder = &finder;
    inst->parse(source);
    d.reset(inst.release());
}

void Info::setFinder(IIncludeFinder const &finder)
{
    d->finder = &finder;
}

void Info::useDefaultFinder()
{
    d->finder = &d->defaultFinder;
}

void Info::setScriptBlocks(const StringList &blocksToParseAsScript)
{
    for (const auto &s : blocksToParseAsScript)
    {
        d->scriptBlockTypes << s;
    }
}

void Info::setAllowDuplicateBlocksOfType(const StringList &duplicatesAllowed)
{
    d->allowDuplicateBlocksOfType = duplicatesAllowed;
}

void Info::setImplicitBlockType(String const &implicitBlock)
{
    d->implicitBlockType = implicitBlock;
}

void Info::parse(String const &infoSource)
{
    d->parse(infoSource);
}

void Info::parse(File const &file)
{
    d->parse(file);
}

void Info::parseNativeFile(NativePath const &nativePath)
{
    if (std::ifstream file{nativePath})
    {
        parse(Block::readAll(file));
    }
}

void Info::clear()
{
    d->sourcePath.clear();
    parse("");
}

void Info::setSourcePath(String const &path)
{
    d->sourcePath = path;
}

String Info::sourcePath() const
{
    return d->sourcePath;
}

Info::BlockElement const &Info::root() const
{
    return d->rootBlock;
}

Info::Element const *Info::findByPath(String const &path) const
{
    if (path.isEmpty()) return &d->rootBlock;
    return d->rootBlock.findByPath(path);
}

bool Info::findValueForKey(String const &key, String &value) const
{
    Element const *element = findByPath(key);
    if (element && element->isKey())
    {
        value = element->as<KeyElement>().value();
        return true;
    }
    return false;
}

String Info::operator[](const String &keyPath) const
{
    String value;
    findValueForKey(keyPath, value);
    return value;
}

bool Info::isEmpty() const
{
    return d->rootBlock.isEmpty();
}

String Info::quoteString(String const &text)
{
    String quoted = text;
    quoted.replace("\"", "''");
    return "\"" + quoted + "\"";
}

String Info::sourceLocation(duint32 lineId) // static
{
    return de::sourceLineTable.sourceLocation(lineId);
}

SourceLineTable const &Info::sourceLineTable() // static
{
    return de::sourceLineTable;
}

} // namespace de

/** @file textcanvas.cpp Text-based drawing surface.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/shell/TextCanvas"

namespace de { namespace shell {

DE_PIMPL_NOREF(TextCanvas)
{
    Size               size;
    List<AttribChar *> lines;

    struct RichFormat {
        AttribChar::Attribs attrib;
        Rangei              range;
    };
    List<RichFormat> richFormats;

    Impl(Size const &initialSize) : size(initialSize)
    {
        // Allocate lines based on supplied initial size.
        for (duint row = 0; row < size.y; ++row)
        {
            lines.append(makeLine());
        }
    }

    ~Impl()
    {
        for (duint i = 0; i < lines.size(); ++i)
        {
            delete [] lines[i];
        }
    }

    dsize lineCount() const
    {
        return lines.size();
    }

    AttribChar *makeLine()
    {
        return new AttribChar[size.x];
    }

    void resize(Size const &newSize)
    {
        if (newSize == size) return;

        // Allocate or free lines.
        while (newSize.y < lineCount())
        {
            lines.removeLast();
        }
        while (newSize.y > lineCount())
        {
            lines.append(makeLine());
        }

        DE_ASSERT(lineCount() == newSize.y);
        size.y = newSize.y;

        // Make sure all lines are the correct width.
        for (duint row = 0; row < lines.size(); ++row)
        {
            AttribChar *newLine = new AttribChar[newSize.x];
            std::memcpy(newLine, lines[row], sizeof(AttribChar) * de::min(size.x, newSize.x));
            delete [] lines[row];
            lines[row] = newLine;
        }

        size.x = newSize.x;
    }

    void markAllAsDirty(bool markDirty)
    {
        for (duint row = 0; row < lines.size(); ++row)
        {
            auto *line = lines[row];
            for (duint col = 0; col < size.x; ++col)
            {
                applyFlagOperation(line[col].attribs, AttribChar::Dirty, markDirty);
            }
        }
    }

    AttribChar::Attribs richAttribsForTextIndex(int pos, int offset = 0) const
    {
        AttribChar::Attribs attr;
        for (const RichFormat &rf : richFormats)
        {
            if (rf.range.contains(offset + pos))
            {
                attr |= rf.attrib;
            }
        }
        return attr;
    }
};

TextCanvas::TextCanvas(Size const &size) : d(new Impl(size))
{
    d->size = size;
}

TextCanvas::~TextCanvas()
{}

TextCanvas::Size TextCanvas::size() const
{
    return d->size;
}

int TextCanvas::width() const
{
    return d->size.x;
}

int TextCanvas::height() const
{
    return d->size.y;
}

Rectanglei TextCanvas::rect() const
{
    return Rectanglei(0, 0, size().x, size().y);
}

void TextCanvas::resize(Size const &newSize)
{
    d->resize(newSize);
}

TextCanvas::AttribChar &TextCanvas::at(Coord const &pos)
{
    DE_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

TextCanvas::AttribChar const &TextCanvas::at(Coord const &pos) const
{
    DE_ASSERT(isValid(pos));
    return d->lines[pos.y][pos.x];
}

bool TextCanvas::isValid(Coord const &pos) const
{
    return (pos.x >= 0 && pos.y >= 0 && pos.x < int(d->size.x) && pos.y < int(d->size.y));
}

void TextCanvas::markDirty()
{
    d->markAllAsDirty(true);
}

void TextCanvas::clear(AttribChar const &ch)
{
    fill(Rectanglei(0, 0, d->size.x, d->size.y), ch);
}

void TextCanvas::fill(Rectanglei const &rect, AttribChar const &ch)
{
    for (int y = rect.top(); y < rect.bottom(); ++y)
    {
        for (int x = rect.left(); x < rect.right(); ++x)
        {
            Coord const xy(x, y);
            if (isValid(xy)) at(xy) = ch;
        }
    }
}

void TextCanvas::put(Vec2i const &pos, AttribChar const &ch)
{
    if (isValid(pos))
    {
        at(pos) = ch;
    }
}

void TextCanvas::clearRichFormat()
{
    d->richFormats.clear();
}

void TextCanvas::setRichFormatRange(AttribChar::Attribs const &attribs, Rangei const &range)
{
    Impl::RichFormat rf;
    rf.attrib = attribs;
    rf.range  = range;
    d->richFormats.append(rf);
}

void TextCanvas::drawText(Vec2i const &              pos,
                          String const &             text,
                          AttribChar::Attribs const &attribs,
                          int                        richOffset)
{
    Vec2i p = pos;
    duint i = 0;
    //for (duint i = 0; i < text.size(); ++i)
    for (String::const_iterator iter = text.begin(), end = text.end(); iter != end; ++iter, ++i)
    {
        if (isValid(p))
        {
            at(p) = AttribChar(*iter, attribs | d->richAttribsForTextIndex(i, richOffset));
        }
        p.x++;
    }
}

void TextCanvas::drawWrappedText(Vec2i const &pos, String const &text,
                                 ILineWrapping const &wraps, AttribChar::Attribs const &attribs,
                                 Alignment lineAlignment)
{
    int const width = wraps.width();

    for (int y = 0; y < wraps.height(); ++y)
    {
        WrappedLine const &span = wraps.line(y);
        String part = text.substr(span.range);
        int x = 0;
        if (lineAlignment.testFlag(AlignRight))
        {
            x = width - part.sizei();
        }
        else if (!lineAlignment.testFlag(AlignLeft))
        {
            x = width/2 - part.sizei()/2;
        }
        drawText(pos + Vec2i(x, y), part, attribs, span.range.start.index);
    }
}

void TextCanvas::drawLineRect(Rectanglei const &rect, AttribChar::Attribs const &attribs)
{
    AttribChar const corner('+', attribs);
    AttribChar const hEdge ('-', attribs);
    AttribChar const vEdge ('|', attribs);

    // Horizontal edges.
    for (duint x = 1; x < rect.width() - 1; ++x)
    {
        put(rect.topLeft + Vec2i(x, 0), hEdge);
        put(rect.bottomLeft() + Vec2i(x, -1), hEdge);
    }

    // Vertical edges.
    for (duint y = 1; y < rect.height() - 1; ++y)
    {
        put(rect.topLeft + Vec2i(0, y), vEdge);
        put(rect.topRight() + Vec2i(-1, y), vEdge);
    }

    put(rect.topLeft, corner);
    put(rect.topRight() - Vec2i(1, 0), corner);
    put(rect.bottomRight - Vec2i(1, 1), corner);
    put(rect.bottomLeft() - Vec2i(0, 1), corner);
}

void TextCanvas::draw(TextCanvas const &canvas, Coord const &topLeft)
{
    for (duint y = 0; y < canvas.d->size.y; ++y)
    {
        for (duint x = 0; x < canvas.d->size.x; ++x)
        {
            Coord const xy(x, y);
            Coord const p = topLeft + xy;
            if (isValid(p))
            {
                at(p) = canvas.at(xy);
            }
        }
    }
}

void TextCanvas::show()
{
    d->markAllAsDirty(false);
}

void TextCanvas::setCursorPosition(Vec2i const &) {}

}} // namespace de::shell

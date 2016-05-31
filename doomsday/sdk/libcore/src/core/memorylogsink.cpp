/** @file memorylogsink.h  Log sink that stores log entries in memory.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/MemoryLogSink"

namespace de {

MemoryLogSink::MemoryLogSink(LogEntry::Level minimumLevel)
    : _minLevel(minimumLevel)
    , _privileged(false)
{}

MemoryLogSink::~MemoryLogSink()
{
    DENG2_GUARD(this);

    qDeleteAll(_entries);
}

void MemoryLogSink::setPrivileged(bool onlyPrivileged)
{
    _privileged = onlyPrivileged;
}

void MemoryLogSink::clear()
{
    DENG2_GUARD(this);

    qDeleteAll(_entries);
    _entries.clear();
}

LogSink &MemoryLogSink::operator << (LogEntry const &entry)
{
    if ((!_privileged &&  (entry.context() & LogEntry::Privileged)) ||
        (_privileged && !(entry.context() & LogEntry::Privileged)))
    {
        // Skip (non-)privileged entry.
        return *this;
    }

    if (entry.level() >= _minLevel)
    {
        DENG2_GUARD(this);
        _entries.append(new LogEntry(entry));
        addedNewEntry(*_entries.back());
    }
    return *this;
}

LogSink &MemoryLogSink::operator << (String const &)
{
    // Ignore...
    return *this;
}

int MemoryLogSink::entryCount() const
{
    DENG2_GUARD(this);

    return _entries.size();
}

LogEntry const &MemoryLogSink::entry(int index) const
{
    DENG2_GUARD(this);
    DENG2_ASSERT(index >= 0);
    DENG2_ASSERT(index < _entries.size());

    return *_entries.at(index);
}

void MemoryLogSink::remove(int pos, int n)
{
    DENG2_GUARD(this);

    while (n-- > 0)
    {
        delete _entries.takeAt(pos);
    }
}

void MemoryLogSink::addedNewEntry(LogEntry &)
{}

} // namespace de

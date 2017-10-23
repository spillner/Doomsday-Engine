/** @file async.cpp  Asynchronous utilities.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Async"

namespace de {

AsyncScope::~AsyncScope()
{
    foreach (AsyncTask *task, _tasks)
    {
        task->invalidate();
    }
}

AsyncScope &AsyncScope::operator += (AsyncTask *task)
{
    if (task)
    {
        _tasks.insert(task);
        QObject::connect(task, &QObject::destroyed, [this] (QObject *obj)
        {
            _tasks.remove(static_cast<AsyncTask *>(obj));
        });
    }
    return *this;
}

} // namespace de

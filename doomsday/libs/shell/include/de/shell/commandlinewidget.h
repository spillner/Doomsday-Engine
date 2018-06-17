/** @file commandlinewidget.h  Widget for command line input.
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

#ifndef LIBSHELL_COMMANDLINEWIDGET_H
#define LIBSHELL_COMMANDLINEWIDGET_H

#include "LineEditTextWidget"

namespace de { namespace shell {

/**
 * Text editor with a history.
 *
 * @ingroup textUi
 */
class LIBSHELL_PUBLIC CommandLineTextWidget : public LineEditTextWidget
{
public:
    DE_DEFINE_AUDIENCE2(Command, void commandEntered(de::String command))

public:
    CommandLineTextWidget(String const &name = String());
    bool handleEvent(Event const &event);

protected:
    void autoCompletionBegan(String const &wordBase);

private:
    DE_PRIVATE(d)
};

}} // namespace de::shell

#endif // LIBSHELL_COMMANDLINEWIDGET_H

/** @file processcheckdialog.cpp Dialog for checking running processes on Windows.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "updater/processcheckdialog.h"
#include "ui/clientwindow.h"

//#include <QProcess>
#include <de/CommandLine>
#include <SDL2/SDL_messagebox.h>

using namespace de;

#if defined (WIN32)

static bool isProcessRunning(char const *name)
{
    CommandLine wmic;
    wmic << "wmic.exe" << "PROCESS" << "get" << "Caption";
    if (!wmic.waitForStarted()) return false;
    if (!wmic.waitForFinished()) return false;

    QByteArray result = wmic.readAll();
    for (QString p : QString(result).split("\n", QString::SkipEmptyParts))
    {
        if (!p.trimmed().compare(QLatin1String(name), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

dd_bool Updater_AskToStopProcess(char const *processName, char const *message)
{
    while (isProcessRunning(processName))
    {
        MessageDialog *msg = new MessageDialog;
        msg->setDeleteAfterDismissed(true);
        msg->title().setText("Files In Use");
        msg->message().setText(String(message) + "\n\n" _E(2) +
                               Stringf("There is a running process called " _E(b)"%s." _E(.),
                                    processName));

        msg->buttons()
                << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, QObject::tr("Retry"))
                << new DialogButtonItem(DialogWidget::Reject, QObject::tr("Ignore"));

        // Show a notification dialog.
        if (!msg->exec(ClientWindow::main().root()))
        {
            return !isProcessRunning(processName);
        }
    }
    return true;
}

#endif // WIN32

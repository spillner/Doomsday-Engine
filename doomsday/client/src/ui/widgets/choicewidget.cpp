/** @file choicewidget.cpp  Widget for choosing from a set of alternatives.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "ui/widgets/choicewidget.h"
#include "ui/widgets/popupmenuwidget.h"

using namespace de;
using namespace ui;

DENG2_PIMPL(ChoiceWidget),
DENG2_OBSERVES(Context, Addition),
DENG2_OBSERVES(Context, Removal),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation)
{   
    struct SelectAction : public de::Action
    {
        Instance *d;
        ui::Item const &selItem;

        SelectAction(Instance *inst, ui::Item const &item) : d(inst), selItem(item) {}

        void trigger()
        {
            Action::trigger();
            d->selected = d->items().find(selItem);
            d->updateButtonWithSelection();
        }

        Action *duplicate() const
        {
            DENG2_ASSERT(false); // not needed
            return 0;
        }
    };

    PopupMenuWidget *choices;
    Context::Pos selected; ///< One item is always selected.

    Instance(Public *i) : Base(i), selected(0)
    {
        choices = new PopupMenuWidget;
        choices->setAnchorAndOpeningDirection(self.hitRule(), ui::Right);
        choices->menu().organizer().audienceForWidgetCreation += this;
        self.add(choices);

        updateButtonWithSelection();
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if(widget.is<ButtonWidget>())
        {
            // Make sure the created buttons have an action that updates the
            // selected item.
            widget.as<ButtonWidget>().setAction(new SelectAction(this, item));
        }
    }

    Context const &items() const
    {
        return choices->menu().items();
    }

    bool isValidSelection() const
    {
        return selected < items().size();
    }

    void contextItemAdded(Context::Pos id, ui::Item const &)
    {
        if(selected >= items().size())
        {
            // If the previous selection was invalid, make a valid one now.
            selected = 0;

            updateButtonWithSelection();
        }

        if(id <= selected)
        {
            // New item added before/at the selection.
            selected++;
        }
    }

    void contextItemBeingRemoved(Context::Pos id, ui::Item const &)
    {
        if(id <= selected && selected > 0)
        {
            selected--;
        }

        if(!isValidSelection())
        {
            updateButtonWithSelection();
        }
    }

    void updateButtonWithSelection()
    {
        if(isValidSelection())
        {
            ui::Item const &item = items().at(selected);
            self.setText(item.label());

            ActionItem const *act = dynamic_cast<ActionItem const *>(&item);
            if(act)
            {
                self.setImage(act->image());
            }
        }
        else
        {
            // No valid selection.
            self.setText("");
            self.setImage(Image());
        }

        emit self.selectionChanged(selected);
    }
};

ChoiceWidget::ChoiceWidget(String const &name) : ButtonWidget(name), d(new Instance(this))
{}

PopupMenuWidget &ChoiceWidget::popup()
{
    return *d->choices;
}

void ChoiceWidget::setSelected(Context::Pos pos)
{
    if(d->selected != pos)
    {
        d->selected = pos;
        d->updateButtonWithSelection();
    }
}

Context::Pos ChoiceWidget::selected() const
{
    return d->selected;
}

Item const &ChoiceWidget::selectedItem() const
{
    DENG2_ASSERT(d->isValidSelection());
    return d->items().at(d->selected);
}

ui::Context &ChoiceWidget::items()
{
    return d->choices->menu().items();
}

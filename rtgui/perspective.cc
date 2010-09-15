/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <perspective.h>

using namespace rtengine;
using namespace rtengine::procparams;

PerspCorrection::PerspCorrection () : vAdd(false) {

	horiz = Gtk::manage (new Adjuster (M("TP_PERSPECTIVE_HORIZONTAL"), -100, 100, 1, 0));
	horiz->setAdjusterListener (this);

	vert = Gtk::manage (new Adjuster (M("TP_PERSPECTIVE_VERTICAL"), -100, 100, 1, 0));
	vert->setAdjusterListener (this);

    pack_start (*horiz);
    pack_start (*vert);

    show_all();
}

void PerspCorrection::read (const ProcParams* pp, const ParamsEdited* pedited) {

    disableListener ();

    if (pedited) {
    	horiz->setEditedState (pedited->perspective.horizontal ? Edited : UnEdited);
    	vert->setEditedState (pedited->perspective.vertical ? Edited : UnEdited);
    }

    horiz->setValue (pp->perspective.horizontal);
    vert->setValue (pp->perspective.vertical);

    enableListener ();
}

void PerspCorrection::write (ProcParams* pp, ParamsEdited* pedited) {

    pp->perspective.horizontal  = horiz->getValue ();
    pp->perspective.vertical = vert->getValue ();

    if (pedited) {
        pedited->perspective.horizontal = horiz->getEditedState ();
        pedited->perspective.vertical = vert->getEditedState ();
    }
}

void PerspCorrection::setDefaults (const ProcParams* defParams, const ParamsEdited* pedited) {

	horiz->setDefault (defParams->perspective.horizontal);
	vert->setDefault (defParams->perspective.vertical);

    if (pedited) {
    	horiz->setDefaultEditedState (pedited->perspective.horizontal ? Edited : UnEdited);
    	vert->setDefaultEditedState (pedited->perspective.vertical ? Edited : UnEdited);
    }
    else {
    	horiz->setDefaultEditedState (Irrelevant);
    	vert->setDefaultEditedState (Irrelevant);
    }
}

void PerspCorrection::adjusterChanged (Adjuster* a, double newval) {

    if (listener)
        listener->panelChanged (EvPerspCorr, Glib::ustring::compose ("%1=%3\n%2=%4", M("TP_PERSPECTIVE_HORIZONTAL"), M("TP_PERSPECTIVE_VERTICAL"), horiz->getValue(), vert->getValue()));
}

void PerspCorrection::setAdjusterBehavior (bool badd) {

    if ((!vAdd && badd) || (vAdd && !badd)) {
//    	horiz->setLimits (-0.005, 0.005, 0.0001, 0);
//    	vert->setLimits (-0.005, 0.005, 0.0001, 0);
    }

    vAdd = badd;
}

void PerspCorrection::setBatchMode (bool batchMode) {

    ToolPanel::setBatchMode (batchMode);
    horiz->showEditedCB ();
    vert->showEditedCB ();
}
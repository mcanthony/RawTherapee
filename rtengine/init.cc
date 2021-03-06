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
#include "rtengine.h"
#include "iccstore.h"
#include "dcp.h"
#include "camconst.h"
#include "curves.h"
#include "rawimagesource.h"
#include "improcfun.h"
#include "improccoordinator.h"
#include "dfmanager.h"
#include "ffmanager.h"
#include "rtthumbnail.h"
#include "../rtgui/profilestore.h"
#include "../rtgui/threadutils.h"

namespace rtengine
{

const Settings* settings;

MyMutex* lcmsMutex = NULL;

int init (const Settings* s, Glib::ustring baseDir, Glib::ustring userSettingsDir)
{

    settings = s;
    iccStore->init (s->iccDirectory, baseDir + "/iccprofiles");
    iccStore->findDefaultMonitorProfile();

    dcpStore->init (baseDir + "/dcpprofiles");

    CameraConstantsStore::initCameraConstants (baseDir, userSettingsDir);
    profileStore.init ();
    ProcParams::init ();
    Color::init ();
    PerceptualToneCurve::init ();
    RawImageSource::init ();
    ImProcFunctions::initCache ();
    Thumbnail::initGamma ();
    delete lcmsMutex;
    lcmsMutex = new MyMutex;
    dfm.init( s->darkFramesPath );
    ffm.init( s->flatFieldsPath );
    return 0;
}

void cleanup ()
{

    ProcParams::cleanup ();
    Color::cleanup ();
    ImProcFunctions::cleanupCache ();
    Thumbnail::cleanupGamma ();
    RawImageSource::cleanup ();
}

StagedImageProcessor* StagedImageProcessor::create (InitialImage* initialImage)
{

    ImProcCoordinator* ipc = new ImProcCoordinator ();
    ipc->assign (initialImage->getImageSource ());
    return ipc;
}

void StagedImageProcessor::destroy (StagedImageProcessor* sip)
{

    delete sip;
}

Settings* Settings::create  ()
{

    return new Settings;
}

void Settings::destroy (Settings* s)
{

    delete s;
}


}


/*
 * Comet and asteroids importer plug-in for Stellarium
 *
 * Copyright (C) 2010 Bogdan Marinov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _SOLAR_SYSTEM_MANAGER_WINDOW_
#define _SOLAR_SYSTEM_MANAGER_WINDOW_

#include <QObject>
#include "StelDialog.hpp"

#include <QHash>
#include <QString>

class CAImporter;

class Ui_solarSystemManagerWindow;
class ImportWindow;
class ManualImportWindow;

/*! \brief Main window for handling Solar System objects.
  \author Bogdan Marinov
*/
class SolarSystemManagerWindow : public StelDialog
{
	Q_OBJECT
public:
	SolarSystemManagerWindow();
	virtual ~SolarSystemManagerWindow();
	void languageChanged();

protected:
	virtual void createDialogContent();
	Ui_solarSystemManagerWindow * ui;

private slots:
	//! \todo Find a way to suggest a default file name (select directory instead of file?)
	void copyConfiguration();
	void replaceConfiguration();

	void populateSolarSystemList();
	void removeObject();

	void newImportMPC();

	void newImportManual();
	void resetImportManual(bool);

private:
	ImportWindow* importWindow;
	ManualImportWindow * manualImportWindow;

	CAImporter * ssoManager;

	QHash<QString,QString> unlocalizedNames;
};

#endif //_SOLAR_SYSTEM_MANAGER_WINDOW_

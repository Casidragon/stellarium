/*
 * Stellarium TelescopeControl Plug-in
 * 
 * Copyright (C) 2009-2010 Bogdan Marinov (this file)
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
 
#ifndef _TELESCOPECONFIGURATIONDIALOG_HPP_
#define _TELESCOPECONFIGURATIONDIALOG_HPP_

#include <QObject>
#include <QHash>
#include <QIntValidator>
#include <QStringList>
//#include "StelDialog.hpp"
#include "StelDialogTelescopeControl.hpp"
#include "TelescopeControlGlobals.hpp"

using namespace TelescopeControlGlobals;

class Ui_telescopeConfigurationDialog;
class TelescopeControl;
struct StelStyle;

class TelescopeConfigurationDialog : public StelDialogTelescopeControl
{
	Q_OBJECT
public:
	TelescopeConfigurationDialog();
	virtual ~TelescopeConfigurationDialog();
	void languageChanged();
	void updateStyle();
	
	void initExistingTelescopeConfiguration(int slot);
	void initNewTelescopeConfiguration(int slot);
	
protected:
	//! Initialize the dialog widgets and connect the signals/slots
	virtual void createDialogContent();
	Ui_telescopeConfigurationDialog* ui;
	
private:
	void initConfigurationDialog();
	
private slots:
	void buttonSavePressed();
	void buttonDiscardPressed();
	
	void toggleTypeLocal(bool);
	void toggleTypeConnection(bool);
	void toggleTypeVirtual(bool);
	
	void deviceModelSelected(const QString&);

signals:
	void changesSaved(QString name, ConnectionType type);
	void changesDiscarded();
	
private:
	QStringList deviceModelNames;
	
	QRegExpValidator * telescopeNameValidator;
	QRegExpValidator * hostNameValidator;
	QRegExpValidator * circleListValidator;
	QRegExpValidator * serialPortValidator;
	
	int configuredSlot;
	
	TelescopeControl * telescopeManager;
};

#endif // _TELESCOPECONFIGURATIONDIALOG_

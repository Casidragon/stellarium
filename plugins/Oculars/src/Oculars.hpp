/*
 * Copyright (C) 2009 Timothy Reaves
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

#ifndef _OCULARS_HPP_
#define _OCULARS_HPP_

#include "VecMath.hpp"
#include "StelModule.hpp"
#include "OcularDialog.hpp"
#include "CCD.hpp"
#include "Ocular.hpp"
#include "Telescope.hpp"

#include <QFont>
#include <QSettings>

#define MIN_OCULARS_INI_VERSION 0.12

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QMouseEvent;
class QPixmap;
class QSettings;
QT_END_NAMESPACE

class StelButton;

//! This is an example of a plug-in which can be dynamically loaded into stellarium
class Oculars : public StelModule
{
	Q_OBJECT

public:
	Oculars();
	virtual ~Oculars();
	static QSettings* appSettings();

	///////////////////////////////////////////////////////////////////////////
	// Methods defined in the StelModule class
	virtual void init();
	virtual void deinit();
	virtual bool configureGui(bool show=true);
	virtual void draw(StelCore* core);
	virtual double getCallOrder(StelModuleActionName actionName) const;
	//! Returns the module-specific style sheet.
	//! The main StelStyle instance should be passed.
	virtual const StelStyle getModuleStyleSheet(const StelStyle& style);
	//	virtual void handleKeys(class QKeyEvent* event);
	virtual void handleMouseClicks(class QMouseEvent* event);
	virtual void setStelStyle(const QString& style);
	virtual void update(double) {;}

public slots:
	//! This method is called with we detect that our hot key is pressed.  It handles
	//! determining if we should do anything - based on a selected object - and painting
	//! labes to the screen.
	void enableOcular(bool b);

	void toggleCrosshair();
	void toggleTelrad();

	void ccdRotationMajorIncrease();
	void ccdRotationMajorDecrease();
	void ccdRotationMinorIncrease();
	void ccdRotationMinorDecrease();
	void ccdRotationReset();
	void decrementCCDIndex();
	void decrementOcularIndex();
	void decrementTelescopeIndex();
	void incrementCCDIndex();
	void incrementOcularIndex();
	void incrementTelescopeIndex();

	void displayPopupMenu();

signals:
	void selectedCCDChanged();
	void selectedOcularChanged();
	void selectedTelescopeChanged();

private slots:
	//! Signifies a change in ocular or telescope.  Sets new zoom level.
	void instrumentChanged();
	void determineMaxEyepieceAngle();
	void setScaleImageCircle(bool state);

private:
	//! Set up the Qt actions needed to activate the plugin.
	void initializeActivationActions();
	
	//! Set up the Qt actions used while the plugin is active.
	void initializeActions();

	//! This method is needed because the MovementMgr classes handleKeys() method consumes the event.
	//! Because we want the ocular view to track, we must intercept & process ourselves.  Only called
	//! while flagShowOculars == true.
	void interceptMovementKey(class QKeyEvent* event);

	//! Renders crosshairs into the viewport.
	void paintCrosshairs();
	void paintCCDBounds();
	//! Paint the mask into the viewport.
	void paintOcularMask();
	//! Renders the three Telrad circles, but only if not in ocular mode.
	void paintTelrad();
	void inscribeCCDBoundsInOcularMask();


	//! Paints the text about the current object selections to the upper right hand of the screen.
	//! Should only be called from a 'ready' state; currently from the draw() method.
	void paintText(const StelCore* core);

	//! This method is called by the zoom() method, when this plugin is toggled off; it resets to the default view.
	void unzoomOcular();

	//! This method is responsible for insuring a valid ini file for the plugin exists.  It first checks to see
	//! if one exists in the expected location.  If it does not, a default one is copied into place, and the process
	//! ends.  However, if one does exist, it opens it, and looks for the oculars_version key.  The value (or even
	//! presence) is used to determine if the ini file is usable.  If not, it is renamed, and a new one copied over.
	//! It does not ty to cope values over.
	//! Once there is a valid ini file, it is loaded into the settings attribute.
	void validateAndLoadIniFile();

	//! Recordd the state of the GridLinesMgr views beforehand, so that it can be reset afterwords.
	//! @param rezoom if true, this zoom operation is starting from an already zoomed state.
	//!		False for the original state.
	void zoom(bool rezoom);

	//! This method is called by the zoom() method, when this plugin is toggled on; it resets the zoomed view.
	void zoomOcular();

	//! A list of all the oculars defined in the ini file.  Must have at least one, or module will not run.
	QList<CCD *> ccds;
	QList<Ocular *> oculars;
	QList<Telescope *> telescopes;
	int selectedCCDIndex;
	int selectedOcularIndex;
	int selectedTelescopeIndex;

	QFont font;					//!< The font used for drawing labels.
	bool flagShowOculars;		//!< flag used to track if we are in ocular mode.
	bool flagShowCrosshairs;	//!< flag used to track in crosshairs should be rendered in the ocular view.
	bool flagShowTelrad;		//!< If true, display the Telrad overlay.
	int usageMessageLabelID;	//!< the id of the label showing the usage message. -1 means it's not displayed.
	int noEntitiesLabelID;	//!< the id of the label showing that there are no telescopes or oclars. -1 means it's not displayed.

	bool flagAzimuthalGrid;		//!< Flag to track if AzimuthalGrid was displayed at activation.
	bool flagEquatorGrid;		//!< Flag to track if EquatorGrid was displayed at activation.
	bool flagEquatorJ2000Grid;	//!< Flag to track if EquatorJ2000Grid was displayed at activation.
	bool flagEquatorLine;		//!< Flag to track if EquatorLine was displayed at activation.
	bool flagEclipticLine;		//!< Flag to track if EclipticLine was displayed at activation.
	bool flagMeridianLine;		//!< Flag to track if MeridianLine was displayed at activation.

	double ccdRotationAngle;	//<! The angle to rotate the CCD bounding box. */
	double maxEyepieceAngle;	//!< The maximum aFOV of any eyepiece.
	bool useMaxEyepieceAngle;	//!< Read from the ini file, whether to scale the mask based aFOV.


	// for toolbar button
	QPixmap* pxmapGlow;
	QPixmap* pxmapOnIcon;
	QPixmap* pxmapOffIcon;
	StelButton* toolbarButton;

	OcularDialog *ocularDialog;
	bool ready; //!< A flag that determines that this module is usable.  If false, we won't open.
	bool newInstrument; //!< true the first time draw is called for a new ocular or telescope, false otherwise.

	//Styles
	QByteArray normalStyleSheet;
	QByteArray nightStyleSheet;
};


#include "fixx11h.h"
#include <QObject>
#include "StelPluginInterface.hpp"

//! This class is used by Qt to manage a plug-in interface
class OcularsStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_INTERFACES(StelPluginInterface)
public:
	virtual StelModule* getStelModule() const;
	virtual StelPluginInfo getPluginInfo() const;
};

#endif /*_OCULARS_HPP_*/


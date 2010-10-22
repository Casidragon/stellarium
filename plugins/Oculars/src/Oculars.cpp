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

#include "Oculars.hpp"

#include "GridLinesMgr.hpp"
#include "LabelMgr.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelMainWindow.hpp"
#include "StelTranslator.hpp"

#include <QAction>
#include <QKeyEvent>
#include <QDebug>
#include <QMouseEvent>
#include <QtNetwork>
#include <QPixmap>

#include <cmath>

#if defined(__APPLE__) && defined(__MACH__)
#include <OpenGL/glu.h>	/* Header File For The GLU Library */
#else
#include <GL/glu.h>	/* Header File For The GLU Library */
#endif

static QSettings *settings; //!< The settings as read in from the ini file.

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark StelModuleMgr Methods
#endif
/* ********************************************************************* */
//! This method is the one called automatically by the StelModuleMgr just
//! after loading the dynamic library
StelModule* OcularsStelPluginInterface::getStelModule() const
{
	return new Oculars();
}

StelPluginInfo OcularsStelPluginInterface::getPluginInfo() const
{
	// Allow to load the resources when used as a static plugin
	Q_INIT_RESOURCE(Oculars);

	StelPluginInfo info;
	info.id = "Oculars";
	info.displayedName = "Ocular";
	info.authors = "Timothy Reaves";
	info.contact = "treaves@silverfieldstech.com";
	info.description = "Shows the sky as if looking through a telescope eyepiece";
	return info;
}

Q_EXPORT_PLUGIN2(Oculars, OcularsStelPluginInterface)


/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Instance Methods
#endif
/* ********************************************************************* */
Oculars::Oculars() : selectedOcularIndex(-1), flagShowOculars(false), usageMessageLabelID(-1),
				   pxmapGlow(NULL), pxmapOnIcon(NULL), pxmapOffIcon(NULL), toolbarButton(NULL)
{
	flagShowOculars = false;
	flagShowCrosshairs = false;
	flagShowTelrad = false;
	ready = false;
	useMaxEyepieceAngle = true;
	visible = false;

	font.setPixelSize(14);
	maxEyepieceAngle = 0.0;

	ccds = QList<CCD *>();
	oculars = QList<Ocular *>();
	telescopes = QList<Telescope *>();

	selectedCCDIndex = -1;
	selectedOcularIndex = 0;
	selectedTelescopeIndex = 0;

	setObjectName("Oculars");

}

Oculars::~Oculars()
{
	delete ocularDialog;
	ocularDialog = NULL;
}

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark StelModule Methods
#endif
/* ********************************************************************* */
bool Oculars::configureGui(bool show)
{
	if (show)
	{
		StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
		Q_ASSERT(gui);
		gui->getGuiActions("actionShow_Ocular_Window")->setChecked(true);
	}

	return ready;
}

void Oculars::deinit()
{
}

//! Draw any parts on the screen which are for our module
void Oculars::draw(StelCore* core)
{
	if (flagShowTelrad) {
		drawTelrad();
	}
	if (flagShowOculars){
		// Insure there is a selected ocular & telescope
		if (selectedCCDIndex > ccds.count()) {
			qWarning() << "Oculars: the selected sensor index of " << selectedCCDIndex << " is greater than the sensor count of "
			<< ccds.count() << ". Module disabled!";
			ready = false;
		}
		if (selectedOcularIndex > oculars.count()) {
			qWarning() << "Oculars: the selected ocular index of " << selectedOcularIndex << " is greater than the ocular count of "
			<< oculars.count() << ". Module disabled!";
			ready = false;
		}
		else if (selectedTelescopeIndex > telescopes.count()) {
			qWarning() << "Oculars: the selected telescope index of " << selectedTelescopeIndex << " is greater than the telescope count of "
			<< telescopes.count() << ". Module disabled!";
			ready = false;
		}
		
		if (ready) {
			paintMask();
			if (flagShowCrosshairs)  {
				drawCrosshairs();
			}
			// Paint the information in the upper-right hand corner
			paintText(core);
		}
		newInstrument = false; // Now that it's been drawn once
	}
	
}

//! Determine which "layer" the plagin's drawing will happen on.
double Oculars::getCallOrder(StelModuleActionName actionName) const
{
	// TODO; this really doesn't seem to have any effect.  I've tried everything from -100 to +100,
	//		and a calculated value.  It all seems to work the same regardless.
	double order = 1000.0;
	if (actionName==StelModule::ActionHandleKeys || actionName==StelModule::ActionHandleMouseMoves)
	{
		order = StelApp::getInstance().getModuleMgr().getModule("StelMovementMgr")->getCallOrder(actionName) - 1.0;
	}
	else if (actionName==StelModule::ActionDraw)
	{
		order = GETSTELMODULE(LabelMgr)->getCallOrder(actionName) + 100.0;
	}

	return order;
}

const StelStyle Oculars::getModuleStyleSheet(const StelStyle& style)
{
	StelStyle pluginStyle(style);
	if (style.confSectionName == "color") {
		pluginStyle.qtStyleSheet.append(normalStyleSheet);
	} else{
		pluginStyle.qtStyleSheet.append(nightStyleSheet);
	}
	return pluginStyle;
}

void Oculars::handleMouseClicks(class QMouseEvent* event)
{
	StelCore *core = StelApp::getInstance().getCore();
	StelMovementMgr *movementManager = core->getMovementMgr();
	if (StelApp::getInstance().getStelObjectMgr().getWasSelected()){
		LabelMgr *labelManager = GETSTELMODULE(LabelMgr);

		if (flagShowOculars)
		{
			// center the selected object in the ocular, and track.
			movementManager->setFlagTracking(true);
		}
		else
		{
			// remove the usage label if it is being displayed.
			if (usageMessageLabelID > -1)
			{
				labelManager->setLabelShow(usageMessageLabelID, false);
				labelManager->deleteLabel(usageMessageLabelID);
				usageMessageLabelID = -1;
			}
		}
	}
	else if(flagShowOculars)
	{
		// The ocular is displayed, but no object is selected.  So don't track the stars.
		movementManager->setFlagLockEquPos(false);
	}
	event->setAccepted(false);
}

void Oculars::init()
{
	qDebug() << "Ocular plugin - press Command-O to toggle eyepiece view mode. Press ALT-o for configuration.";

	// Load settings from ocular.ini
	try {
		validateAndLoadIniFile();
		// assume all is well

		useMaxEyepieceAngle = settings->value("use_max_exit_circle", 0.0).toBool();
		int ocularCount = settings->value("ocular_count", 0).toInt();
		for (int index = 0; index < ocularCount; index++) {
			Ocular *newOcular = Ocular::ocularFromSettings(settings, index);
			if (newOcular != NULL) {
				oculars.append(newOcular);
			}
		}
		int ccdCount = settings->value("ccd_count", 0).toInt();
		for (int index = 0; index < ccdCount; index++) {
			CCD *newCCD = CCD::ccdFromSettings(settings, index);
			if (newCCD != NULL) {
				ccds.append(newCCD);
			}
		}
		int telescopeCount = settings->value("telescope_count", 0).toInt();
		for (int index = 0; index < telescopeCount; index++) {
			Telescope *newTelescope = Telescope::telescopeFromSettings(settings, index);
			if (newTelescope != NULL) {
				telescopes.append(newTelescope);
			}
		}

		ready = true;
		ocularDialog = new OcularDialog(&ccds, &oculars, &telescopes);
		initializeActivationActions();
		determineMaxEyepieceAngle();
	} catch (std::runtime_error& e) {
		qWarning() << "WARNING: unable to locate ocular.ini file or create a default one for Ocular plugin: " << e.what();
	}

	//Load the module's custom style sheets
	QFile styleSheetFile;
	styleSheetFile.setFileName(":/ocular/normalStyle.css");
	if(styleSheetFile.open(QFile::ReadOnly|QFile::Text))
	{
		normalStyleSheet = styleSheetFile.readAll();
	}
	styleSheetFile.close();
	styleSheetFile.setFileName(":/ocular/nightStyle.css");
	if(styleSheetFile.open(QFile::ReadOnly|QFile::Text))
	{
		nightStyleSheet = styleSheetFile.readAll();
	}
	styleSheetFile.close();
}

void Oculars::setStelStyle(const QString&)
{
	qDebug() << "====> Oculars here.";
	if(ocularDialog)
		ocularDialog->updateStyle();
}

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Private slots Methods
#endif
/* ********************************************************************* */
void Oculars::determineMaxEyepieceAngle()
{
	if (ready) {
		QListIterator<Ocular *> ocularIterator(oculars);
		while (ocularIterator.hasNext()) {
			Ocular *ocular = ocularIterator.next();

			if (ocular->appearentFOV() > maxEyepieceAngle) {
				maxEyepieceAngle = ocular->appearentFOV();
			}
		}
	}
	// insure it is not zero
	if (maxEyepieceAngle == 0.0) {
		maxEyepieceAngle = 1.0;
	}
}

void Oculars::instrumentChanged()
{
	newInstrument = true;
	zoom(true);
}

void Oculars::setScaleImageCircle(bool state)
{
	if (state) {
		determineMaxEyepieceAngle();
	}
	useMaxEyepieceAngle = state;
}

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Slots Methods
#endif
/* ********************************************************************* */
void Oculars::enableOcular(bool b)
{
	if (!ready) {
		qDebug() << "The Oculars module has been disabled.";
		return;
	}

	if (b) {
		// load data and determine if we're ready (if we have all required data)
	}
	if (!ready) {
		// no, some data was missing. We already warned, done.
		return;
	}

	StelCore *core = StelApp::getInstance().getCore();
	LabelMgr* labelManager = GETSTELMODULE(LabelMgr);
	// Toggle the plugin on & off.  To toggle on, we want to ensure there is a selected object.
	if (!flagShowOculars && !StelApp::getInstance().getStelObjectMgr().getWasSelected()) {
		if (usageMessageLabelID == -1) {
			QFontMetrics metrics(font);
			QString labelText = "Please select an object before enabling Ocular.";
			StelProjector::StelProjectorParams projectorParams = core->getCurrentStelProjectorParams();
			int xPosition = projectorParams.viewportCenter[0];
			xPosition = xPosition - 0.5 * (metrics.width(labelText));
			int yPosition = projectorParams.viewportCenter[1];
			yPosition = yPosition - 0.5 * (metrics.height());
			usageMessageLabelID = labelManager->labelScreen(labelText, xPosition, yPosition, true, font.pixelSize(), "#99FF99");
		}
		// we didn't accept the new status - make sure the toolbar button reflects this
		StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
		Q_ASSERT(gui);
		gui->getGuiActions("actionShow_Ocular")->setChecked(false);
	} else {
		if (selectedOcularIndex != -1) {
			// remove the usage label if it is being displayed.
			if (usageMessageLabelID > -1) {
				labelManager->setLabelShow(usageMessageLabelID, false);
				labelManager->deleteLabel(usageMessageLabelID);
				usageMessageLabelID = -1;
			}
			flagShowOculars = b;
			zoom(false);
		}
	}
	if (flagShowOculars) {
		initializeActions();
	}
}

void Oculars::decrementCCDIndex()
{
	selectedCCDIndex--;
	if (selectedCCDIndex == -2) {
		selectedCCDIndex = ccds.count() - 1;
	}
	emit(selectedCCDChanged());
}

void Oculars::decrementOcularIndex()
{
	selectedOcularIndex--;
	if (selectedOcularIndex == -1) {
		selectedOcularIndex = oculars.count() - 1;
	}
	emit(selectedOcularChanged());
}

void Oculars::decrementTelescopeIndex()
{
	selectedTelescopeIndex--;
	if (selectedTelescopeIndex == -1) {
		selectedTelescopeIndex = telescopes.count() - 1;
	}
	emit(selectedTelescopeChanged());
}

void Oculars::incrementCCDIndex()
{
	selectedCCDIndex++;
	if (selectedCCDIndex == ccds.count()) {
		selectedCCDIndex = -1;
	}
	emit(selectedCCDChanged());
}

void Oculars::incrementOcularIndex()
{
	selectedOcularIndex++;
	if (selectedOcularIndex == oculars.count()) {
		selectedOcularIndex = 0;
	}
	emit(selectedOcularChanged());
}

void Oculars::incrementTelescopeIndex()
{
	selectedTelescopeIndex++;
	if (selectedTelescopeIndex == telescopes.count()) {
		selectedTelescopeIndex = 0;
	}
	emit(selectedTelescopeChanged());
}

void Oculars::toggleCrosshair()
{
	flagShowCrosshairs = !flagShowCrosshairs;
}

void Oculars::toggleTelrad()
{
	flagShowTelrad = !flagShowTelrad;
}

/* ********************************************************************* */
#if 0
#pragma mark -
#pragma mark Private Methods
#endif
/* ********************************************************************* */
void Oculars::drawCrosshairs()
{
	const StelProjectorP projector = StelApp::getInstance().getCore()->getProjection(StelCore::FrameEquinoxEqu);
	StelCore *core = StelApp::getInstance().getCore();
	StelProjector::StelProjectorParams params = core->getCurrentStelProjectorParams();
	// Center of screen
	Vec2i centerScreen(projector->getViewportPosX()+projector->getViewportWidth()/2,
					   projector->getViewportPosY()+projector->getViewportHeight()/2);
	GLdouble length = 0.5 * params.viewportFovDiameter;
	// See if we need to scale the length
	if (useMaxEyepieceAngle && oculars[selectedOcularIndex]->appearentFOV() > 0.0) {
		length = oculars[selectedOcularIndex]->appearentFOV() * length / maxEyepieceAngle;
	}

	// Draw the lines
	StelPainter painter(projector);
	painter.setColor(0.77, 0.14, 0.16, 1);
	painter.drawLine2d(centerScreen[0], centerScreen[1], centerScreen[0], centerScreen[1] + length);
	painter.drawLine2d(centerScreen[0], centerScreen[1], centerScreen[0], centerScreen[1] - length);
	painter.drawLine2d(centerScreen[0], centerScreen[1], centerScreen[0] + length, centerScreen[1]);
	painter.drawLine2d(centerScreen[0], centerScreen[1], centerScreen[0] - length, centerScreen[1]);
}

void Oculars::drawTelrad()
{
	if (!flagShowOculars) {
		const StelProjectorP projector = StelApp::getInstance().getCore()->getProjection(StelCore::FrameEquinoxEqu);


		StelCore *core = StelApp::getInstance().getCore();
		StelProjector::StelProjectorParams params = core->getCurrentStelProjectorParams();

		// StelPainter drawing
		StelPainter painter(projector);
		painter.setColor(0.77, 0.14, 0.16, 1.0);
		Vec2i centerScreen(projector->getViewportPosX()+projector->getViewportWidth()/2,
						   projector->getViewportPosY()+projector->getViewportHeight()/2);
		painter.drawCircle(centerScreen[0], centerScreen[1], 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (0.5));
		painter.drawCircle(centerScreen[0], centerScreen[1], 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (2.0));
		painter.drawCircle(centerScreen[0], centerScreen[1], 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (4.0));

//		// Direct drawing
//		glDisable(GL_BLEND);
//		glColor3f(0.f,0.f,0.f);
//		glPushMatrix();
//		glTranslated(params.viewportCenter[0], params.viewportCenter[1], 0.0);
//		GLUquadricObj *quadric = gluNewQuadric();		
//		// the gray circle
//		glColor4f(0.77, 0.14, 0.16, 0.5);
//		float radius = 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (0.5);
//		gluDisk(quadric, radius - 1.0, radius, 256, 1);
//		radius = 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (2.0);
//		gluDisk(quadric, radius - 1.0, radius, 256, 1);
//		radius = 0.5 * projector->getPixelPerRadAtCenter() * (M_PI/180) * (4.0);
//		gluDisk(quadric, radius - 1.0, radius, 256, 1);
//		gluDeleteQuadric(quadric);
//		glPopMatrix();		
	}
}

void Oculars::initializeActivationActions()
{
	QString group = "Oculars Plugin";
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	Q_ASSERT(gui);

	gui->addGuiActions("actionShow_Ocular",
							 N_("Enable ocular"),
							 settings->value("toggle_oculars", "Ctrl+O").toString(),
							 "Plugin Key Bindings",
							 true);
	gui->getGuiActions("actionShow_Ocular")->setChecked(flagShowOculars);
	connect(gui->getGuiActions("actionShow_Ocular"), SIGNAL(toggled(bool)), this, SLOT(enableOcular(bool)));
	gui->addGuiActions("actionShow_Ocular_Window",
							 N_("Configuration Window"),
							 settings->value("toggle_config_dialog", "ALT+O").toString(),
							 group,
							 true);
	connect(gui->getGuiActions("actionShow_Ocular_Window"), SIGNAL(toggled(bool)), ocularDialog, SLOT(setVisible(bool)));
	connect(ocularDialog, SIGNAL(visibleChanged(bool)), gui->getGuiActions("actionShow_Ocular_Window"), SLOT(setChecked(bool)));
	gui->addGuiActions("actionShow_Ocular_Telrad",
							 N_("Display Telrad"),
							 settings->value("toggle_telrad", "Ctrl+B").toString(),
							 group,
							 true);
	gui->getGuiActions("actionShow_Ocular_Telrad")->setChecked(flagShowTelrad);
	connect(gui->getGuiActions("actionShow_Ocular_Telrad"), SIGNAL(toggled(bool)), this, SLOT(toggleTelrad()));

	// Make a toolbar button
	try {
		pxmapGlow = new QPixmap(":/graphicGui/glow32x32.png");
		pxmapOnIcon = new QPixmap(":/ocular/bt_ocular_on.png");
		pxmapOffIcon = new QPixmap(":/ocular/bt_ocular_off.png");

		toolbarButton = new StelButton(NULL,
									   *pxmapOnIcon,
									   *pxmapOffIcon,
									   *pxmapGlow,
									   gui->getGuiActions("actionShow_Ocular"));
		gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup");
	} catch (std::runtime_error& e) {
		qWarning() << "WARNING: unable create toolbar button for Oculars plugin: " << e.what();
	}
}

void Oculars::initializeActions()
{
	static bool actions_initialized;
	if (actions_initialized)
		return;
	actions_initialized = true;
	QString group = "Oculars Plugin";
	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());

	Q_ASSERT(gui);
	gui->addGuiActions("actionShow_Ocular_Crosshair",
							 N_("Toggle Crosshair"),
							 settings->value("toggle_crosshair", "ALT+C").toString(),
							 group, true);

	gui->addGuiActions("actionShow_CCD_increment",
							 N_("Select next sensor"),
							 settings->value("next_ccd", "Shift+Ctrl+]").toString(),
							 group, false);
	gui->addGuiActions("actionShow_CCD_decrement",
							 N_("Select previous sensor"),
							 settings->value("prev_ccd", "Shift+Ctrl+[").toString(),
							 group, false);
	gui->addGuiActions("actionShow_Ocular_increment",
							 N_("Select next ocular"),
							 settings->value("next_ocular", "Ctrl+]").toString(),
							 group, false);
	gui->addGuiActions("actionShow_Ocular_decrement",
							 N_("Select previous ocular"),
							 settings->value("prev_ocular", "Ctrl+[").toString(),
							 group, false);
	gui->addGuiActions("actionShow_Telescope_increment",
							 N_("Select next telescope"),
							 settings->value("next_telescope", "Shift+]").toString(),
							 group, false);
	gui->addGuiActions("actionShow_Telescope_decrement",
							 N_("Select previous telescope"),
							 settings->value("prev_telescope", "Shift+[").toString(),
							 group, false);

	connect(gui->getGuiActions("actionShow_Ocular_Crosshair"), SIGNAL(toggled(bool)), this, SLOT(toggleCrosshair()));


	connect(gui->getGuiActions("actionShow_CCD_increment"), SIGNAL(triggered()), this, SLOT(incrementCCDIndex()));
	connect(gui->getGuiActions("actionShow_CCD_decrement"), SIGNAL(triggered()), this, SLOT(decrementCCDIndex()));
	connect(gui->getGuiActions("actionShow_Ocular_increment"), SIGNAL(triggered()), this, SLOT(incrementOcularIndex()));
	connect(gui->getGuiActions("actionShow_Ocular_decrement"), SIGNAL(triggered()), this, SLOT(decrementOcularIndex()));
	connect(gui->getGuiActions("actionShow_Telescope_increment"), SIGNAL(triggered()), this, SLOT(incrementTelescopeIndex()));
	connect(gui->getGuiActions("actionShow_Telescope_decrement"), SIGNAL(triggered()), this, SLOT(decrementTelescopeIndex()));
	/*
	 connect(telescopesTableModel, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &)), this, SLOT(dataChanged()));
	 connect(telescopesTableModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(rowsInserted()));
	 connect(telescopesTableModel, SIGNAL(rowsRemoved(const QModelIndex &, int, int)), this, SLOT(rowsRemoved()));
	 */
	connect(this, SIGNAL(selectedCCDChanged()), this, SLOT(instrumentChanged()));
	connect(this, SIGNAL(selectedOcularChanged()), this, SLOT(instrumentChanged()));
	connect(this, SIGNAL(selectedTelescopeChanged()), this, SLOT(instrumentChanged()));
	connect(ocularDialog, SIGNAL(scaleImageCircleChanged(bool)), this, SLOT(setScaleImageCircle(bool)));
}


void Oculars::interceptMovementKey(QKeyEvent* event)
{
	// We onle care about the arrow keys.  This flag tracks that.
	bool consumeEvent = false;

	StelCore *core = StelApp::getInstance().getCore();
	StelMovementMgr *movementManager = core->getMovementMgr();

	if (event->type() == QEvent::KeyPress)
	{
		// Direction and zoom replacements
		switch (event->key())
		{
			case Qt::Key_Left:
				movementManager->turnLeft(true);
				consumeEvent = true;
				break;
			case Qt::Key_Right:
				movementManager->turnRight(true);
				consumeEvent = true;
				break;
			case Qt::Key_Up:
				if (!event->modifiers().testFlag(Qt::ControlModifier))
				{
					movementManager->turnUp(true);
				}
				consumeEvent = true;
				break;
			case Qt::Key_Down:
				if (!event->modifiers().testFlag(Qt::ControlModifier))
				{
					movementManager->turnDown(true);
				}
				consumeEvent = true;
				break;
			case Qt::Key_PageUp:
				movementManager->zoomIn(true);
				consumeEvent = true;
				break;
			case Qt::Key_PageDown:
				movementManager->zoomOut(true);
				consumeEvent = true;
				break;
			case Qt::Key_Shift:
				movementManager->moveSlow(true);
				consumeEvent = true;
				break;
		}
	}
	else
	{
		// When a deplacement key is released stop mooving
		switch (event->key())
		{
			case Qt::Key_Left:
				movementManager->turnLeft(false);
				consumeEvent = true;
				break;
			case Qt::Key_Right:
				movementManager->turnRight(false);
				consumeEvent = true;
				break;
			case Qt::Key_Up:
				movementManager->turnUp(false);
				consumeEvent = true;
				break;
			case Qt::Key_Down:
				movementManager->turnDown(false);
				consumeEvent = true;
				break;
			case Qt::Key_PageUp:
				movementManager->zoomIn(false);
				consumeEvent = true;
				break;
			case Qt::Key_PageDown:
				movementManager->zoomOut(false);
				consumeEvent = true;
				break;
			case Qt::Key_Shift:
				movementManager->moveSlow(false);
				consumeEvent = true;
				break;
		}
		if (consumeEvent)
		{
			// We don't want to re-center the object; just hold the current position.
			movementManager->setFlagLockEquPos(true);
		}
	}
	if (consumeEvent)
	{
		event->accept();
	}
	else
	{
		event->setAccepted(false);
	}
}

void Oculars::paintMask()
{
	StelCore *core = StelApp::getInstance().getCore();
	StelProjector::StelProjectorParams params = core->getCurrentStelProjectorParams();

	glDisable(GL_BLEND);
	glColor3f(0.f,0.f,0.f);
	glPushMatrix();
	glTranslated(params.viewportCenter[0], params.viewportCenter[1], 0.0);
	GLUquadricObj *quadric = gluNewQuadric();

	GLdouble inner = 0.5 * params.viewportFovDiameter;

	// See if we need to scale the mask
	if (useMaxEyepieceAngle && oculars[selectedOcularIndex]->appearentFOV() > 0.0) {
		inner = oculars[selectedOcularIndex]->appearentFOV() * inner / maxEyepieceAngle;
	}

	GLdouble outer = params.viewportXywh[2] + params.viewportXywh[3];
	// Draw the mask
	gluDisk(quadric, inner, outer, 256, 1);
	// the gray circle
	glColor3f(0.15f,0.15f,0.15f);
	gluDisk(quadric, inner - 1.0, inner, 256, 1);
	gluDeleteQuadric(quadric);
	// draw sensor rectangle
	if(selectedCCDIndex != -1) {
		CCD *ccd = ccds[selectedCCDIndex];
		if (ccd) {
			glColor4f(0.77, 0.14, 0.16, 0.5);
			Ocular *ocular = oculars[selectedOcularIndex];
			float CCDx = ccd->getActualFOVx(ocular);
			float CCDy = ccd->getActualFOVy(ocular);
			if (CCDx > 0.0 && CCDy > 0.0) {
				glBegin(GL_LINE_LOOP);
				glVertex2f(-CCDx, CCDy);
				glVertex2f(CCDx, CCDy);
				glVertex2f(CCDx, -CCDy);
				glVertex2f(-CCDx, -CCDy);
				glEnd();
			}
		}
}
	glPopMatrix();
}

void Oculars::paintText(const StelCore* core)
{
	const StelProjectorP prj = core->getProjection(StelCore::FrameAltAz);
	StelPainter painter(prj);	

	// Get the current instruments
	CCD *ccd = NULL;
	if(selectedCCDIndex != -1) {
		ccd = ccds[selectedCCDIndex];
	}
	Ocular *ocular = oculars[selectedOcularIndex];
	Telescope *telescope = telescopes[selectedTelescopeIndex];

	// set up the color and the GL state
	painter.setColor(0.8, 0.48, 0.0, 1);
	glDisable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	// Get the X & Y positions, and the line height
	painter.setFont(font);
	QString widthString = "MMMMMMMMMMMMMMMMMMM";
	float insetFromRHS = painter.getFontMetrics().width(widthString);
	StelProjector::StelProjectorParams projectorParams = core->getCurrentStelProjectorParams();
	int xPosition = projectorParams.viewportXywh[2];
	xPosition -= insetFromRHS;
	int yPosition = projectorParams.viewportXywh[3];
	yPosition -= 40;
	const int lineHeight = painter.getFontMetrics().height();
	
	// The CCD
	QString ccdsensorLabel, ccdInfoLabel;
	if (ccd && ccd->chipWidth() > .0 && ccd->chipHeight() > .0) {
		ccdInfoLabel = "Dimension : " + QVariant(ccd->chipWidth()).toString() + "x" + QVariant(ccd->chipHeight()).toString() + " mm";
		if (ccd->name() != QString("")) {
			ccdsensorLabel = "Sensor #" + QVariant(selectedCCDIndex).toString();
			ccdsensorLabel.append(" : ").append(ccd->name());
		}
	}
	if (ccdsensorLabel != QString("")) {
		painter.drawText(xPosition, yPosition, ccdsensorLabel);
		yPosition-=lineHeight;
		painter.drawText(xPosition, yPosition, ccdInfoLabel);
		yPosition-=lineHeight;
	}

	// The Ocular
	QString ocularNumberLabel = "Ocular #" + QVariant(selectedOcularIndex).toString();
	if (ocular->name() != QString(""))  {
		ocularNumberLabel.append(" : ").append(ocular->name());
	}
	painter.drawText(xPosition, yPosition, ocularNumberLabel);
	yPosition-=lineHeight;

	QString ocularFLLabel = "Ocular FL: " + QVariant(ocular->effectiveFocalLength()).toString() + QChar(0x00B0);
	painter.drawText(xPosition, yPosition, ocularFLLabel);
	yPosition-=lineHeight;

	QString ocularFOVLabel = "Ocular aFOV: " + QVariant(ocular->appearentFOV()).toString() + QChar(0x00B0);
	painter.drawText(xPosition, yPosition, ocularFOVLabel);
	yPosition-=lineHeight;

	// The telescope
	QString telescopeNumberLabel = "Telescope #" + QVariant(selectedTelescopeIndex).toString();
	if (telescope->name() != QString(""))  {
		telescopeNumberLabel.append(" : ").append(telescope->name());
	}
	painter.drawText(xPosition, yPosition, telescopeNumberLabel);
	yPosition-=lineHeight;

	// General info
	QString magnificationLabel = "Magnification: " + QVariant(((int)(ocular->magnification(telescope) * 10.0)) / 10.0).toString() + "x";
	painter.drawText(xPosition, yPosition, magnificationLabel);
	yPosition-=lineHeight;

	QString fovLabel = "FOV: " + QVariant(((int)(ocular->actualFOV(telescope) * 10000.00)) / 10000.0).toString() + QChar(0x00B0);
	painter.drawText(xPosition, yPosition, fovLabel);
}

void Oculars::validateAndLoadIniFile()
{
	// Insure the module directory exists
	StelFileMgr::makeSureDirExistsAndIsWritable(StelFileMgr::getUserDir()+"/modules/Oculars");
	StelFileMgr::Flags flags = (StelFileMgr::Flags)(StelFileMgr::Directory|StelFileMgr::Writable);
	QString ocularIniPath = StelFileMgr::findFile("modules/Oculars/", flags) + "ocular.ini";

	// If the ini file does not already exist, create it from the resource in the QT resource
	if(!QFileInfo(ocularIniPath).exists()) {
		QFile src(":/ocular/default_ocular.ini");
		if (!src.copy(ocularIniPath)) {
			qWarning() << "Oculars::validateIniFile cannot copy default_ocular.ini resource to [non-existing] "
					+ ocularIniPath;
		} else {
			qDebug() << "Oculars::validateIniFile copied default_ocular.ini to " << ocularIniPath;
			// The resource is read only, and the new file inherits this, so set write-able.
			QFile dest(ocularIniPath);
			dest.setPermissions(dest.permissions() | QFile::WriteOwner);
		}
	} else {
		qDebug() << "Oculars::validateIniFile ocular.ini exists at: " << ocularIniPath << ". Checking version...";
		QSettings settings(ocularIniPath, QSettings::IniFormat);
		double ocularsVersion = settings.value("oculars_version", "0.0").toDouble();
		qWarning() << "Oculars::validateIniFile found existing ini file version " << ocularsVersion;

		if (ocularsVersion < MIN_OCULARS_INI_VERSION) {
			qWarning() << "Oculars::validateIniFile existing ini file version " << ocularsVersion
						<< " too old to use; required version is " << MIN_OCULARS_INI_VERSION << ". Copying over new one.";
			// delete last "old" file, if it exists
			QFile deleteFile(ocularIniPath + ".old");
			deleteFile.remove();

			// Rename the old one, and copy over a new one
			QFile oldFile(ocularIniPath);
			if (!oldFile.rename(ocularIniPath + ".old")) {
				qWarning() << "Oculars::validateIniFile cannot move ocular.ini resource to ocular.ini.old at path  "
						+ ocularIniPath;
			} else {
				qWarning() << "Oculars::validateIniFile ocular.ini resource renamed to ocular.ini.old at path  "
						+ ocularIniPath;
				QFile src(":/ocular/default_ocular.ini");
				if (!src.copy(ocularIniPath)) {
					qWarning() << "Oculars::validateIniFile cannot copy default_ocular.ini resource to [non-existing] "
							+ ocularIniPath;
				} else {
					qDebug() << "Oculars::validateIniFile copied default_ocular.ini to " << ocularIniPath;
					// The resource is read only, and the new file inherits this...  make sure the new file
					// is writable by the Stellarium process so that updates can be done.
					QFile dest(ocularIniPath);
					dest.setPermissions(dest.permissions() | QFile::WriteOwner);
				}
			}
		}
	}
	settings = new QSettings(ocularIniPath, QSettings::IniFormat, this);
}

QSettings* Oculars::appSettings()
{
	return settings;
}

void Oculars::unzoomOcular()
{
	StelCore *core = StelApp::getInstance().getCore();
	StelMovementMgr *movementManager = core->getMovementMgr();
	GridLinesMgr *gridManager = (GridLinesMgr *)StelApp::getInstance().getModuleMgr().getModule("GridLinesMgr");

	gridManager->setFlagAzimuthalGrid(flagAzimuthalGrid);
	gridManager->setFlagEquatorGrid(flagEquatorGrid);
	gridManager->setFlagEquatorJ2000Grid(flagEquatorJ2000Grid);
	gridManager->setFlagEquatorLine(flagEquatorLine);
	gridManager->setFlagEclipticLine(flagEclipticLine);
	gridManager->setFlagMeridianLine(flagMeridianLine);
	movementManager->setFlagTracking(false);
	movementManager->setFlagEnableZoomKeys(true);
	movementManager->setFlagEnableMouseNavigation(true);

	// Set the screen display
	// core->setMaskType(StelProjector::MaskNone);
	core->setFlipHorz(false);
	core->setFlipVert(false);

	movementManager->zoomTo(movementManager->getInitFov());
}

void Oculars::zoom(bool rezoom)
{
	if (flagShowOculars)  {
		if (!rezoom)  {
			GridLinesMgr *gridManager = (GridLinesMgr *)StelApp::getInstance().getModuleMgr().getModule("GridLinesMgr");
			// Current state
			flagAzimuthalGrid = gridManager->getFlagAzimuthalGrid();
			flagEquatorGrid = gridManager->getFlagEquatorGrid();
			flagEquatorJ2000Grid = gridManager->getFlagEquatorJ2000Grid();
			flagEquatorLine = gridManager->getFlagEquatorLine();
			flagEclipticLine = gridManager->getFlagEclipticLine();
			flagMeridianLine = gridManager->getFlagMeridianLine();
		}

		// set new state
		zoomOcular();
	} else {
		//reset to original state
		unzoomOcular();
	}
}

void Oculars::zoomOcular()
{
	StelCore *core = StelApp::getInstance().getCore();
	StelMovementMgr *movementManager = core->getMovementMgr();
	GridLinesMgr *gridManager =
		(GridLinesMgr *)StelApp::getInstance().getModuleMgr().getModule("GridLinesMgr");

	gridManager->setFlagAzimuthalGrid(false);
	gridManager->setFlagEquatorGrid(false);
	gridManager->setFlagEquatorJ2000Grid(false);
	gridManager->setFlagEquatorLine(false);
	gridManager->setFlagEclipticLine(false);
	gridManager->setFlagMeridianLine(false);
	
	movementManager->setFlagTracking(true);
	movementManager->setFlagEnableZoomKeys(false);
	movementManager->setFlagEnableMouseNavigation(false);
	
	// We won't always have a selected object
	if (StelApp::getInstance().getStelObjectMgr().getWasSelected()) {
		movementManager->
			moveToJ2000(StelApp::getInstance().getStelObjectMgr().getSelectedObject()[0]->getEquinoxEquatorialPos(core->getNavigator()),
						0.0,
						1);
	}

	// Set the screen display
	// core->setMaskType(StelProjector::MaskDisk);
	Ocular *ocular = oculars[selectedOcularIndex];
	Telescope *telescope = telescopes[selectedTelescopeIndex];
	core->setFlipHorz(telescope->isHFlipped());
	core->setFlipVert(telescope->isVFlipped());

	double actualFOV = ocular->actualFOV(telescope);
	// See if the mask was scaled; if so, correct the actualFOV.
	if (useMaxEyepieceAngle && ocular->appearentFOV() > 0.0) {
		actualFOV = maxEyepieceAngle * actualFOV / ocular->appearentFOV();
	}
	movementManager->zoomTo(actualFOV, 0.0);
}


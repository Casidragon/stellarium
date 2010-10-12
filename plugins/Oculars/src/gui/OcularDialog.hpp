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

#ifndef _OCULARDIALOG_HPP_
#define _OCULARDIALOG_HPP_

#include <QObject>
#include "CCD.hpp"
#include "Ocular.hpp"
#include "StelDialogOculars.hpp"
#include "StelStyle.hpp"
#include "Telescope.hpp"

class Ui_ocularDialogForm;

QT_BEGIN_NAMESPACE
class QDataWidgetMapper;
class QDoubleValidator;
class QIntValidator;
class QRegExpValidator;
class QModelIndex;
QT_END_NAMESPACE


class OcularDialog : public StelDialogOculars
{
	Q_OBJECT

public:
	OcularDialog(QList<CCD *> ccds, QList<Ocular *> oculars, QList<Telescope *> telescopes);
	virtual ~OcularDialog();
	void languageChanged();
	//! Notify that the application style changed
	void styleChanged();
	void setOculars(QList<Ocular*> theOculars);
	void updateStyle();

public slots:
	void closeWindow();
	void deleteSelectedCCD();
	void deleteSelectedOcular();
	void deleteSelectedTelescope();
	void insertNewCCD();
	void insertNewOcular();
	void insertNewTelescope();
	void ccdSelected(int currentIndex);
	void ocularSelected(int currentIndex);
	void telescopeSelected(int currentIndex);
	void updateCCD();
	void updateOcular();
	void updateTelescope();

signals:
	void scaleImageCircleChanged(bool state);
protected:
	//! Initialize the dialog widgets and connect the signals/slots
	virtual void createDialogContent();
	Ui_ocularDialogForm* ui;

private slots:
	void scaleImageCircleStateChanged(int state);

private:
	QDataWidgetMapper *ccdMapper;
	QList<CCD *> ccds;
	QDataWidgetMapper *ocularMapper;
	QList<Ocular *> oculars;
	QDataWidgetMapper *telescopeMapper;
	QList<Telescope *> telescopes;
	QIntValidator *validatorOcularAFOV;
	QDoubleValidator *validatorOcularEFL;
	QDoubleValidator *validatorTelescopeDiameter;
	QDoubleValidator *validatorTelescopeFL;
	QRegExpValidator *validatorName;
	QIntValidator *validatorPositiveInt;
	QDoubleValidator *validatorPositiveDouble;
};

#endif // _OCULARDIALOG_HPP_

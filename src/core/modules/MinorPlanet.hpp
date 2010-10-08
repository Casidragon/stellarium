/*
 * Stellarium
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
 
#ifndef _MINOR_PLANET_HPP_
#define _MINOR_PLANET_HPP_

#include "Planet.hpp"
 
class MinorPlanet : public Planet
{
public:
	MinorPlanet(const QString& englishName,
	       int flagLighting,
	       double radius,
	       double oblateness,
	       Vec3f color,
	       float albedo,
	       const QString& texMapName,
	       posFuncType _coordFunc,
	       void* userDataPtr,
	       OsulatingFunctType *osculatingFunc,
	       bool closeOrbit,
	       bool hidden,
	       bool hasAtmosphere); //TODO: Remove this one :)

	~MinorPlanet();

	//Inherited from StelObject via Planet
	//! Get a string with data about the Planet.
	//! Planets support the following InfoStringGroup flags:
	//! - Name
	//! - Magnitude
	//! - RaDec
	//! - AltAzi
	//! - Distance
	//! - Size
	//! - PlainText
	//! @param core the Stelore object
	//! @param flags a set of InfoStringGroup items to include in the return value.
	//! @return a QString containing an HMTL encoded description of the Planet.
	virtual QString getInfoString(const StelCore *core, const InfoStringGroup &flags) const;
	virtual QString getType() const {return "MinorPlanet";} //TODO: Or "Asteroid"?
	virtual float getVMagnitude(const StelNavigator *nav) const;

	//! set the minor planet's number, if any.
	//! The number should be specified as an additional parameter, as
	//! englishName is passed as a constant to Planet's constructor.
	//! The number can be set only once. Any further calls of this function will
	//! have no result.
	void setMinorPlanetNumber(int number);

	//! sets a provisional designation.
	//! At the moment, the only role is for it to be displayed in the info
	//! field.
	//! \todo Support more than one provisional designations.
	//! \todo Include them in the search lists.
	void setProvisionalDesignation(QString designation);


	//! sets absolute magnitude (H) and slope parameter (G).
	//! These are the parameters in the IAU's two-parameter magnitude system
	//! for minor planets. They are used to calculate the apparent magnitude at
	//! different phase angles.
	void setAbsoluteMagnitudeAndSlope(double magnitude, double slope);

	//! renders the subscript in a minor planet provisional designation with HTML.
	//! \returns an emtpy string if the source string is not a provisional
	//! designation.
	static QString renderProvisionalDesignationinHtml(QString plainText);


private:
	int minorPlanetNumber;
	double absoluteMagnitude;
	double slopeParameter;

	bool nameIsProvisionalDesignation;
	QString provisionalDesignationHtml;
	static QString convertNameToHtml (QString plainTextName);

};

#endif //_MINOR_PLANET_HPP_

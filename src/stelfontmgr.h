/*
 * Copyright (C) 2006 Fabien Chereau
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
 
#ifndef STELFONTMGR_H
#define STELFONTMGR_H

#include <map>
#include <string>

using namespace std;

//! Manage font for Stellarium. Take into account special font for special language.
//! @author Fabien Chéreau <stellarium@free.fr>
class StelFontMgr
{
public:
	StelFontMgr();

	~StelFontMgr();
	
	//! Class which describes which font to use for a given langage ISO code.
	class FontForLanguage
	{
		public:
			string langageName;
			string fontFileName;
			double fontScale;
			string fixedFontFileName;
			double fixedFontScale;
			bool operator == (const FontForLanguage & f) const;
	};
	
	//! Return the structure describing the fonts and scales to use for a given language
	FontForLanguage& getFontForLocale(const string &langageName);
	
private:
	//! Load the associations between langages and font file/scaling
	void loadFontForLanguage(const string &fontMapFile);
	
	// Contains a mapping of font/langage
	std::map<string, FontForLanguage> fontMapping;
};

#endif

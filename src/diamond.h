/* Diamond
   Sets starting position when captured

   Copyright (C) 2000  Mathias Broxvall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef DIAMOND_H
#define DIAMOND_H

#include "animated.h"

class Diamond : public Animated {
 public:
  Diamond(Coord3d position);
  void draw();
  void draw2();
  void tick(Real t);
  virtual void onGet();

 protected:
  Real fade;
  int taken;
};

#endif

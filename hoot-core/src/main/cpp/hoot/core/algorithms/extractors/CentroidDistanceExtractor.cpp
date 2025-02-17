/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2005 VividSolutions (http://www.vividsolutions.com/)
 * @copyright Copyright (C) 2015-2023 Maxar (http://www.maxar.com/)
 */
#include "CentroidDistanceExtractor.h"

// hoot
#include <hoot/core/elements/ElementGeometryUtils.h>
#include <hoot/core/geometry/ElementToGeometryConverter.h>
#include <hoot/core/geometry/GeometryUtils.h>
#include <hoot/core/util/Factory.h>

using namespace geos::geom;
using namespace std;

namespace hoot
{

HOOT_FACTORY_REGISTER(FeatureExtractor, CentroidDistanceExtractor)

double CentroidDistanceExtractor::distance(const OsmMap& map, const std::shared_ptr<const Element>& target,
                                           const std::shared_ptr<const Element>& candidate) const
{
  ConstOsmMapPtr pmap = map.shared_from_this();
  Coordinate tc = ElementGeometryUtils::calculateElementCentroid(target->getElementId(), pmap);
  Coordinate cc = ElementGeometryUtils::calculateElementCentroid(candidate->getElementId(), pmap);

  if (tc == Coordinate::getNull() || cc == Coordinate::getNull())
    return nullValue();

  return tc.distance(cc);
}

}

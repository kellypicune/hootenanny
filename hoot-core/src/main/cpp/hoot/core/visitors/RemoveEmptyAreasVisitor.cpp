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
 * @copyright Copyright (C) 2015, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "RemoveEmptyAreasVisitor.h"

// geos
#include <geos/geom/Geometry.h>

// hoot
#include <hoot/core/criterion/AreaCriterion.h>
#include <hoot/core/criterion/BuildingCriterion.h>
#include <hoot/core/geometry/ElementToGeometryConverter.h>
#include <hoot/core/geometry/GeometryUtils.h>
#include <hoot/core/ops/RecursiveElementRemover.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/util/StringUtils.h>

using namespace geos::geom;
namespace hoot
{

HOOT_FACTORY_REGISTER(ElementVisitor, RemoveEmptyAreasVisitor)

RemoveEmptyAreasVisitor::RemoveEmptyAreasVisitor()
  : _requireAreaForPolygonConversion(true)
{
}

void RemoveEmptyAreasVisitor::setConfiguration(const Settings& conf)
{
  _requireAreaForPolygonConversion = ConfigOptions(conf).getConvertRequireAreaForPolygon();
  LOG_VARD(_requireAreaForPolygonConversion);
}

void RemoveEmptyAreasVisitor::visit(const std::shared_ptr<Element>& e)
{
  if (!e || e->getElementType() == ElementType::Node)
    return;

  LOG_VART(e->getElementId());
  //LOG_VARD(e);

  if (!_ec.get())
  {
    _ec = std::make_shared<ElementToGeometryConverter>(_map->shared_from_this());
    // TODO: This is directly related to the relation change commented out below. If this logic
    // isn't needed, then we can remove implementation of the Configurable interface.
    _ec->setRequireAreaForPolygonConversion(_requireAreaForPolygonConversion);
  }

  LOG_VART(AreaCriterion().isSatisfied(e));
  if (AreaCriterion().isSatisfied(e))
  {
    std::shared_ptr<Geometry> g = _ec->convertToGeometry(e);
    LOG_VART(g.get());
    if (g.get() && !g->isEmpty())
    {
      LOG_VART(GeometryUtils::geometryTypeIdToString(g->getGeometryTypeId()));
      LOG_VART(g->getArea());
    }
    bool removeArea = false;
    if (g && g->getArea() == 0.0)
    {
      if (e->getElementType() == ElementType::Relation)
      {
        // require that all way children of this relation have empty areas in order to remove it
        ConstRelationPtr relation = std::dynamic_pointer_cast<const Relation>(e);
        const std::vector<RelationData::Entry>& members = relation->getMembers();
        bool anyMemberHasPositiveArea = false;
        for (const auto& member : members)
        {
          // not going down more than one relation level here, but that may eventually need to be
          // done; Should we also require that each child way also satisfy AreaCriterion?
          LOG_VART(member.getElementId());
          if (member.getElementId().getType() == ElementType::Way)
          {
            ConstWayPtr memberWay = std::dynamic_pointer_cast<const Way>(_map->getElement(member.getElementId()));
            if (memberWay)
            {
              std::shared_ptr<Geometry> wayGeom = _ec->convertToGeometry(memberWay);
              LOG_VART(wayGeom->toString());
              if (wayGeom && !wayGeom->isEmpty() && wayGeom->getArea() > 0.0)
              {
                LOG_TRACE(memberWay->getElementId() << " has positive area.");
                anyMemberHasPositiveArea = true;
                break;
              }
            }
          }
        }
        removeArea = !anyMemberHasPositiveArea;
      }
      else
        removeArea = true;
    }

    if (removeArea)
    {
      LOG_TRACE("Recursively removing empty area: " << e->getElementId() << "...");
      RecursiveElementRemover(e->getElementId()).apply(_map->shared_from_this());
      _numAffected++;
    }
  }

  _numProcessed++;
  if (_numProcessed % 10000 == 0)
  {
    PROGRESS_INFO(
      "\tProcessed " << StringUtils::formatLargeNumber(_numProcessed) <<
      " / " << StringUtils::formatLargeNumber(_map->size()) << " elements for empty area removal.");
  }
}

QStringList RemoveEmptyAreasVisitor::getCriteria() const
{
  QStringList criteria;
  // need to also add building here for unecessary conflation op removal purposes, since
  // AreaCriterion includes buildings in some instances
  criteria.append(BuildingCriterion::className());
  criteria.append(AreaCriterion::className());
  return criteria;
}

}

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
 * @copyright Copyright (C) 2018-2023 Maxar (http://www.maxar.com/)
 */
#include "NonConflatableCriterion.h"

// hoot
#include <hoot/core/criterion/ConflatableElementCriterion.h>
#include <hoot/core/elements/Element.h>
#include <hoot/core/util/Factory.h>

namespace hoot
{

HOOT_FACTORY_REGISTER(ElementCriterion, NonConflatableCriterion)

NonConflatableCriterion::NonConflatableCriterion()
  : _ignoreChildren(false),
    _geometryTypeFilter(GeometryTypeCriterion::GeometryType::Unknown),
    _ignoreGenericConflators(false)
{
}

NonConflatableCriterion::NonConflatableCriterion(ConstOsmMapPtr map)
  : ConstOsmMapConsumerBase(map),
    _ignoreChildren(false),
    _geometryTypeFilter(GeometryTypeCriterion::GeometryType::Unknown),
    _ignoreGenericConflators(false)
{
}

void NonConflatableCriterion::setConfiguration(const Settings& conf)
{
  ConfigOptions config = ConfigOptions(conf);
  _ignoreChildren = config.getNonConflatableCriterionIgnoreRelationMembers();
  LOG_VARD(_ignoreChildren);
}

bool NonConflatableCriterion::isSatisfied(const ConstElementPtr& e) const
{
  LOG_VART(e->getElementId());
  const QMap<QString, ElementCriterionPtr> conflatableCriteria = ConflatableElementCriterion::getConflatableCriteria();
  for (auto itr = conflatableCriteria.begin(); itr != conflatableCriteria.end(); ++itr)
  {
    ElementCriterionPtr crit = itr.value();

    if (_map)
    {
      ConstOsmMapConsumer* mapConsumer = dynamic_cast<ConstOsmMapConsumer*>(crit.get());
      if (mapConsumer != nullptr)
        mapConsumer->setOsmMap(_map.get());
    }

    if (_ignoreGenericConflators)
    {
      std::shared_ptr<ConflatableElementCriterion> conflatableCrit = std::dynamic_pointer_cast<ConflatableElementCriterion>(crit);
      assert(conflatableCrit);
      if (!conflatableCrit->supportsSpecificConflation())
      {
        LOG_TRACE(
          "Element: " << e->getElementId() << " does not support specific conflation and " <<
          "generic conflators are being ignored.");
        continue;
      }
    }

    bool satisfiesGeometryFilter = true;
    if (_geometryTypeFilter != GeometryTypeCriterion::GeometryType::Unknown)
    {
      std::shared_ptr<GeometryTypeCriterion> geometryCrit = std::dynamic_pointer_cast<GeometryTypeCriterion>(crit);
      satisfiesGeometryFilter = geometryCrit->getGeometryType() == _geometryTypeFilter;
      LOG_VART(_geometryTypeFilter);
      LOG_VART(satisfiesGeometryFilter);
    }

    LOG_VART(itr.key());
    if (crit->isSatisfied(e) && satisfiesGeometryFilter)
    {
      // It is something we can conflate.
      LOG_TRACE("Element: " << e->getElementId() << " is conflatable with: " << itr.key());
      return false;
    }
  }
  // Technically, there could also be something like a building way with a POI child and you'd want
  // to check for ways here as well. Will wait to support that situation until an actual use case
  // is encountered.
  if (!_ignoreChildren && e->getElementType() == ElementType::Relation)
  {
    // We need to verify that none of the child relation members are conflatable in order to sign
    // off on this element not being conflatable.

    ConstRelationPtr relation = std::dynamic_pointer_cast<const Relation>(e);
    const std::vector<RelationData::Entry>& members = relation->getMembers();
    for (const auto& member : members)
    {
      ConstElementPtr memberElement = _map->getElement(member.getElementId());
      if (memberElement && isSatisfied(memberElement))
      {
        // It is something we can conflate.
        LOG_TRACE(
          "Element: " << e->getElementId() << " has a child that is conflatable: " <<
          memberElement->getElementId() << ", and member children are not being ignored, " <<
          "therefore it is conflatable.");
        return false;
      }
    }
  }
  // It is not something we can conflate.
  LOG_TRACE("Element: " << e->getElementId() << " is not conflatable.");
  return true;
}

}

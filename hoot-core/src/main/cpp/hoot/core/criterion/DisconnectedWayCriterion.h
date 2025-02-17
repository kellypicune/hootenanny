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
 * @copyright Copyright (C) 2020-2023 Maxar (http://www.maxar.com/)
 */
#ifndef DISCONNECTED_WAY_CRITERION_H
#define DISCONNECTED_WAY_CRITERION_H

// hoot
#include <hoot/core/criterion/ElementCriterion.h>
#include <hoot/core/elements/ConstOsmMapConsumer.h>
#include <hoot/core/elements/OsmMap.h>

namespace hoot
{

/**
 * @brief The DisconnectedWayCriterion class identifies ways that are disconnected from other ways.
 */
class DisconnectedWayCriterion : public ElementCriterion, public ConstOsmMapConsumerBase
{
public:

  static QString className() { return "DisconnectedWayCriterion"; }

  DisconnectedWayCriterion() = default;
  DisconnectedWayCriterion(ConstOsmMapPtr map) : ConstOsmMapConsumerBase(map) { }
  ~DisconnectedWayCriterion() override = default;

  /**
   * @see ElementCriterion
   */
  bool isSatisfied(const ConstElementPtr& e) const override;
  ElementCriterionPtr clone() override { return std::make_shared<DisconnectedWayCriterion>(_map); }

  QString getDescription() const override
  { return "Identifies ways that are connected to no other ways"; }
  QString getName() const override { return className(); }
  QString getClassName() const override { return className(); }
  QString toString() const override { return className(); }

};

}

#endif // DISCONNECTED_WAY_CRITERION_H

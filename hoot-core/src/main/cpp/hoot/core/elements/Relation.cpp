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
 * @copyright Copyright (C) 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022, 2023 Maxar (http://www.maxar.com/)
 */
#include "Relation.h"

// geos
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/LineString.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Polygon.h>

// hoot
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/elements/Way.h>
#include <hoot/core/visitors/ConstElementVisitor.h>

using namespace geos::geom;
using namespace std;

namespace hoot
{

int Relation::logWarnCount = 0;

/**
 * This is a convenience class to handle cases when exceptions are thrown.
 */
class AddToVisitedRelationsList
{
public:

  AddToVisitedRelationsList(QList<long>& relationIds, long thisId)
    : _relationIds(relationIds),
      _thisId(thisId)
  {
    relationIds.append(thisId);
  }

  ~AddToVisitedRelationsList()
  {
    assert(_relationIds.size() >= 1);
    assert(_relationIds.at(_relationIds.size() - 1) == _thisId);
    _relationIds.removeLast();
  }

private:

  QList<long>& _relationIds;
  long _thisId;
};

Relation::Relation(Status s, long id, Meters circularError, QString type, long changeset, long version,
                   OsmTimestamp timestamp, QString user, long uid, bool visible)
  : Element(s),
    _relationData(std::make_shared<RelationData>(id, changeset, version, timestamp, user, uid, visible))
{
  _relationData->setCircularError(circularError);
  _relationData->setType(type);
}

Relation::Relation(const Relation& from)
  : Element(from.getStatus()),
    _relationData(std::make_shared<RelationData>(*from._relationData.get()))
{
}

void Relation::addElement(const QString& role, const std::shared_ptr<const Element>& e)
{
  addElement(role, e->getElementType(), e->getId());
}

void Relation::addElement(const QString& role, ElementId eid)
{
  //  Don't add self-references
  if (eid.getType() == ElementType::Relation && eid.getId() == getId())
    return;
  _preGeometryChange();
  _makeWritable();
  _relationData->addElement(role, eid);
  _postGeometryChange();
}

void Relation::addElement(const QString& role, ElementType t, long id)
{
  addElement(role, ElementId(t, id));
}

void Relation::clear()
{
  _preGeometryChange();
  _makeWritable();
  _relationData->clear();
  _postGeometryChange();
}

RelationData::Entry Relation::getMember(const ElementId& elementId) const
{
  const size_t index = indexOf(elementId);
  if (static_cast<int>(index) == -1)
    return RelationData::Entry();
  return getMembers().at(index);
}

QString Relation::getRole(const ElementId& elementId) const
{
  const RelationData::Entry member = getMember(elementId);
  if (member.getElementId().getType() == ElementType::Unknown)
    return "";
  return member.getRole();
}

bool Relation::contains(const ElementId& eid) const
{
  return static_cast<int>(indexOf(eid)) != -1;
}

size_t Relation::indexOf(const ElementId& eid) const
{
  const vector<RelationData::Entry>& members = getMembers();
  for (size_t i = 0; i < members.size(); i++)
  {
    if (members[i].getElementId() == eid)
      return i;
  }
  return -1;
}

ElementId Relation::memberIdAt(const size_t index) const
{
  const std::vector<RelationData::Entry>& members = getMembers();
  if (static_cast<int>(index) >= 0 && index < members.size())
    return getMembers().at(index).getElementId();
  return ElementId();
}

bool Relation::isFirstMember(const ElementId& eid) const
{
  return indexOf(eid) == 0;
}

bool Relation::isLastMember(const ElementId& eid) const
{
  return indexOf(eid) == getMemberCount() - 1;
}

void Relation::insertElement(const QString& role, const ElementId& elementId, size_t pos)
{
  //  Don't add self-references
  if (elementId.getType() == ElementType::Relation && elementId.getId() == getId())
    return;

  _preGeometryChange();
  _makeWritable();

  vector<RelationData::Entry> members = getMembers();
  RelationData::Entry newMember(role, elementId);
  members.insert(members.begin() + pos, newMember);
  setMembers(members);

  _postGeometryChange();
}

int Relation::numElementsByRole(const QString& role) const
{
  return static_cast<int>(getElementsByRole(role).size());
}

std::vector<RelationData::Entry> Relation::getElementsByRole(const QString& role) const
{
  const vector<RelationData::Entry>& members = getMembers();
  std::vector<RelationData::Entry> membersByRole;
  for (const auto& member : members)
  {
    if (member.getRole() == role)
      membersByRole.push_back(member);
  }
  return membersByRole;
}

std::set<ElementId> Relation::getMemberIds(const ElementType& elementType) const
{
  std::set<ElementId> memberIds;
  const vector<RelationData::Entry>& members = getMembers();
  for (const auto& member : members)
  {
    if (elementType == ElementType::Unknown || member.getElementId().getType() == elementType)
      memberIds.insert(member.getElementId());
  }
  return memberIds;
}

std::shared_ptr<Envelope> Relation::getEnvelope(const std::shared_ptr<const ElementProvider>& ep) const
{
  set<long> visited;
  return std::make_shared<Envelope>(getEnvelopeInternal(ep, visited));
}

std::shared_ptr<Envelope> Relation::getEnvelope(const std::shared_ptr<const ElementProvider>& ep, std::set<long>& visited) const
{
  return std::make_shared<Envelope>(getEnvelopeInternal(ep, visited));
}

const Envelope& Relation::getEnvelopeInternal(const std::shared_ptr<const ElementProvider>& ep) const
{
  set<long> visited;
  return getEnvelopeInternal(ep, visited);
}

const geos::geom::Envelope& Relation::getEnvelopeInternal(const std::shared_ptr<const ElementProvider>& ep, std::set<long>& visited) const
{
  LOG_VART(ep.get());

  _cachedEnvelope.init();
  //  Check if we've seen
  if (visited.find(getId()) != visited.end())
  {
    _cachedEnvelope.setToNull();
    return _cachedEnvelope;
  }
  //  Add this relation to the visited list
  visited.insert(getId());

  const vector<RelationData::Entry>& members = getMembers();
  for (const auto& m : members)
  {
    ElementId eid = m.getElementId();
    LOG_VART(eid);
    //  Ignore self references and reference loops
    if (eid.getType() == ElementType::Relation && (eid.getId() == getId() || visited.find(eid.getId()) != visited.end()))
      continue;

    // If any of the elements don't exist, then return an empty envelope.
    if (ep->containsElement(eid) == false)
    {
      LOG_TRACE(eid << " missing.  Returning empty envelope...");
      _cachedEnvelope.setToNull();
      return _cachedEnvelope;
    }

    const std::shared_ptr<const Element> e = ep->getElement(eid);
    LOG_VART(e.get());
    if (e)
    {
      std::shared_ptr<Envelope> childEnvelope;
      //  Relations need different handling
      if (eid.getType() == ElementType::Relation)
        childEnvelope = std::dynamic_pointer_cast<const Relation>(e)->getEnvelope(ep, visited);
      else
        childEnvelope = e->getEnvelope(ep);
      LOG_VART(childEnvelope.get());
      LOG_VART(childEnvelope->isNull());
      if (childEnvelope->isNull())
      {
        LOG_TRACE("Child envelope for " << eid << " null.  Returning empty envelope...");
        _cachedEnvelope.setToNull();
        return _cachedEnvelope;
      }
      _cachedEnvelope.expandToInclude(childEnvelope.get());
    }
  }

  LOG_VART(_cachedEnvelope);
  return _cachedEnvelope;
}

void Relation::_makeWritable()
{
  // Make sure we're the only one with a reference to the data before we modify it.
  if (_relationData.use_count() > 1)
    _relationData = std::make_shared<RelationData>(*_relationData);
}

void Relation::removeElement(const QString& role, const std::shared_ptr<const Element>& e)
{
  removeElement(role, e->getElementId());
}

void Relation::removeElement(const QString& role, ElementId eid)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->removeElement(role, eid);
  _postGeometryChange();
}

void Relation::removeElement(ElementId eid)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->removeElement(eid);
  _postGeometryChange();
}

void Relation::replaceElement(const std::shared_ptr<const Element>& from,
                              const std::shared_ptr<const Element>& to)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->replaceElement(from->getElementId(), to->getElementId());
  _postGeometryChange();
}

void Relation::replaceElement(const ElementId& from, const ElementId& to)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->replaceElement(from, to);
  _postGeometryChange();
}

void Relation::replaceElement(const ConstElementPtr& from, const QList<ElementPtr>& to)
{
  LOG_TRACE("Replacing " << from->getElementId() << " with " << to << "...");
  _preGeometryChange();
  _makeWritable();
  QList<ElementId> copy;
  for (const auto& element : qAsConst(to))
    copy.append(element->getElementId());
  _relationData->replaceElement(from->getElementId(), copy);
  _postGeometryChange();
}

void Relation::setMembers(const vector<RelationData::Entry>& members)
{
  _preGeometryChange();
  _makeWritable();
  _relationData->setMembers(members);
  _postGeometryChange();
}

void Relation::setType(const QString& type)
{
  _makeWritable();
  _relationData->setType(type);
}

QString Relation::toString() const
{
  stringstream ss(stringstream::out);
  ss << "relation(" << getId() << ")" << endl;
  ss << "type: " << getType() << endl;
  ss << "members: ";
  for (const auto& member : getMembers())
    ss << "  " << member.toString().toUtf8().data() << endl;
  ss << endl;
  ss << "tags: " << getTags().toString().toUtf8().data();
  ss << "status: " << getStatusString().toStdString() << endl;
  ss << "version: " << getVersion() << endl;
  ss << "visible: " << getVisible();
  if (hasCircularError())
    ss << endl << "circular error: " << getCircularError();
  return QString::fromUtf8(ss.str().data());
}

void Relation::visitRo(const ElementProvider& map, ConstElementVisitor& filter,
                       const bool recursive) const
{
  QList<long> visitedRelations;
  _visitRo(map, filter, visitedRelations, recursive);
  assert(visitedRelations.size() == 0);
}

void Relation::_visitRo(const ElementProvider& map, ConstElementVisitor& filter,
                        QList<long>& visitedRelations, const bool recursive) const
{
  if (visitedRelations.contains(getId()))
  {
    // removed warning logging for these now that we have a cleaner in the pipeline to remove
    // these types of circular refs
    if (logWarnCount < Log::getWarnMessageLimit())
    {
      LOG_TRACE("Invalid data. This relation contains a circular reference. " + toString());
    }
    else if (logWarnCount == Log::getWarnMessageLimit())
    {
      LOG_TRACE(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
    }
    logWarnCount++;
    return;
  }

  AddToVisitedRelationsList addTo(visitedRelations, getId());

  filter.visit(map.getRelation(getId()));

  if (recursive)
  {
    const vector<RelationData::Entry>& members = getMembers();
    for (const auto& m : members)
    {
      LOG_VART(m.getElementId());
      LOG_VART(map.containsElement(m.getElementId()));
      if (map.containsElement(m.getElementId()))
      {
        switch(m.getElementId().getType().getEnum())
        {
        case ElementType::Node:
          map.getNode(m.getElementId().getId())->visitRo(map, filter);
          break;
        case ElementType::Way:
          map.getWay(m.getElementId().getId())->visitRo(map, filter);
          break;
        case ElementType::Relation:
          map.getRelation(m.getElementId().getId())->_visitRo(map, filter, visitedRelations);
          break;
        default:
          assert(false);
        }
      }
    }
  }
}

void Relation::visitRw(ElementProvider& map, ConstElementVisitor& filter, const bool recursive)
{
  QList<long> visitedRelations;
  _visitRw(map, filter, visitedRelations, recursive);
  assert(visitedRelations.size() == 0);
}

void Relation::_visitRw(ElementProvider& map, ConstElementVisitor& filter,
                        QList<long>& visitedRelations, const bool recursive) const
{
  if (visitedRelations.contains(getId()))
  {
    if (logWarnCount < Log::getWarnMessageLimit())
    {
      LOG_WARN("Invalid data. This relation contains a circular reference. " + toString());
    }
    else if (logWarnCount == Log::getWarnMessageLimit())
    {
      LOG_WARN(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
    }
    logWarnCount++;
    return;
  }

  AddToVisitedRelationsList addTo(visitedRelations, getId());

  filter.visit(std::dynamic_pointer_cast<const Relation>(map.getRelation(getId())));

  if (recursive)
  {
    const vector<RelationData::Entry>& members = getMembers();
    for (const auto& m : members)
    {
      if (map.containsElement(m.getElementId()))
      {
        switch(m.getElementId().getType().getEnum())
        {
        case ElementType::Node:
          map.getNode(m.getElementId().getId())->visitRw(map, filter);
          break;
        case ElementType::Way:
          map.getWay(m.getElementId().getId())->visitRw(map, filter);
          break;
        case ElementType::Relation:
          map.getRelation(m.getElementId().getId())->_visitRw(map, filter, visitedRelations);
          break;
        default:
          assert(false);
        }
      }
    }
  }
}

QList<ElementId> Relation::getAdjoiningMemberIds(const ElementId& memberId) const
{
  LOG_VART(getMembers());
  QList<ElementId> ids;
  const size_t memberIndex = indexOf(memberId);
  LOG_VART(memberIndex);
  if (static_cast<int>(memberIndex) != -1)
  {
    if (!isFirstMember(memberId))
    {
      const ElementId memberBeforeId = memberIdAt(memberIndex - 1);
      LOG_VART(memberBeforeId);
      if (memberBeforeId.getType() != ElementType::Unknown)
        ids.append(memberBeforeId);
    }
    if (!isLastMember(memberId))
    {
      const ElementId memberAfterId = memberIdAt(memberIndex + 1);
      LOG_VART(memberAfterId);
      if (memberAfterId.getType() != ElementType::Unknown)
        ids.append(memberAfterId);
    }
  }
  LOG_VART(ids);
  return ids;
}

}

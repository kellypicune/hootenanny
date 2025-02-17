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
 * @copyright Copyright (C) 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "NetworkMatchCreator.h"

// hoot
#include <hoot/core/conflate/matching/MatchType.h>
#include <hoot/core/conflate/matching/MatchThreshold.h>
#include <hoot/core/conflate/network/NetworkMatch.h>
#include <hoot/core/conflate/network/NetworkMatcher.h>
#include <hoot/core/conflate/network/OsmNetworkExtractor.h>
#include <hoot/core/criterion/ChainCriterion.h>
#include <hoot/core/criterion/HighwayCriterion.h>
#include <hoot/core/criterion/StatusCriterion.h>
#include <hoot/core/elements/MapProjector.h>
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/io/OsmJsonWriter.h>
#include <hoot/core/io/OsmMapWriterFactory.h>
#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/util/StringUtils.h>
#include <hoot/core/visitors/ConstElementVisitor.h>

// Standard
#include <fstream>

// tgs
#include <tgs/RandomForest/RandomForest.h>

// Qt
#include <QElapsedTimer>

using namespace std;
using namespace Tgs;

namespace hoot
{

HOOT_FACTORY_REGISTER(MatchCreator, NetworkMatchCreator)

NetworkMatchCreator::NetworkMatchCreator()
  : _userCriterion(std::make_shared<HighwayCriterion>()),
    _matchScoringFunctionMax(ConfigOptions().getNetworkMatchScoringFunctionMax()),
    _matchScoringFunctionCurveMidpointX(ConfigOptions().getNetworkMatchScoringFunctionCurveMidX()),
    _matchScoringFunctionCurveSteepness(ConfigOptions().getNetworkMatchScoringFunctionCurveSteepness())
{
}

MatchPtr NetworkMatchCreator::createMatch(const ConstOsmMapPtr& /*map*/, ElementId /*eid1*/, ElementId /*eid2*/)
{
  return MatchPtr();
}

ConstMatchPtr NetworkMatchCreator::_createMatch(const NetworkDetailsPtr& map, NetworkEdgeScorePtr e,
                                                ConstMatchThresholdPtr mt) const
{
  return
    std::make_shared<const NetworkMatch>(
      map, e->getEdgeMatch(), e->getScore(), mt, _matchScoringFunctionMax,
      _matchScoringFunctionCurveMidpointX, _matchScoringFunctionCurveSteepness);
}

void NetworkMatchCreator::createMatches(const ConstOsmMapPtr& map, std::vector<ConstMatchPtr>& matches,
                                        ConstMatchThresholdPtr threshold)
{
  QElapsedTimer timer;
  timer.start();

  // The parent does some initialization we need.
  MatchCreator::createMatches(map, matches, threshold);

  QString searchRadiusStr;
  const double searchRadius = ConfigOptions().getSearchRadiusHighway();
  if (searchRadius < 0)
    searchRadiusStr = "within a feature dependent search radius";
  else
    searchRadiusStr = "within a search radius of " + QString::number(searchRadius, 'g', 2) + " meters";
  LOG_INFO("Looking for matches with: " << className() << " " << searchRadiusStr << "...");
  LOG_VART(threshold);
  const int matchesSizeBefore = static_cast<int>(matches.size());

  // use another class to extract graph nodes and graph edges.
  OsmNetworkExtractor e1;
  std::shared_ptr<ChainCriterion> c1 =
    std::make_shared<ChainCriterion>(std::make_shared<StatusCriterion>(Status::Unknown1), _userCriterion);
  if (_filter)
    c1->addCriterion(_filter);

  e1.setCriterion(c1);
  OsmNetworkPtr n1 = e1.extractNetwork(map);
  LOG_TRACE("Extracted Network 1: " << n1->toString());

  OsmNetworkExtractor e2;
  std::shared_ptr<ChainCriterion> c2 =
    std::make_shared<ChainCriterion>(std::make_shared<StatusCriterion>(Status::Unknown2), _userCriterion);
  if (_filter)
    c2->addCriterion(_filter);

  e2.setCriterion(c2);
  OsmNetworkPtr n2 = e2.extractNetwork(map);
  LOG_TRACE("Extracted Network 2: " << n2->toString());

  LOG_INFO("Matching networks...");
  // call class to derive final graph node and graph edge matches
  NetworkMatcherPtr matcher = Factory::getInstance().constructObject<NetworkMatcher>(ConfigOptions().getNetworkMatcher());
  matcher->matchNetworks(map, n1, n2);

  NetworkDetailsPtr details = std::make_shared<NetworkDetails>(map, n1, n2);

  const size_t numIterations = ConfigOptions().getNetworkOptimizationIterations();
  if (numIterations < 1)
  {
    throw HootException(QString("Invalid value: %1 for setting %2")
                        .arg(QString::number(numIterations), ConfigOptions::getNetworkOptimizationIterationsKey()));
  }
  LOG_INFO("Optimizing network over " << numIterations << " iterations...");
  for (size_t i = 0; i < numIterations; ++i)
  {
    matcher->iterate();
    LOG_INFO("Optimization iteration: " << i + 1 << "/" << numIterations << " complete.");

    OsmMapWriterFactory::writeDebugMap(map, className(), "match-iteration-" + QString::number(i + 1));
  }

  matcher->finalize();

  OsmMapWriterFactory::writeDebugMap(map, className(), "match-after-final-iteration", matcher);

  LOG_DEBUG("Retrieving edge scores...");
  // Convert graph edge matches into NetworkMatch objects.
  QList<NetworkEdgeScorePtr> edgeMatch = matcher->getAllEdgeScores();

  for (const auto& em : qAsConst(edgeMatch))
  {
    LOG_VART(em->getUid());
    LOG_VART(em->getScore());
    LOG_VART(em->getEdgeMatch());

    // Note that here we want the whole network match threshold, not the individual match threshold.
    if (em->getScore() > matcher->getMatchThreshold())
    {
      LOG_VART(em->getEdgeMatch()->getUid());
      ConstMatchPtr match = _createMatch(details, em, threshold);
      LOG_VART(match);
      matches.push_back(match);
    }
  }
  const int matchesSizeAfter = static_cast<int>(matches.size());

  LOG_STATUS(
    "Found " << StringUtils::formatLargeNumber(matchesSizeAfter - matchesSizeBefore) <<
    " total highway matches in: " << StringUtils::millisecondsToDhms(timer.elapsed()) << ".");
}

vector<CreatorDescription> NetworkMatchCreator::getAllCreators() const
{
  vector<CreatorDescription> result;
  result.emplace_back(
    className(),
    "Generates matchers that match roads with the Network Algorithm",
    CreatorDescription::BaseFeatureType::Highway,
    false);
  return result;
}

bool NetworkMatchCreator::isMatchCandidate(ConstElementPtr element, const ConstOsmMapPtr& /*map*/)
{
  // The hoot:special tag is currently only used by roundabout processing to mark temporary
  // features.
  if (element->getTags().contains(MetadataTags::HootSpecial()) || (_filter && !_filter->isSatisfied(element)))
    return false;

  return _userCriterion->isSatisfied(element);
}

std::shared_ptr<MatchThreshold> NetworkMatchCreator::getMatchThreshold()
{
  if (!_matchThreshold.get())
  {
    _matchThreshold =
      std::make_shared<MatchThreshold>(
        ConfigOptions().getNetworkMatchThreshold(), ConfigOptions().getNetworkMissThreshold(),
        ConfigOptions().getNetworkReviewThreshold());
  }
  return _matchThreshold;
}

QStringList NetworkMatchCreator::getCriteria() const
{
  return QStringList(HighwayCriterion::className());
}

}

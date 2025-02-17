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
 * @copyright Copyright (C) 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/elements/OsmMap.h>
#include "../TestUtils.h"
#include <hoot/core/io/OsmXmlReader.h>
#include <hoot/core/ops/CalculateStatsOp.h>
#include <hoot/core/util/ConfigOptions.h>

// Qt
#include <qnumeric.h>

namespace hoot
{

/*
 * Formerly this class tried to test every statistic calculated, but maintenance was a nightmare,
 * especially as new conflatable feature types were added. There is good enough coverage for stats
 * in the command tests, so those tests were removed. runStatsNumTest is currently the only test
 * left, and arguably it could be removed as well.
 */
class CalculateStatsOpTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(CalculateStatsOpTest);
  CPPUNIT_TEST(runStatsNumTest);
  CPPUNIT_TEST_SUITE_END();

public:


  CalculateStatsOpTest()
    : HootTestFixture("test-files/ops/CalculateStatsOp/",
                      UNUSED_PATH)
  {
    setResetType(ResetAll);
  }

  void setUp() override
  {
    HootTestFixture::setUp();
    conf().set(ConfigOptions::getStatsTranslateScriptKey(), "${HOOT_HOME}/translations/HootTest.js");
  }

  void runStatsNumTest()
  {
    std::shared_ptr<CalculateStatsOp> calcStatsOp = _calcStats(_inputPath + "all-data-types.osm");

    // This is here to prevent inadvertent removal of stats and to make sure any new stats get added
    // to this test.
    CPPUNIT_ASSERT_EQUAL(289, calcStatsOp->getStats().size());

    // This lets you know if the total stat calls made don't match what was predicted by
    // _initStatCalc.
    // TODO: These don't quite match up yet...off by one...not sure where, though.
    LOG_VART(calcStatsOp->_currentStatCalcIndex);
    LOG_VART(calcStatsOp->_totalStatCalcs);
    //CPPUNIT_ASSERT(calcStatsOp->_currentStatCalcIndex == calcStatsOp->_totalStatCalcs);
  }

private:

  std::shared_ptr<CalculateStatsOp> _calcStats(const QString& inputFile)
  {
    OsmXmlReader reader;
    OsmMapPtr map = std::make_shared<OsmMap>();
    reader.setDefaultStatus(Status::Unknown1);
    reader.setUseFileStatus(true);
    reader.setUseDataSourceIds(true);
    reader.read(inputFile, map);

    std::shared_ptr<CalculateStatsOp> calcStatsOp = std::make_shared<CalculateStatsOp>();
    // If we figure out the error messages logged by the script translator related stats are
    // invalid and fix them, then this log disablement can be removed.
    {
      DisableLog dl(Log::Fatal);
      calcStatsOp->apply(map);
    }
    //calcStatsOp->printStats();
    return calcStatsOp;
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(CalculateStatsOpTest, "slow");

}

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
 * @copyright Copyright (C) 2015, 2017, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#include "OsmSchemaLoaderFactory.h"

// hoot
#include <hoot/core/schema/OsmSchemaLoader.h>
#include <hoot/core/util/Factory.h>

using namespace std;

namespace hoot
{

OsmSchemaLoaderFactory& OsmSchemaLoaderFactory::getInstance()
{
  //  Local static singleton instance
  static OsmSchemaLoaderFactory instance;
  return instance;
}

std::shared_ptr<OsmSchemaLoader> OsmSchemaLoaderFactory::createLoader(QString url) const
{
  vector<QString> names = Factory::getInstance().getObjectNamesByBase(OsmSchemaLoader::className());
  for (const auto& n : names)
  {
    std::shared_ptr<OsmSchemaLoader> l = Factory::getInstance().constructObject<OsmSchemaLoader>(n);
    if (l->isSupported(url))
      return l;
  }

  LOG_DEBUG("Available loaders: " << names);
  throw HootException("Unable to load specified URL (wrong extension?): " + url);
}

}

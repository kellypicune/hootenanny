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
 * @copyright Copyright (C) 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#include "AddressNormalizer.h"

// hoot
#include <hoot/core/conflate/address/Address.h>
#include <hoot/core/conflate/address/LibPostalInit.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/StringUtils.h>

// libpostal
#include <libpostal/libpostal.h>

namespace hoot
{

AddressNormalizer::AddressNormalizer()
  : _numNormalized(0),
    _addressTagKeys(std::make_shared<AddressTagKeys>())
{
}

void AddressNormalizer::normalizeAddresses(const ElementPtr& e) const
{
  const QSet<QString> addressTagKeys = _addressTagKeys->getAddressTagKeys(*e);
  LOG_VART(addressTagKeys);
  for (const auto& addressTagKey : qAsConst(addressTagKeys))
  {
    // normalization may find multiple addresses; the first field should contain the "most correct" address, use
    // that as the address field and put the rest in an alt field
    const QVector<QString> normalizedAddresses = normalizeAddress(e->getTags().get(addressTagKey));
    bool firstAddressParsed = false;
    QStringList altAddresses;
    for (const auto& normalizedAddress : qAsConst(normalizedAddresses))
    {
      if (!firstAddressParsed)
      {
        e->getTags().set(addressTagKey, normalizedAddress);
        LOG_TRACE("Set normalized address: " << normalizedAddress << " for tag key: " << addressTagKey);
        firstAddressParsed = true;
      }
      else
        altAddresses.append(normalizedAddress);
    }
    if (!altAddresses.isEmpty())
    {
      e->getTags().set("alt_address", altAddresses.join(";"));
      LOG_TRACE("Set alt normalized address(es): " << altAddresses << " for tag key: " << addressTagKey);
    }
  }
}

QVector<QString> AddressNormalizer::normalizeAddress(const QString& address) const
{
  if (!ConfigOptions().getAddressMatchEnabled())
  {
    QVector<QString> addresses;
    addresses.append(address);
    return addresses;
  }

  const QString addressToNormalize = address.trimmed().simplified();
  if (!Address::isStreetIntersectionAddress(addressToNormalize))
    return _normalizeAddressWithLibPostal(addressToNormalize);
  else // libpostal doesn't handle intersections very well
    return _normalizeAddressIntersection(addressToNormalize);
}

QVector<QString> AddressNormalizer::_normalizeAddressWithLibPostal(const QString& address) const
{
  if (address.trimmed().isEmpty())
    return QVector<QString>();

  LOG_TRACE("Normalizing " << address << " with libpostal...");

  QVector<QString> normalizedAddresses;
  QSet<QString> addressSet;
  QString addressCopy = address;

  // See note about init of this in AddressParser::parseAddresses.
  LibPostalInit::getInstance();

  _prepareAddressForLibPostalNormalization(addressCopy);

  size_t num_expansions;
  // specifying a language in the options is optional, but could we get better performance if
  // we did specify one when we know what it is (would have to check to see if it was supported
  // first, of course)?
  libpostal_normalize_options_t options = libpostal_get_default_options();
  char** expansions = libpostal_expand_address(addressCopy.toUtf8().data(), options, &num_expansions);
  // add all the normalizations libpostal finds as possible addresses
  for (size_t i = 0; i < num_expansions; i++)
  {
    const QString normalizedAddress = QString::fromUtf8(expansions[i]);
    LOG_VART(normalizedAddress);
    if (_isValidNormalizedAddress(addressCopy, normalizedAddress) && !addressSet.contains(normalizedAddress))
    {
      normalizedAddresses.append(normalizedAddress);
      addressSet.insert(normalizedAddress);
      LOG_TRACE("Normalized address from: " << address << " to: " << normalizedAddress);
      _numNormalized++;
    }
    else
    {
      LOG_TRACE("Skipping normalized address: " << normalizedAddress << "...");
    }
  }
  libpostal_expansion_array_destroy(expansions, num_expansions);

  return normalizedAddresses;
}

QVector<QString> AddressNormalizer::_normalizeAddressIntersection(const QString& address) const
{
  LOG_TRACE("Normalizing intersection: " << address << "...");

  const QMap<QString, QString> streetTypeAbbreviationsToFullTypes = Address::getStreetTypeAbbreviationsToFullTypes();
  const QStringList addressParts = StringUtils::splitOnAny(address, Address::getIntersectionSplitTokens(), 2);
  LOG_VART(addressParts.size());

  if (addressParts.size() != 2)
    throw IllegalArgumentException("A non-intersection address was passed into street intersection address normalization.");

  // replace all street type abbreviations in both intersection parts in the address with their full
  // name counterparts

  QString modifiedAddress;
  for (int i = 0; i < addressParts.size(); i++)
  {
    QString addressPart = addressParts.at(i).trimmed();
    LOG_VART(addressPart);
    for (auto itr = streetTypeAbbreviationsToFullTypes.begin(); itr != streetTypeAbbreviationsToFullTypes.end(); ++itr)
    {
      const QString abbrev = itr.key().trimmed();
      LOG_VART(abbrev);
      const QString fullType = itr.value().trimmed();
      LOG_VART(fullType);

      LOG_VART(addressPart.endsWith(abbrev, Qt::CaseInsensitive));
      if (addressPart.endsWith(abbrev, Qt::CaseInsensitive))
      {
        StringUtils::replaceLastIndexOf(addressPart, abbrev, fullType);
        LOG_VART(addressPart);
      }
    }
    LOG_VART(addressPart);

    modifiedAddress += addressPart.trimmed();
    if (i == 0)
      modifiedAddress += " and ";
  }
  LOG_VART(modifiedAddress);

  // If one of the intersection parts has a street type and the other doesn't we'll copy one street
  // type over to the other. This isn't foolproof as we could end up giving one of the intersections
  // an incorrect street type (the actual element's address tags never get modified, though).
  // However, it does help with address matching and will remain in place unless its found to be
  // causing harm in some way.

  QStringList modifiedAddressParts = StringUtils::splitOnAny(modifiedAddress, Address::getIntersectionSplitTokens(), 2);
  assert(modifiedAddressParts.size() == 2);
  LOG_VART(modifiedAddressParts);
  QString firstIntersectionPart = modifiedAddressParts[0].trimmed();
  LOG_VART(firstIntersectionPart);
  QString secondIntersectionPart = modifiedAddressParts[1].trimmed();
  LOG_VART(secondIntersectionPart);

  QStringList streetFullTypes;
  QStringList streetPluralTypes;
  const QStringList streetFullTypesTemp = Address::getStreetFullTypesToTypeAbbreviations().keys();
  streetFullTypes = streetFullTypesTemp;

  // Sometimes intersections have plural street types (suffixes). We want to be able to handle those
  // as well, but we don't want them plural in the final normalized address.
  for (const auto& fullType : qAsConst(streetFullTypesTemp))
  {
    const QString pluralType = fullType + "s";
    if (!streetFullTypes.contains(pluralType))
      streetFullTypes.append(pluralType);
    streetPluralTypes.append(pluralType);
  }
  LOG_VART(streetPluralTypes);

  // remove any plural suffixes found; we may not need all of this here...
  if (StringUtils::endsWithAny(firstIntersectionPart.trimmed(), streetPluralTypes))
  {
    firstIntersectionPart.chop(1);
    LOG_VART(firstIntersectionPart);
  }
  if (StringUtils::endsWithAny(secondIntersectionPart.trimmed(), streetPluralTypes))
  {
    secondIntersectionPart.chop(1);
    LOG_VART(secondIntersectionPart);
  }
  QStringList modifiedAddressPartsTemp;
  for (auto modifiedAddressPart : qAsConst(modifiedAddressParts))
  {
    if (StringUtils::endsWithAny(modifiedAddressPart.trimmed(), streetPluralTypes))
    {
      modifiedAddressPart.chop(1);
      LOG_VART(modifiedAddressPart);
    }
    modifiedAddressPartsTemp.append(modifiedAddressPart);
  }
  modifiedAddressParts = modifiedAddressPartsTemp;
  QString firstIntersectionEndingStreetType = StringUtils::endsWithAnyAsStr(firstIntersectionPart.trimmed(), streetFullTypes).trimmed();
  if (firstIntersectionEndingStreetType.endsWith('s'))
    firstIntersectionEndingStreetType.chop(1);
  LOG_VART(firstIntersectionEndingStreetType);
  QString secondIntersectionEndingStreetType = StringUtils::endsWithAnyAsStr(secondIntersectionPart.trimmed(), streetFullTypes).trimmed();
  if (secondIntersectionEndingStreetType.endsWith('s'))
    secondIntersectionEndingStreetType.chop(1);
  LOG_VART(secondIntersectionEndingStreetType);

  if (!firstIntersectionEndingStreetType.isEmpty() && secondIntersectionEndingStreetType.isEmpty())
  {
    LOG_VART(modifiedAddressParts[1]);
    modifiedAddressParts[1] = modifiedAddressParts[1].trimmed() + " " + firstIntersectionEndingStreetType.trimmed();
    LOG_VART(modifiedAddressParts[1]);
  }
  else if (firstIntersectionEndingStreetType.isEmpty() && !secondIntersectionEndingStreetType.isEmpty())
  {
    LOG_VART(modifiedAddressParts[0]);
    modifiedAddressParts[0] = modifiedAddressParts[0].trimmed() + " " + secondIntersectionEndingStreetType.trimmed();
    LOG_VART(modifiedAddressParts[0]);
  }
  modifiedAddress = modifiedAddressParts[0].trimmed() + " and " + modifiedAddressParts[1].trimmed();
  LOG_VART(modifiedAddress);

  // go ahead and send it to libpostal to finish out the normalization to avoid duplicating some
  // code, although it probably won't change any from this point
  return _normalizeAddressWithLibPostal(modifiedAddress);
}

void AddressNormalizer::_prepareAddressForLibPostalNormalization(QString& address)
{
  LOG_TRACE("Before normalization fix: " << address);
  LOG_VART(Address::isStreetIntersectionAddress(address));
  // This is a nasty thing libpostal does where it changes "St" to "Saint" when it should be
  // "Street".
  if (address.endsWith("st", Qt::CaseInsensitive) && !Address::isStreetIntersectionAddress(address))
    StringUtils::replaceLastIndexOf(address, "st", "Street");
  LOG_TRACE("After normalization fix: " << address);
}

bool AddressNormalizer::_isValidNormalizedAddress(const QString& inputAddress,
                                                  const QString& normalizedAddress)
{
  // force normalization of "&" to "and"
  if (normalizedAddress.contains(" & "))
    return false;
  // This is a bit of hack, but I don't like the way libpostal is turning "St" or "Street" into
  // "Saint". Should probably look into configuration of libpostal or update it for a possible fix
  // instead.
  else if (normalizedAddress.endsWith("saint", Qt::CaseInsensitive) &&
           (inputAddress.endsWith("street", Qt::CaseInsensitive) ||
            inputAddress.endsWith("st", Qt::CaseInsensitive)))
    return false;
  else
    return true;
}

}

#pragma once

#include "3drando/text.hpp"
#include "3drando/hints.hpp"
#include "../custom-message/CustomMessageManager.h"
#include "randomizerTypes.h"
#include <vector>
#include "nlohmann/json.hpp"
using oJson = nlohmann::ordered_json;

namespace Rando {
class Hint {
  public:
    Hint();
    Hint(RandomizerHint ownKey_, HintType hintType_, std::string distributionName_,
         std::vector<RandomizerHintTextKey> hintKeys_, std::vector<RandomizerCheck> locations_,
         std::vector<RandomizerArea> areas_ = {}, std::vector<TrialKey> trials_ = {});
    Hint(RandomizerHint ownKey_, HintType hintType_, std::vector<RandomizerHintTextKey> hintKeys_,
         std::vector<RandomizerCheck> locations_ = {}, std::vector<RandomizerArea> areas_ = {},
         std::vector<TrialKey> trials_ = {}, bool yourPocket_ = false, int num_ = 0);
    Hint(RandomizerHint ownKey_, std::vector<CustomMessage> messages_);
    Hint(RandomizerHint ownKey_, nlohmann::json json_);
    void FillGapsInData();
    void SetLocationsAsHinted() const;
    void NamesChosen();
    size_t GetNumberOfMessages() const;
    const std::vector<std::string> GetAllMessageStrings(MessageFormat format = MF_AUTO_FORMAT) const;
    const CustomMessage GetHintMessage(MessageFormat format = MF_AUTO_FORMAT, size_t id = 0) const;
    const HintText GetHintText(size_t id = 0) const;
    oJson toJSON();
    void logHint(oJson& jsonData);
    const HintText GetItemHintText(uint8_t slot, bool mysterious = false) const;
    const HintText GetAreaHintText(uint8_t slot) const;
    const CustomMessage GetItemName(uint8_t slot, bool mysterious = false) const;
    const CustomMessage GetAreaName(uint8_t slot) const;
    static CustomMessage GetBridgeReqsText();
    static CustomMessage GetGanonBossKeyText();
    void AddHintedLocation(RandomizerCheck location);
    std::vector<RandomizerCheck> GetHintedLocations() const;
    void SetHintType(HintType type);
    HintType GetHintType() const;
    void AddHintedArea(RandomizerArea area);
    std::vector<RandomizerArea> GetHintedAreas() const;
    void SetDistribution(std::string distribution);
    const std::string& GetDistribution() const;
    bool IsEnabled() const;
    std::vector<RandomizerHintTextKey> GetHintTextKeys() const;
    std::vector<RandomizerGet> GetHintedItems() const;
    std::vector<uint8_t> GetItemNamesChosen() const;
    std::vector<uint8_t> GetHintTextsChosen() const;
    std::vector<uint8_t> GetAreaTextsChosen() const;
    std::vector<TrialKey> GetHintedTrials() const;
    int GetNum();
    void ResetVariables();
#ifdef RSBS_SINGLE_EXECUTABLE
    // RedShipBlueShip (#575 H3a): the Majora's Mask area a slot names. Parallel to
    // `areas`: an MM area of src/common's shared vocabulary (combo_area.h) for a
    // slot whose area is RA_TERMINA, else 0. Empty when no slot names Termina.
    void SetForeignAreas(std::vector<uint8_t> foreignAreas_);
    std::vector<uint8_t> GetForeignAreas() const;
#endif

  private:
    RandomizerHint ownKey = RH_NONE;
    HintType hintType = HINT_TYPE_HINT_KEY;
    std::string distribution = "";
    std::vector<RandomizerHintTextKey> hintKeys = {};
    std::vector<RandomizerCheck> locations = {};
    std::vector<RandomizerArea> areas = {};
    std::vector<TrialKey> trials = {};
    bool yourPocket = false;
    int num = 0;
    std::vector<CustomMessage> messages = {};
    std::vector<RandomizerGet> items = {};
    bool enabled = false;
    std::vector<uint8_t> itemNamesChosen = {};
    std::vector<uint8_t> hintTextsChosen = {};
    std::vector<uint8_t> areaNamesChosen = {};
#ifdef RSBS_SINGLE_EXECUTABLE
    std::vector<uint8_t> foreignAreas = {};
#endif
};
} // namespace Rando
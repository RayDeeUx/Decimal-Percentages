#include <cvolton.level-id-api/include/EditorIDs.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/GJGameLevel.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelPage.hpp>
#include <regex>

#define PREFERRED_HOOK_PRIO (-3999) // because for some incredible reason QOLMod changes a level's levelType value for a split second and now this hook prio's here to work around that
#define MARIOMASTR "mariomastr.progress_of_editor_levels"
#define DEATHSCREENTWEAKS "raydeeux.deathscreentweaks"

static const std::regex percentageRegex(R"(^(?:(?:\d+(?:\.\d+)?%)([^\n\d]*))+(\d+(?:\.\d+)?%)$)", std::regex::optimize | std::regex::icase);
// see https://regex101.com/r/jlTQrI/2 for context, and https://regex101.com/r/poPUOK/1 for the better version
static const std::regex trailingZeroesRegex(R"((.*[^0])(0+)$)", std::regex::optimize | std::regex::icase);
// see https://regex101.com/r/j5IVLk/2 for context.

using namespace geode::prelude;

geode::Mod* dst = nullptr; // deathscreentweaks
bool calledAlready = false;
bool marioMastrMod = false;

bool getBool(const std::string_view& key) {
	return Mod::get()->getSettingValue<bool>(key);
}

int64_t getDecimalPlaces(const bool qualifiedForInsaneMode = true) {
	auto decimalPlaces = Mod::get()->getSettingValue<int64_t>("decimalPlaces");
	if (!qualifiedForInsaneMode && decimalPlaces > 3 && !getBool("insaneMode")) decimalPlaces = 3;
	return decimalPlaces;
}

float getPercentageForLevel(GJGameLevel* level, const bool practice = false) {
	if (level->m_normalPercent > 99 && !practice || level->m_practicePercent > 99 && practice) return 100.f;
	std::string str;
	if (level->m_levelType == GJLevelType::Editor) {
		str = fmt::format("percentage_{}_local_{}", practice ? "practice" : "normal", EditorIDs::getID(level));
	} else if (level->m_gauntletLevel) {
		str = fmt::format("percentage_{}_gauntlet_{}", practice ? "practice" : "normal", level->m_levelID.value());
	} else if (level->m_dailyID.value() == 0) {
		str = fmt::format("percentage_{}_{}", practice ? "practice" : "normal", level->m_levelID.value());
	} else {
		str = fmt::format("percentage_{}_{}_periodic_{}", practice ? "practice" : "normal", level->m_levelID.value(), level->m_dailyID.value());
	}
	if (!Mod::get()->hasSavedValue(str)) {
		Mod::get()->setSavedValue<float>(str, practice ? static_cast<float>(level->m_practicePercent) : static_cast<float>(level->m_normalPercent.value()));
	}
	return Mod::get()->getSavedValue<float>(str, practice ? static_cast<float>(level->m_practicePercent) : static_cast<float>(level->m_normalPercent.value()));
}

std::string roundPercentage(const float percentage, const bool qualifiedForInsaneMode = true) {
	if (percentage >= 100.f && getBool("ignoreHundredPercent")) return "100";
	auto roundedPercent = numToString<float>(percentage, getDecimalPlaces(qualifiedForInsaneMode));
	if (!getBool("noTrailingZeros")) return roundedPercent;
	if (roundedPercent.find('.') == std::string::npos) return roundedPercent; // if percentage (after accuracy is applied) does not have a decimal point, abort!
	std::smatch match;
	if (!std::regex_match(roundedPercent, match, trailingZeroesRegex)) return roundedPercent;
	if (match.empty() || match.size() > 3 || match[1].str().empty() || match[2].str().empty()) return roundedPercent;
	roundedPercent = match[1].str();
	if (roundedPercent.ends_with('.')) roundedPercent.pop_back(); // if, after removing trailing zeroes, string ends with decimal point separtor, remove decimal point separator
	return roundedPercent;
}

std::string decimalPercentAsString(GJGameLevel *level, const bool practice = false, const bool qualifiedForInsaneMode = true) {
	return fmt::format("{}%", roundPercentage(getPercentageForLevel(level, practice), qualifiedForInsaneMode));
}

CCLabelBMFont* getLabelByID(CCNode* parent, const std::string& nodeID) {
	const auto node = typeinfo_cast<CCLabelBMFont*>(parent->getChildByIDRecursive(nodeID));
	if (!node) return nullptr;
	return node;
}

void savePercent(GJGameLevel* level, const float percent, const bool practice) {
	if (level->isPlatformer()) return;
	std::string str;
	if (level->m_levelType == GJLevelType::Editor) {
		str = fmt::format("percentage_{}_local_{}", practice ? "practice" : "normal", EditorIDs::getID(level));
	} else if (level->m_gauntletLevel) {
		str = fmt::format("percentage_{}_gauntlet_{}", practice ? "practice" : "normal", level->m_levelID.value());
	} else if (level->m_dailyID.value() == 0) {
		str = fmt::format("percentage_{}_{}", practice ? "practice" : "normal", level->m_levelID.value());
	} else {
		str = fmt::format("percentage_{}_{}_periodic_{}", practice ? "practice" : "normal", level->m_levelID.value(), level->m_dailyID.value());
	}
	Mod::get()->setSavedValue<float>(str, percent);
}

class $modify(MyMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		if (calledAlready) return true;
		calledAlready = true;
		if (Loader::get()->isModLoaded(MARIOMASTR)) marioMastrMod = true;
		dst = Loader::get()->getLoadedMod(DEATHSCREENTWEAKS);
		return true;
	}
};

class $modify(MyGJGameLevel, GJGameLevel) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("GJGameLevel::savePercentage", PREFERRED_HOOK_PRIO);
	}
	void savePercentage(int percent, bool isPracticeMode, int clicks, int attempts, bool isChkValid) {
		GJGameLevel::savePercentage(percent, isPracticeMode, clicks, attempts, isChkValid);
		if (this->isPlatformer()) return;
		const auto pl = PlayLayer::get();
		if (getBool("logging")) log::info("=== level ID vs daily/weekly/event ID debug info ===\ndaily/weekly ID: {}\nlevel ID: {}\nis gauntlet:", m_dailyID.value(), m_levelID.value(), m_gauntletLevel);
		if (!pl) return savePercent(this, static_cast<float>(percent), isPracticeMode);
		if (pl->getCurrentPercent() > getPercentageForLevel(this, isPracticeMode)) return savePercent(this, PlayLayer::get()->getCurrentPercent(), isPracticeMode);
	}
};

class $modify(MyLevelInfoLayer, LevelInfoLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("LevelInfoLayer::init", PREFERRED_HOOK_PRIO);
	}
	bool init(GJGameLevel* level, bool challenge) {
		if (!LevelInfoLayer::init(level, challenge)) return false;
		if (!getBool("enabled") || level->isPlatformer()) return true;
		if (CCLabelBMFont* normal = getLabelByID(this, "normal-mode-percentage")) {
			const std::string& dpAsString = decimalPercentAsString(level, false, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return true;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_normalPercent.value()) return true;
			normal->setString(dpAsString.c_str());
		}
		if (CCLabelBMFont* practice = getLabelByID(this, "practice-mode-percentage")) {
			const std::string& dpAsString = decimalPercentAsString(level, true, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return true;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_practicePercent) return true;
			practice->setString(dpAsString.c_str());
		}
		return true;
	}
};

class $modify(MyPauseLayer, PauseLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("PauseLayer::customSetup", PREFERRED_HOOK_PRIO);
	}
	virtual void customSetup() {
		PauseLayer::customSetup();
		if (!getBool("enabled")) return;
		auto level = PlayLayer::get()->m_level;
		if (!level || level->isPlatformer()) return;
		if (CCLabelBMFont* normal = getLabelByID(this, "normal-progress-label")) {
			if (std::string(normal->getString()).starts_with("100") && getBool("ignoreHundredPercent")) return;
			std::string dpAsString = decimalPercentAsString(level, false, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_normalPercent.value()) return;
			normal->setString(dpAsString.c_str());
		}
		if (CCLabelBMFont* practice = getLabelByID(this, "practice-progress-label")) {
			if (std::string(practice->getString()).starts_with("100") && getBool("ignoreHundredPercent")) return;
			std::string dpAsString = decimalPercentAsString(level, true, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_practicePercent) return;
			practice->setString(dpAsString.c_str());
		}
	}
};

class $modify(MyLevelCell, LevelCell) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("LevelCell::loadFromLevel", PREFERRED_HOOK_PRIO);
	}
	void applyDecimalPercentage(GJGameLevel *level) {
		if (!level || level->isPlatformer()) return;
		CCLabelBMFont* percent = getLabelByID(this, "percentage-label");
		if (!percent) return;
		const std::string& dpAsString = decimalPercentAsString(level, false, false);
		std::smatch match;
		if (dpAsString == "0%" || (utils::string::startsWith(dpAsString, "0.") && std::regex_match(dpAsString, match, trailingZeroesRegex) && getBool("noTrailingZeros"))) return;
		std::string dpNoPercent = dpAsString;
		dpNoPercent.pop_back();
		const auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
		if (dpAsFloat.isErr()) return;
		if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_normalPercent.value()) {
			auto percentString = static_cast<std::string>(percent->getString());
			if (!percentString.ends_with('%') && percentString.length() < 2) return;
			percentString.pop_back();
			if (percentString.empty()) return;
			const std::vector<std::string> splitVector = utils::string::split(dpAsString, ".");
			if (splitVector.empty()) return;
			if (splitVector.at(0) != percentString) return;
		}
		percent->setString(dpAsString.c_str());
	}
	void loadFromLevel(GJGameLevel* level) {
		LevelCell::loadFromLevel(level);
		if (!getBool("enabled") || getBool("ignoreLevelCell")) return;
		MyLevelCell::applyDecimalPercentage(level);
	}
};

class $modify(MyLevelPage, LevelPage) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("LevelPage::updateDynamicPage", PREFERRED_HOOK_PRIO);
	}
	void updateDynamicPage(GJGameLevel* level) {
		LevelPage::updateDynamicPage(level);
		if (!getBool("enabled")) return;
		if (!level || level->isPlatformer()) return;
		if (CCLabelBMFont* normal = getLabelByID(this, "normal-progress-label")) {
			if (static_cast<std::string>(normal->getString()).starts_with("100") && getBool("ignoreHundredPercent")) return;
			const std::string& dpAsString = decimalPercentAsString(level, false, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_normalPercent.value()) return;
			normal->setString(dpAsString.c_str());
		}
		if (CCLabelBMFont* practice = getLabelByID(this, "practice-progress-label")) {
			if (static_cast<std::string>(practice->getString()).starts_with("100") && getBool("ignoreHundredPercent")) return;
			const std::string& dpAsString = decimalPercentAsString(level, true, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_practicePercent) return;
			practice->setString(dpAsString.c_str());
		}
	}
};

class $modify(MyPlayLayer, PlayLayer) {
	static void onModify(auto& self) {
		(void) self.setHookPriority("PlayLayer::updateProgressbar", PREFERRED_HOOK_PRIO);
		// DO NOT SET A HOOK PRIO FOR PlayLayer::showNewBest
	}
	std::string formatCurrentPercentInPlayLayer() {
		return fmt::format("{:.{}f}%", this->getCurrentPercent(), getDecimalPlaces());
	}
	void showNewBest(bool p0, int p1, int p2, bool p3, bool p4, bool p5) {
		PlayLayer::showNewBest(p0, p1, p2, p3, p4, p5);
		if (!getBool("enabled") || !m_level || m_level->isPlatformer()) return;
		if (dst) {
			if (dst->getSettingValue<bool>("enabled") && dst->getSettingValue<bool>("accuratePercent")) return;
		}
		Loader::get()->queueInMainThread([this]{
			for (CCNode* child : CCArrayExt<CCNode*>(this->getChildren())) {
				if (child->getZOrder() != 100) continue;
				for (CCNode* grandchild : CCArrayExt<CCNode*>(child->getChildren())) {
					const auto label = typeinfo_cast<CCLabelBMFont*>(grandchild);
					if (!label) continue;
					const auto& labelString = static_cast<std::string>(label->getString());
					if (!labelString.ends_with('%')) continue;
					if (getBool("logging")) log::info("\ndst: {}\nlabel->getString(): {}\nlabel->getString().ends_with('%'): {}", dst, labelString, labelString.ends_with('%'));
					if (!dst) return label->setString(formatCurrentPercentInPlayLayer().c_str());
					if (!dst->getSettingValue<bool>("enabled")) return label->setString(fmt::format("{}%", roundPercentage(getPercentageForLevel(m_level, false))).c_str());
					return label->setString(formatCurrentPercentInPlayLayer().c_str());
				}
			}
		});
	}
	void updateProgressbar() {
		PlayLayer::updateProgressbar();
		if (!getBool("enabled") || getBool("ignorePercentageLabel") || !m_level || m_level->isPlatformer() || !m_percentageLabel) return;
		const std::string& percentLabelText = m_percentageLabel->getString();
		std::smatch match;
		if (!std::regex_match(percentLabelText, match, percentageRegex)) return;
		std::string newBestSeparator = match[1].str();
		std::string possiblyNewBest = match[2].str();
		if (match.empty() || match.size() > 3 || newBestSeparator.empty() || possiblyNewBest.empty() || !possiblyNewBest.ends_with("%")) return;
		if (getBool("logging")) log::info("=== PERCENTAGE LABEL DEBUG INFO ===\nmatch[1].str() [newBestSeparator]: {}\nmatch[2].str() [possiblyNewBest]: {}", newBestSeparator, possiblyNewBest);
		std::string newBestWithoutPercent = possiblyNewBest;
		newBestWithoutPercent.pop_back();
		auto numFromString = utils::numFromString<int64_t>(newBestWithoutPercent);
		if (numFromString.isErr()) return;
		if (numFromString.unwrap() != m_level->m_normalPercent && !m_isPracticeMode) return;
		if (numFromString.unwrap() != m_level->m_practicePercent && m_isPracticeMode && !m_isTestMode) return;
		std::string newLabelText = std::regex_replace(percentLabelText, std::regex(fmt::format("{}{}", newBestSeparator, possiblyNewBest)), fmt::format("{}{}", newBestSeparator, decimalPercentAsString(m_level, m_isPracticeMode)));
		m_percentageLabel->setString(newLabelText.c_str());
	}
};

class $modify(MyEditLevelLayer, EditLevelLayer) {
	static void onModify(auto& self) {
		if (marioMastrMod) (void) self.setHookPriorityAfterPost("EditLevelLayer::init", MARIOMASTR);
		else (void) self.setHookPriority("EditLevelLayer::init", PREFERRED_HOOK_PRIO);
	}
	bool init(GJGameLevel* level) {
		if (!EditLevelLayer::init(level)) return false;
		if (!marioMastrMod || getBool("ignoreEditLevelLayer")) return true;
		if (CCLabelBMFont* normal = getLabelByID(this, "normal-mode-percentage"); normal && !(static_cast<std::string>(normal->getString()).starts_with("100") && getBool("ignoreHundredPercent"))) {
			const std::string& dpAsString = decimalPercentAsString(level, false, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			const auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isOk() && static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) == level->m_normalPercent.value()) normal->setString(dpAsString.c_str());
		}
		if (CCLabelBMFont* practice = getLabelByID(this, "practice-mode-percentage"); practice && !(static_cast<std::string>(practice->getString()).starts_with("100") && getBool("ignoreHundredPercent"))) {
			const std::string& dpAsString = decimalPercentAsString(level, true, true);
			std::string dpNoPercent = dpAsString;
			dpNoPercent.pop_back();
			const auto dpAsFloat = utils::numFromString<float>(dpNoPercent);
			if (dpAsFloat.isErr()) return true;
			if (static_cast<int64_t>(dpAsFloat.unwrapOr(0.f)) != level->m_practicePercent) return true;
			practice->setString(dpAsString.c_str());
		}
		return true;
	}
};
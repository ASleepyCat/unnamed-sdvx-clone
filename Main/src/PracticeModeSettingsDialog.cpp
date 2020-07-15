#include "stdafx.h"
#include "PracticeModeSettingsDialog.hpp"

PracticeModeSettingsDialog::PracticeModeSettingsDialog(MapTime endTime, MapTime& lastMapTime, Game::PlayOptions& playOptions, MapTimeRange& range)
    : m_endTime(endTime), m_lastMapTime(lastMapTime), m_playOptions(playOptions), m_range(range)
{
}

void PracticeModeSettingsDialog::InitTabs()
{
    AddTab(std::move(m_CreatePlaybackTab()));
    AddTab(std::move(m_CreateFailConditionTab()));
    AddTab(std::move(m_CreateGameSettingTab()));

    SetCurrentTab(0);
}

const static Vector<float> SPEEDS = { 0.25f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
const static Vector<String> SPEEDS_STR = { "0.25", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0" };
static int GetSpeedInd(float f)
{
    if (f <= SPEEDS.front())
        return 0;
    if (f >= SPEEDS.back())
        return static_cast<int>(SPEEDS.size()) - 1;

    int minInd = 0;
    for (int i = 1; i < SPEEDS.size(); ++i)
    {
        if (std::abs(f - SPEEDS[i]) < std::abs(f - SPEEDS[minInd]))
            minInd = i;
    }

    return minInd;
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreatePlaybackTab()
{
    Tab playbackTab = std::make_unique<TabData>();
    playbackTab->name = "Playback";

    Setting loopBeginSetting = CreateIntSetting("Start point", m_range.begin, {0, m_endTime}, 50);
    loopBeginSetting->setter.AddLambda([this](const SettingData& data) { onSetMapTime.Call(data.intSetting.val); });
    playbackTab->settings.emplace_back(std::move(loopBeginSetting));

    Setting loopBeginButton = CreateButton("Set to here",
        std::move([this](const auto&) { m_range.begin = Math::Clamp(m_lastMapTime, 0, m_endTime); }));
    playbackTab->settings.emplace_back(std::move(loopBeginButton));

    Setting loopEndSetting = CreateIntSetting("End point", m_range.end, { 0, m_endTime }, 50);
    loopEndSetting->setter.AddLambda([this](const SettingData& data) { onSetMapTime.Call(data.intSetting.val); });
    playbackTab->settings.emplace_back(std::move(loopEndSetting));

    Setting loopEndClearButton = CreateButton("Clear",
        std::move([this](const auto&) { m_range.end = 0; }));
    playbackTab->settings.emplace_back(std::move(loopEndClearButton));

    Setting loopEndButton = CreateButton("Set to here",
        std::move([this](const auto&) { m_range.end = Math::Clamp(m_lastMapTime, 0, m_endTime); }));
    playbackTab->settings.emplace_back(std::move(loopEndButton));

    Setting loopOnSuccess = CreateBoolSetting("Loop on success", m_playOptions.loopOnSuccess);
    playbackTab->settings.emplace_back(std::move(loopOnSuccess));

    Setting loopOnFail = CreateBoolSetting("Loop on fail", m_playOptions.loopOnFail);
    playbackTab->settings.emplace_back(std::move(loopOnFail));

    assert(SPEEDS.size() == SPEEDS_STR.size());

    Setting speedSetting = std::make_unique<SettingData>("Playback speed", SettingType::Enum);
    speedSetting->enumSetting.options = SPEEDS_STR;
    speedSetting->enumSetting.val = GetSpeedInd(m_playOptions.playbackSpeed);
    speedSetting->setter.AddLambda([this](const SettingData& data) { onSpeedChange.Call(SPEEDS[data.enumSetting.val]); });
    speedSetting->getter.AddLambda([this](SettingData& data) { data.enumSetting.val = GetSpeedInd(m_playOptions.playbackSpeed); });
    playbackTab->settings.emplace_back(std::move(speedSetting));

    return playbackTab;
}

enum class GameFailConditionType
{
    None, Score, Grade, Miss, MissAndNear
};

const char* GAME_FAIL_CONDITION_TYPE_STR[5] = { "None", "Score", "Grade", "Miss", "Miss+Near" };

static inline GameFailConditionType GetGameFailConditionType(const GameFailCondition* failCondition)
{
    if (!failCondition) return GameFailConditionType::None;

    if (dynamic_cast<const GameFailCondition::Score*>(failCondition)) return GameFailConditionType::Score;
    if (dynamic_cast<const GameFailCondition::Grade*>(failCondition)) return GameFailConditionType::Grade;
    if (dynamic_cast<const GameFailCondition::MissCount*>(failCondition)) return GameFailConditionType::Miss;
    if (dynamic_cast<const GameFailCondition::MissAndNearCount*>(failCondition)) return GameFailConditionType::MissAndNear;

    return GameFailConditionType::None;
}

static inline std::unique_ptr<GameFailCondition> CreateGameFailCondition(GameFailConditionType type)
{
    switch (type)
    {
    case GameFailConditionType::Score: return std::make_unique<GameFailCondition::Score>(10000000);
    case GameFailConditionType::Grade: return std::make_unique<GameFailCondition::Grade>(GradeMark::PUC);
    case GameFailConditionType::Miss: return std::make_unique<GameFailCondition::MissCount>(0);
    case GameFailConditionType::MissAndNear: return std::make_unique<GameFailCondition::MissAndNearCount>(0);
    default: return nullptr;
    }
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateFailConditionTab()
{
    Tab conditionTab = std::make_unique<TabData>();
    conditionTab->name = "Failing";

    Setting conditionType = std::make_unique<SettingData>("Fail condition", SettingType::Enum);
    for (const char* str : GAME_FAIL_CONDITION_TYPE_STR)
        conditionType->enumSetting.options.Add(str);
    conditionType->enumSetting.val = static_cast<int>(GameFailConditionType::None);
    conditionType->getter.AddLambda([this](SettingData& data) {
        data.intSetting.val = static_cast<int>(GetGameFailConditionType(m_playOptions.failCondition.get()));
    });
    conditionType->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = CreateGameFailCondition(static_cast<GameFailConditionType>(data.enumSetting.val));
    });
    conditionTab->settings.emplace_back(std::move(conditionType));

    Setting scoreThreshold = std::make_unique<SettingData>("Score less than", SettingType::Integer);
    scoreThreshold->intSetting.min = 800 * 10000;
    scoreThreshold->intSetting.max = 1000 * 10000;
    scoreThreshold->intSetting.step = 10000;
    scoreThreshold->intSetting.val = 10000000;
    scoreThreshold->getter.AddLambda([this](SettingData& data) {
        if (auto* cond = dynamic_cast<GameFailCondition::Score*>(m_playOptions.failCondition.get()))
            data.intSetting.val = cond->GetMinAllowed();
    });
    scoreThreshold->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::Score>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(scoreThreshold));

    Setting gradeThreshold = std::make_unique<SettingData>("Grade less than", SettingType::Enum);
    for (const char* str : GRADE_MARK_STR)
        gradeThreshold->enumSetting.options.Add(str);
    gradeThreshold->enumSetting.val = static_cast<int>(GradeMark::PUC);
    gradeThreshold->getter.AddLambda([this](SettingData& data) {
        if (auto* cond = dynamic_cast<GameFailCondition::Grade*>(m_playOptions.failCondition.get()))
        {
            data.enumSetting.val = static_cast<int>(cond->GetMinAllowed());
        }
    });
    gradeThreshold->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::Grade>(static_cast<GradeMark>(data.enumSetting.val));
    });
    conditionTab->settings.emplace_back(std::move(gradeThreshold));

    Setting missThreshold = std::make_unique<SettingData>("Miss count more than", SettingType::Integer);
    missThreshold->intSetting.min = 0;
    missThreshold->intSetting.max = 100;
    missThreshold->intSetting.val = 0;
    missThreshold->getter.AddLambda([this](SettingData& data) {
        if (auto* cond = dynamic_cast<GameFailCondition::MissCount*>(m_playOptions.failCondition.get()))
            data.intSetting.val = cond->GetMaxAllowed();
    });
    missThreshold->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::MissCount>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(missThreshold));

    Setting missNearThreshold = std::make_unique<SettingData>("Miss+Near count more than", SettingType::Integer);
    missNearThreshold->intSetting.min = 0;
    missNearThreshold->intSetting.max = 100;
    missNearThreshold->intSetting.val = 0;
    missNearThreshold->getter.AddLambda([this](SettingData& data) {
        if (auto* cond = dynamic_cast<GameFailCondition::MissAndNearCount*>(m_playOptions.failCondition.get()))
            data.intSetting.val = cond->GetMaxAllowed();
    });
    missNearThreshold->setter.AddLambda([this](const SettingData& data) {
        m_playOptions.failCondition = std::make_unique<GameFailCondition::MissAndNearCount>(data.intSetting.val);
    });
    conditionTab->settings.emplace_back(std::move(missNearThreshold));

    return conditionTab;
}

PracticeModeSettingsDialog::Tab PracticeModeSettingsDialog::m_CreateGameSettingTab()
{
    Tab gameSettingTab = std::make_unique<TabData>();
    gameSettingTab->name = "GameSetting";

    Setting globalOffsetSetting = CreateIntSetting(GameConfigKeys::GlobalOffset, "Global offset", { -200, 200 });
    globalOffsetSetting->setter.AddLambda([this](const SettingData&) { onSettingChange.Call(); });
    gameSettingTab->settings.emplace_back(std::move(globalOffsetSetting));

    Setting chartOffsetSetting = std::make_unique<SettingData>("Chart offset", SettingType::Integer);
    chartOffsetSetting->intSetting.min = -200;
    chartOffsetSetting->intSetting.max = 200;
    chartOffsetSetting->intSetting.val = 0;
    // TODO: get and set the chart offset
    gameSettingTab->settings.emplace_back(std::move(chartOffsetSetting));

    // TODO: speed mod
    // TODO: hidden and sudden

    return gameSettingTab;
}

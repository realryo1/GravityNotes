#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include "framework/nlohmann/json.hpp"

enum class ScoreType
{
	Enemy,
	Obstacle,
	Gravity,
	Jump
};

enum class ScoreWall
{
	Up,
	Down,
	Left,
	Right
};

inline std::string NormalizeScoreToken(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

inline ScoreType ParseScoreType(const std::string& value)
{
	const std::string token = NormalizeScoreToken(value);
	if (token == "enemy") return ScoreType::Enemy;
	if (token == "obstacle") return ScoreType::Obstacle;
	if (token == "gravity") return ScoreType::Gravity;
	if (token == "jump") return ScoreType::Jump;
	throw std::runtime_error("Invalid score type: " + value);
}

inline ScoreWall ParseScoreWall(const std::string& value)
{
	const std::string token = NormalizeScoreToken(value);
	if (token == "up") return ScoreWall::Up;
	if (token == "down") return ScoreWall::Down;
	if (token == "left") return ScoreWall::Left;
	if (token == "right") return ScoreWall::Right;
	throw std::runtime_error("Invalid score wall: " + value);
}

// スコアイベントの構造体
struct ScoreEvent
{
	float beat;
	int lane;
	ScoreType type;
	ScoreWall wall;
};

// スコアデータの構造体
struct ScoreData
{
	float bpm;
	std::string music;
	std::vector<ScoreEvent> events;
};

// JSONファイルからスコアデータを読み込む
inline ScoreData LoadScore(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + filePath);
	}

	nlohmann::json jsonData;
	file >> jsonData;
	file.close();

	ScoreData scoreData;
	scoreData.bpm = jsonData["bpm"].get<float>();
	scoreData.music = jsonData["music"].get<std::string>();

	// イベント配列をパース
	if (jsonData.contains("events"))
	{
		for (const auto& event : jsonData["events"])
		{
			ScoreEvent scoreEvent;
			scoreEvent.beat = event["beat"].get<float>();
			scoreEvent.lane = event["lane"].get<int>();
			scoreEvent.type = ParseScoreType(event["type"].get<std::string>());
			scoreEvent.wall = ParseScoreWall(event["wall"].get<std::string>());

			scoreData.events.push_back(scoreEvent);
		}
	}

	return scoreData;
}
